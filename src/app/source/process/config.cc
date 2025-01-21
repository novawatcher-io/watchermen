#include "process/config.h"
#include "process/manager.h"
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <fstream>
#include <linux/if.h>

using google::protobuf::util::JsonStringToMessage;

namespace App::Process {
static const std::unordered_map<std::string, spdlog::level::level_enum> logLevels = {
    {"trace", spdlog::level::trace}, {"debug", spdlog::level::debug}, {"info", spdlog::level::info},
    {"warn", spdlog::level::warn},   {"error", spdlog::level::err},   {"off", spdlog::level::off}};

static bool IsValidLogLevel(const std::string &level) { return logLevels.find(level) != logLevels.end(); }

static auto GetLogLevel(const std::string &level) {
  auto it = logLevels.find(level);
  if (it != logLevels.end()) {
    return it->second;
  }
  return spdlog::level::info; // default log level
}

void GetHostNetworkCard(std::unordered_map<std::string, IpInfo> &ip_map) {
  struct ifaddrs *interfaces = nullptr;

  // Retrieve the current interfaces
  if (getifaddrs(&interfaces) == -1) {
    SPDLOG_ERROR("Error getting interfaces");
    return;
  }

  auto temp_addr = interfaces;
  while (temp_addr != nullptr) {
    // Exclude loopback and Docker interfaces
    if (strcmp(temp_addr->ifa_name, "lo") == 0 || strncmp(temp_addr->ifa_name, "docker", 6) == 0 ||
        strncmp(temp_addr->ifa_name, "br-", 3) == 0) {
      temp_addr = temp_addr->ifa_next;
      continue;
    }

    // Check if the interface is up
    if (!(temp_addr->ifa_flags & IFF_UP)) {
      temp_addr = temp_addr->ifa_next;
      continue;
    }

    char address[INET6_ADDRSTRLEN];
    memset(address, 0, sizeof(address));
    const char *ip;

    if (temp_addr->ifa_addr->sa_family == AF_INET6) {
      void *addr_ptr = &((struct sockaddr_in6 *)temp_addr->ifa_addr)->sin6_addr;
      ip = inet_ntop(temp_addr->ifa_addr->sa_family, addr_ptr, address, sizeof(address));
      ip_map[temp_addr->ifa_name].ipv6 = ip;
    } else if (temp_addr->ifa_addr->sa_family == AF_INET) {
      void *addr_ptr = &((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr;
      ip = inet_ntop(temp_addr->ifa_addr->sa_family, addr_ptr, address, sizeof(address));
      ip_map[temp_addr->ifa_name].ipv4 = ip;
    }

    temp_addr = temp_addr->ifa_next;
  }
  // Free the memory allocated by getifaddrs
  freeifaddrs(interfaces);
}

IpInfo Config::GetIpInfo() const {
  std::unordered_map<std::string, IpInfo> ip_map;
  GetHostNetworkCard(ip_map);
  for (auto &[name, ip] : ip_map) {
    SPDLOG_INFO("interface: {}, ipv6: {}, ipv4: {}", name, ip.ipv4, ip.ipv6);
  }
  std::string network_interface;
  {
    std::shared_lock<std::shared_mutex> lock(rw_lock_);
    network_interface = config_.network_interface();
  }

  if (!network_interface.empty()) {
    auto it = ip_map.find(network_interface);
    if (it != ip_map.end()) {
      return it->second;
    } else {
      SPDLOG_ERROR("network_interface {} not found, random select one", network_interface);
    }
  }
  IpInfo ret;
  for (auto &[name, ip] : ip_map) {
    if (!ip.ipv4.empty() && !ip.ipv6.empty()) {
      SPDLOG_INFO("use interface: {}, ipv4: {}, ipv6: {}", name, ip.ipv4, ip.ipv6);
      return ip;
    } else if (!ip.ipv6.empty()) {
      ret.ipv6 = ip.ipv6; // random pick one
    } else if (!ip.ipv4.empty()) {
      ret.ipv4 = ip.ipv4; // random pick one
    }
  }
  return ret;
}

static std::string ReadFile(const std::string &path) {
  if (path.empty()) {
    return "";
  }
  std::ifstream istream(path);
  std::stringstream buffer;
  buffer << istream.rdbuf();
  istream.close();
  return buffer.str();
}

static bool WriteFile(const std::string &path, const std::string &content) {
  if (path.empty()) {
    return false;
  }

  std::ofstream file(path);
  if (file) {
    file << content;
    file.close();
    return true;
  }
  return false;
}

std::string GetAbsPath(const std::string &src) {
  std::filesystem::path p(src);
  return std::filesystem::absolute(p).string();
}

Config::Config(const std::string &path) {
  // set all path-related settings to absolute path
  path_ = GetAbsPath(path);

  {
    std::unique_lock<std::shared_mutex> lock(rw_lock_);
    ReadConfig(path_, config_);
  }

  // init logger
  {
    std::shared_lock<std::shared_mutex> lock(rw_lock_);
    UpdateLogPath(config_.daemon(), config_.log_path(), config_.log_level());
  }
}

void Config::UpdateLogPath(bool daemon, const std::string &path, const std::string &level) {
  auto get_syslog_sink = [&]() -> spdlog::sink_ptr {
    if (syslog_sink_) return syslog_sink_;
    std::string ident = fmt::format("watchermen"); // syslog ident
    syslog_sink_ =
        std::make_shared<spdlog::sinks::syslog_sink_mt>(ident, LOG_PID | LOG_CONS | LOG_NDELAY, LOG_LOCAL2, true);
    return syslog_sink_;
  };

  auto get_stdout_sink = [&]() -> spdlog::sink_ptr {
    if (stdout_sink_) return stdout_sink_;
    stdout_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    return stdout_sink_;
  };

  auto get_file_sink = [&](const std::string &path) -> spdlog::sink_ptr {
    if (file_sink_) {
      auto file = std::dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_mt>(file_sink_);
      if (file && file->filename() == path) {
        SPDLOG_INFO("file sink already exists, path={}", path);
        return file_sink_;
      }
      SPDLOG_INFO("log file changed from {} to path={}", file->filename(), path);
    }
    if (path.empty()) return nullptr;
    if (path[0] != '/') {
      SPDLOG_ERROR("log path must be absolute path: {}", path);
      return nullptr;
    }
    spdlog::file_event_handlers handler;
    handler.after_open = [](const spdlog::filename_t &filename, std::FILE *file_stream) {
      int fd = fileno(file_stream);
      if (fd == -1) return;
      if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
        SPDLOG_INFO("Failed to set FD_CLOEXEC to file: {}", filename);
      }
    };
    file_sink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path, 1024 * 1024 * 10, 3, false, handler);
    return file_sink_;
  };

  // default sink
  if (daemon) {
    get_syslog_sink();
  } else {
    get_stdout_sink();
  }

  // output to syslog
  if (!path.empty()) {
    if (path == "syslog") {
      if (!daemon) {
        // allow log to syslog in non-daemon mode
        get_syslog_sink();
      }
    } else if (path != "stdout") {
      get_file_sink(path);
    }
  }

  std::vector<spdlog::sink_ptr> sinks;
  if (file_sink_) sinks.push_back(file_sink_);
  if (stdout_sink_) sinks.push_back(stdout_sink_);
  if (syslog_sink_) sinks.push_back(syslog_sink_);

  if (logger_) {
    logger_->sinks().swap(sinks);
    SPDLOG_INFO("update log sinks");
  } else {
    logger_ = std::make_shared<spdlog::logger>("multi", sinks.begin(), sinks.end());
    logger_->set_level(GetLogLevel(level));
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
    spdlog::register_logger(logger_);
    spdlog::set_default_logger(logger_);
    spdlog::flush_on(spdlog::level::trace);
  }
}

bool Config::ReadConfig(const std::string &file, ManagerConfig &config) {
  auto content = ReadFile(file);
  auto status = JsonStringToMessage(content, &config);
  if (!status.ok()) {
    SPDLOG_ERROR("JsonStringToMessage ({}) error:{}", content, status.message());
    return false;
  }
  return true;
}

void Config::OnServerConfig(const std::string &new_config) {
  if (new_config.empty()) return;
  // check if config is valid
  ManagerConfig temp{};
  auto status = JsonStringToMessage(new_config, &temp);
  if (!status.ok()) {
    SPDLOG_ERROR("JsonStringToMessage failed, new config=({}) error:{}", new_config, status.message());
    return;
  }
  for (auto &process : temp.service()) {
    WriteFile(process.config_path(), process.config());
  }
  // reload config
  ReloadConfig(temp);

  // save to file
  SaveConfig();
}

bool Config::ReloadConfig(ManagerConfig &new_config) {
  std::unique_lock<std::shared_mutex> lock(rw_lock_);
  // following field will not be updated
  //  newConfig.set_company_uuid(config_.company_uuid());
  //  newConfig.set_version(config_.version());
  //  newConfig.set_network_interface(config_.network_interface());
  config_.set_daemon(new_config.daemon());

  // todo: update on the fly
  if (IsValidLogLevel(new_config.log_level()) && new_config.log_level() != config_.log_level()) {
    SPDLOG_INFO("update log level from {} to {}", config_.log_level(), new_config.log_level());
    spdlog::set_level(GetLogLevel(new_config.log_level()));
    config_.set_log_level(new_config.log_level());
  }

  if (!new_config.log_path().empty() && new_config.log_path() != config_.log_path()) {
    config_.set_log_path(new_config.log_path());
    UpdateLogPath(config_.daemon(), config_.log_path(), config_.log_level());
  }

  // cgroup 变了重启整个cgroup
  if (!google::protobuf::util::MessageDifferencer::Equals(config_.cgroup(), new_config.cgroup())) {
    config_.mutable_cgroup()->CopyFrom(new_config.cgroup());
    config_.mutable_service()->CopyFrom(new_config.service());
    for (auto &process : m_->all()) {
      process.second->stop();
    }
    m_->startProcessPool();
  } else {
    // 比较process，重启部分process
    auto diff = DiffProcessPool(config_.service(), new_config.service());
    if (!diff.first.empty() || !diff.second.empty()) {
      config_.mutable_service()->CopyFrom(new_config.service());
      // 停止旧的进程
      if (!diff.second.empty()) {
        m_->destroyPartProcess(diff.second);
      }

      // 启动新的进程
      if (!diff.first.empty()) {
        m_->startPartProcess(diff.first);
      }
    }
  }

  // httpserver 配置是否变化，变化要重启 manager
  if (!google::protobuf::util::MessageDifferencer::Equals(config_.http_server(), new_config.http_server())) {
    config_.mutable_http_server()->CopyFrom(new_config.http_server());
    m_->unInstallHttpServer();
    m_->setupHttpServer();
  }
  return true;
}

void Config::OnLogFileChanged() {
  SPDLOG_INFO("local file changed, reload config");
  ManagerConfig temp{};
  if (!ReadConfig(path_, temp)) {
    SPDLOG_ERROR("load config failed, path={}", path_);
    return;
  }
  ReloadConfig(temp);
}

DiffProcessPoolPair Config::DiffProcessPool(const google::protobuf::RepeatedPtrField<::ProcessConfig> &oldService,
                                            const google::protobuf::RepeatedPtrField<::ProcessConfig> &newService) {
  std::map<std::string, ::ProcessConfig> addProcessMap;
  std::map<std::string, ::ProcessConfig> reduceProcessMap;
  std::map<std::string, ::ProcessConfig> oldProcessMap;
  std::map<std::string, ::ProcessConfig> newProcessMap;
  auto oldProcessMapBegin = oldService.begin();
  auto newProcessMapBegin = newService.begin();
  for (; oldProcessMapBegin != oldService.end(); oldProcessMapBegin++) {
    if (oldProcessMapBegin->process_name().empty()) {
      continue;
    }

    if (oldProcessMapBegin->command().empty()) {
      continue;
    }

    auto name = oldProcessMapBegin->process_name();
    oldProcessMap[name] = *oldProcessMapBegin;
  }

  for (; newProcessMapBegin != newService.end(); newProcessMapBegin++) {
    if (newProcessMapBegin->process_name().empty()) {
      continue;
    }

    if (newProcessMapBegin->command().empty()) {
      continue;
    }

    auto name = newProcessMapBegin->process_name();
    newProcessMap[name] = *newProcessMapBegin;
  }

  if (oldProcessMap.empty()) {
    return {newProcessMap, oldProcessMap};
  }

  if (newProcessMap.empty()) {
    return {oldProcessMap, newProcessMap};
  }

  // 找出新添加的进程
  for (auto iter = newProcessMap.begin(); iter != newProcessMap.end(); iter++) {
    auto newIter = oldProcessMap.find(iter->second.process_name());
    if (newIter == oldProcessMap.end()) {
      addProcessMap[iter->second.process_name()] = iter->second;
      continue;
    }

    // 查一下已经存在的进程是否要reload
    if (!google::protobuf::util::MessageDifferencer::Equals(iter->second, newIter->second)) {
      addProcessMap[newIter->second.process_name()] = newIter->second;
      continue;
    }
  }

  // 找出新减少的进程
  for (auto iter = oldProcessMap.begin(); iter != oldProcessMap.end(); iter++) {
    auto newIter = newProcessMap.find(iter->second.process_name());
    if (newIter == newProcessMap.end()) {
      reduceProcessMap[iter->second.process_name()] = iter->second;
      continue;
    }
  }
  return {addProcessMap, reduceProcessMap};
}

void Config::SaveConfig() {
  std::shared_lock<std::shared_mutex> lock(rw_lock_);
  std::string json_config;
  auto ret = google::protobuf::util::MessageToJsonString(config_, &json_config);
  if (ret.ok()) {
    WriteFile(path_, json_config);
  }
}

} // namespace App::Process
