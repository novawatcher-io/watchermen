#include "process/configcenter_client.h"
#include "component/process/process.h"
#include "generated/grpc/agent/v1/controller.grpc.pb.h"
#include "generated/grpc/agent/v1/controller.pb.h"
#include <any>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/client_callback.h>
#include <os/unix_util.h>
#include <spdlog/spdlog.h>
#include <thread>

using agent::AgentCmd;
using agent::AgentControllerService;
using agent::AgentGetConfigReq;
using agent::AgentGetConfigRes;
using agent::AgentHeartbeatReq;
using agent::AgentHeartbeatRes;
using agent::AgentOperateReq;
using agent::AgentOperateRes;
using agent::AgentProcessInfo;
using agent::AgentRegisterReq;
using agent::AgentRegisterRes;
using agent::AgentUnregisterReq;
using agent::AgentUnregisterRes;
using agent::ProcessInfo;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using std::placeholders::_1;
using std::placeholders::_2;

namespace Core::Component::Discovery {

// 心跳周期
constexpr int kHeartbeatPeriodInSeconds = 5 * 60;
constexpr int kGRPCKeepAliveInSeconds = 60;
constexpr int kGRPCTimeoutCallInSeconds = 10;
constexpr int kHealthCheckInSeconds = 30;

/**
 * 退避策略: 每次的延迟时间比上次多5-10秒, 最多加到30秒.
 * @param last 上一次的延迟时间
 * @return value in range [5,30]
 */
static int GetRandomTimeout(int last) {
  if (last < 30) {
    int timeout = rand() % 5 + 5 + last;
    if (timeout > 30) {
      timeout = 30;
    }
    last = timeout;
  } else if (last > 30) {
    return 30;
  }
  return last;
}

static std::string GetHostname() {
  char hostname[256] = {0};
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    return hostname;
  }
  return "";
}

/**
 * Client-> Server的单向的请求
 * @tparam RequestType
 * @tparam ResponseType
 */
template <typename RequestType, typename ResponseType> //
struct AsyncUnaryCall : public grpc::ClientUnaryReactor {
  grpc::Status status{};
  ClientContext context{};
  RequestType request{};
  ResponseType response{};
  std::function<void(const grpc::Status &, const ResponseType &)> callback;
  explicit AsyncUnaryCall(const std::string &company_uuid) {
    context.AddMetadata("company_uuid", company_uuid);
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(kGRPCTimeoutCallInSeconds));
  }

  void OnDone(const grpc::Status &s) override {
    if (callback) callback(s, response);
  }
};

/**
 * 服务端返回类型为stream的调用模型.
 * @tparam RequestType
 * @tparam ResponseType
 */
template <typename RequestType, typename ResponseType>
struct AsyncServerStreamingCall : public grpc::ClientReadReactor<ResponseType> {
  grpc::Status status{};
  ClientContext context{};
  RequestType request{};
  ResponseType response{};
  std::function<void(const ResponseType &)> callback;
  std::function<void()> error_cb;
  explicit AsyncServerStreamingCall(const std::string &company_uuid) {
    context.AddMetadata("company_uuid", company_uuid);
    SPDLOG_INFO("create async stream call, addr={}", (void *)this);
  }
  void OnDone(const grpc::Status &s) override {
    // todo: figure out a better solution
    if (s.ok()) {
      SPDLOG_INFO("server shutdown channel");
    } else {
      SPDLOG_INFO("async stream call error: code={}, message={}, addr={}", static_cast<int>(s.error_code()),
                  s.error_message(), (void *)this);
      if (error_cb) error_cb();
    }
  }
  void OnReadDone(bool ok) override {
    if (!ok) {
      SPDLOG_WARN("async read steam failed");
      if (error_cb) error_cb();
      return;
    }
    if (callback) callback(response);
    // continue to read next message
    this->StartRead(&response);
  }
};

void ConfigClient::OnRegisterResponse(const grpc::Status &s, const agent::AgentRegisterRes &reply) {
  async_queue_.Push([&, s, reply]() {
    if (s.ok()) {
      SPDLOG_INFO("register response: {}", reply.ShortDebugString());
      if (callback_) callback_->OnRegistered();

      // clear timeout value
      last_timeout_ = 0;

      if (!reply.configuuid().empty() && reply.configuuid() != config_uuid_) {
        config_uuid_ = reply.configuuid();
        AgentGetConfigAsync();
      }

      /* 已经注册成功, 无需再次尝试 */
      if (register_timer_->enabled()) {
        register_timer_->disable();
      }

      /* 注册成功之后重启心跳 */
      AgentHeartbeatAsync();

      /* 订阅服务端的操作流 */
      AgentOperateAsync();
    } else {
      last_timeout_ = GetRandomTimeout(last_timeout_);
      SPDLOG_INFO("register failed, code: {}, message: {}, retry after: {}", static_cast<int>(s.error_code()),
                  s.error_message(), last_timeout_);
      register_timer_->enable(std::chrono::seconds(last_timeout_));
      // to cancel timer set in constructor
      if (heartbeat_timer_->enabled()) {
        heartbeat_timer_->disable();
      }
    }
  });
}

void ConfigClient::AgentRegisterAsync() {
  auto &local_config = config_listener_->GetConfig();
  auto *call = new AsyncUnaryCall<AgentRegisterReq, AgentRegisterRes>(local_config.company_uuid());
  call->callback = std::bind(&ConfigClient::OnRegisterResponse, this, _1, _2);
  call->request.set_name(hostname_);
  call->request.set_version(local_config.version());
  call->request.set_objectid(object_id_);
  call->request.set_ipv4(ipv4_);
  call->request.set_ipv6(ipv6_);
  SPDLOG_INFO("AgentRegisterReq: {}", call->request.ShortDebugString());
  stub_->async()->AgentRegister(&call->context, &call->request, &call->response, call);
  call->StartCall();

  if (heartbeat_timer_->enabled()) {
    heartbeat_timer_->disable();
  }
}

void ConfigClient::OnGetConfigResponse(const grpc::Status &s, const agent::AgentGetConfigRes &response) {
  async_queue_.Push([&, s, response]() {
    if (!s.ok()) {
      SPDLOG_ERROR("get config failed: code={}, message={}", static_cast<int>(s.error_code()), s.error_message());
      return;
    }

    SPDLOG_INFO("get config response: {}", response.ShortDebugString());
    if (response.content().empty()) {
      SPDLOG_INFO("empty config, use local");
    } else if (config_ != response.content()) {
      SPDLOG_INFO("update config.");
      config_ = response.content();
      if (callback_) callback_->OnNewConfig(config_uuid_, config_);
      if (config_listener_) {
        config_listener_->OnServerConfig(config_);
        // check new address
        auto server = config_listener_->GetConfig().network();
        auto new_address = fmt::format("{}:{}", server.host(), server.port());
        if (server_address_ != new_address) {
          server_address_ = new_address;
          Connect(false);
          AgentRegisterAsync();
        }
      }
    } else {
      SPDLOG_INFO("config not changed, ignore");
    }
  });
}

void ConfigClient::AgentGetConfigAsync() {
  auto &local_config = config_listener_->GetConfig();
  auto *call = new AsyncUnaryCall<AgentGetConfigReq, AgentGetConfigRes>(local_config.company_uuid());
  call->request.set_configuuid(config_uuid_);
  call->callback = std::bind(&ConfigClient::OnGetConfigResponse, this, _1, _2);
  stub_->async()->AgentGetConfig(&call->context, &call->request, &call->response, call);
  call->StartCall();
}

void ConfigClient::OnUnregisterResponse(const grpc::Status &s, const agent::AgentUnregisterRes &) {
  async_queue_.Push([&, s]() {
    if (s.ok()) {
      SPDLOG_INFO("unregister success");
      if (callback_) callback_->OnUnregistered();
    } else {
      SPDLOG_ERROR("unregister failed");
    }
  });
}

void ConfigClient::AgentUnregisterAsync() {
  auto &local_config = config_listener_->GetConfig();
  auto *call = new AsyncUnaryCall<AgentUnregisterReq, AgentUnregisterRes>(local_config.company_uuid());
  AgentUnregisterReq &request = call->request;
  request.set_objectid(object_id_);
  SPDLOG_INFO("AgentUnregisterReq request={}", request.ShortDebugString());
  call->callback = std::bind(&ConfigClient::OnUnregisterResponse, this, _1, _2);

  stub_->async()->AgentUnregister(&call->context, &call->request, &call->response, call);
  call->StartCall();
}

void ConfigClient::SetupHeartbeat() {
  if (heartbeat_timer_->enabled()) {
    heartbeat_timer_->disable();
  }
  heartbeat_timer_->enable(std::chrono::seconds(kHeartbeatPeriodInSeconds));
}

void ConfigClient::OnHeartbeatResponse(const grpc::Status &s, const agent::AgentHeartbeatRes &response) {
  async_queue_.Push([&, s, response]() {
    if (s.ok()) {
      SPDLOG_INFO("heartbeat response: {}", response.configuuid());
      if (!response.configuuid().empty() && config_uuid_ != response.configuuid()) {
        config_uuid_ = response.configuuid();
        AgentGetConfigAsync();
      }
    } else {
      SPDLOG_WARN("heartbeat failed, error code={}, error message={}", static_cast<int>(s.error_code()),
                  s.error_message());
      heartbeat_fail_cnt_++;
      if (heartbeat_fail_cnt_ > 5) {
        SPDLOG_INFO("heartbeat failed for: {} times", heartbeat_fail_cnt_);
        heartbeat_timer_->disable();
        /* 重启注册流程 */
        AgentRegisterAsync();
        heartbeat_fail_cnt_ = 0;
        return;
      }
    }
    SetupHeartbeat();
  });
}

void ConfigClient::AgentHeartbeatAsync() {
  auto &local_config = config_listener_->GetConfig();
  auto *call = new AsyncUnaryCall<AgentHeartbeatReq, AgentHeartbeatRes>(local_config.company_uuid());
  call->request.set_configuuid(config_uuid_);
  call->request.set_objectid(object_id_);
  call->request.set_name(hostname_);
  call->request.set_version(local_config.version());
  if (manager_) {
    auto agent_info = new AgentProcessInfo;
    for (auto &[pid, p] : manager_->all()) {
      auto process = agent_info->add_processlist();
      process->set_name(p->name());
      switch (p->getStatus()) {
      case Process::RELOAD:
      case Process::RELOADING:
      case Process::RUN:
      case Process::RUNNING:
        process->set_state(::agent::ProcessState::Running);
        break;
      case Process::DELETED:
      case Process::DELETING:
      case Process::EXITED:
      case Process::STOPPED:
      case Process::STOPPING:
        process->set_state(::agent::ProcessState::Stopped);
        break;
      case Process::UNKNOWN:
        SPDLOG_WARN("unknown process state, skip");
        break;
      }
      // process->set_version() todo: ??
      auto start_time = process->mutable_starttime();
      start_time->set_seconds(p->getStartTime());
    }
    call->request.set_allocated_agentprocessinfo(agent_info);
  }
  call->callback = std::bind(&ConfigClient::OnHeartbeatResponse, this, _1, _2);

  SPDLOG_INFO("AgentHeartbeatReq request={}", call->request.ShortDebugString());

  stub_->async()->AgentHeartbeat(&call->context, &call->request, &call->response, call);
  call->StartCall();

  SetupHeartbeat();
}

void ConfigClient::OnServerOperate(const agent::AgentOperateRes &cmd) {
  async_queue_.Push([&, cmd]() {
    SPDLOG_INFO("server command: {}", cmd.ShortDebugString());
    if (callback_) callback_->OnServerCommand();
    for (auto &name : cmd.names()) {
      if (cmd.cmd() == AgentCmd::Start) {
        manager_->startProcess(name);
      } else if (cmd.cmd() == AgentCmd::Stop) {
        manager_->stopProcess(name);
      }
    }
    AgentHeartbeatAsync();
  });
}

void ConfigClient::AgentOperateAsync() {
  auto &local_config = config_listener_->GetConfig();
  auto call = new AsyncServerStreamingCall<AgentOperateReq, AgentOperateRes>(local_config.company_uuid());
  call->request.set_objectid(object_id_);
  call->callback = std::bind(&ConfigClient::OnServerOperate, this, _1);
  call->error_cb = std::bind(&ConfigClient::AgentRegisterAsync, this); // restart from register
  stub_->async()->AgentOperate(&call->context, &call->request, call);
  call->StartRead(&call->response);
  call->StartCall();
}

void ConfigClient::OnHealthCheck() {
  auto state = channel_->GetState(false);
  const char *p = "unknown";
  switch (state) {
  case GRPC_CHANNEL_IDLE:
    p = "idle";
    break;
  case GRPC_CHANNEL_CONNECTING:
    p = "connecting";
    break;
  case GRPC_CHANNEL_READY:
    p = "ready";
    break;
  case GRPC_CHANNEL_TRANSIENT_FAILURE:
    p = "transient failure";
    break;
  case GRPC_CHANNEL_SHUTDOWN:
    p = "shutdown";
    break;
  default:
    break;
  }
  SPDLOG_DEBUG("config center client channel state {}", p);
  health_check_timer_->enable(std::chrono::seconds(kHealthCheckInSeconds));
}

ConfigClient::ConfigClient(ConfigureCallback *callback, App::Process::Config *config_listener,
                           App::Process::Manager *manager, Core::Event::EventLoop *loop)
    : callback_(callback), config_listener_(config_listener), manager_(manager), loop_(loop), async_queue_(loop) {
  auto &network = config_listener_->GetConfig().network();
  if (network.host().empty() || network.port() == 0) {
    SPDLOG_ERROR("invalid server address: {}", network.ShortDebugString());
  } else {
    server_address_ = fmt::format("{}:{}", network.host(), network.port());
  }

  hostname_ = GetHostname();
  register_timer_ =
      std::make_unique<Core::Component::TimerChannel>(loop_, std::bind(&ConfigClient::AgentRegisterAsync, this));
  heartbeat_timer_ =
      std::make_unique<Core::Component::TimerChannel>(loop_, std::bind(&ConfigClient::AgentHeartbeatAsync, this));
  // prevent the loop from exit
  heartbeat_timer_->enable(std::chrono::seconds(kHeartbeatPeriodInSeconds));

  health_check_timer_ =
      std::make_unique<Core::Component::TimerChannel>(loop_, std::bind(&ConfigClient::OnHealthCheck, this));
  health_check_timer_->enable(std::chrono::seconds(kHealthCheckInSeconds));

  object_id_ = OS::getMachineId();
  auto ret = config_listener_->GetIpInfo();
  ipv4_ = ret.ipv4;
  ipv6_ = ret.ipv6;
  SPDLOG_INFO("client info: hostname_={}, machine id={}, ipv4={}, ipv6={}", hostname_, object_id_, ipv4_, ipv6_);
  Connect(false);
}

void ConfigClient::Connect(bool keepalive) {
  if (server_address_.empty()) return;
  SPDLOG_INFO("control center server address: {}", server_address_);

  if (keepalive) {
    grpc::ChannelArguments channel_args;
    channel_args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, kGRPCKeepAliveInSeconds * 1000);
    channel_args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, kGRPCTimeoutCallInSeconds * 1000);
    channel_args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    channel_args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);

    channel_ = grpc::CreateCustomChannel(server_address_, grpc::InsecureChannelCredentials(), channel_args);
  } else {
    channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
  }

  stub_ = AgentControllerService::NewStub(channel_);
  if (!stub_) {
    SPDLOG_ERROR("can not create stub {}", server_address_);
  }
}

void ConfigClient::Start() {
  if (!stub_) {
    SPDLOG_ERROR("client is not correctly initialized");
    return;
  }
  AgentRegisterAsync();
}
} // namespace Core::Component::Discovery
