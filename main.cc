#include <memory>

#include "component/container.h"
#include "process/configcenter_client.h"
#include "process/manager.h"
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fmt/core.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

constexpr const char *kPidFile = "watchermen.pid"; // another option is /var/run/
#define VERSION "0.5"

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif
#ifdef USE_DEBUG
#define BUILD_TYPE "debug version"
#else
#define BUILD_TYPE "release version"
#endif

// Define the flags
ABSL_FLAG(std::string, c, "", "Path to the configuration file");
ABSL_FLAG(std::string, e, "", "Execute command");
ABSL_FLAG(std::string, n, "", "Network Connection");
ABSL_FLAG(bool, v, false, "Show version");

static int CreatePidFile(const char *pid_file) {
  // this pid_fd will be closed automatically when the process exits, i.e. current process shall hold the pid_fd as lock
  auto pid_fd = open(pid_file, O_CREAT | O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (pid_fd == -1) {
    fmt::println("could not open pid file {}", pid_file);
    return -1;
  }

  // Attempt to acquire an exclusive lock
  if (flock(pid_fd, LOCK_EX | LOCK_NB) < 0) {
    if (errno == EWOULDBLOCK) {
      fmt::println("pid file '{}' is already locked by another process.", pid_file);
    } else {
      fmt::println("failed to lock pid file '{}', errno={}, message={}", pid_file, errno, strerror(errno));
    }
    close(pid_fd);
    return -1;
  }

  std::array<char, 64> buf = {0};
  // 清空内容
  if (ftruncate(pid_fd, 0) == -1) {
    fmt::println("fail to truncate pid file {}", pid_file);
    close(pid_fd);
    return -1;
  }
  // 写入进程pid
  auto ret = snprintf(buf.data(), buf.size(), "%ld\n", (long)getpid());
  if (write(pid_fd, buf.data(), ret) != ret) {
    fmt::println("writing to PID file '{}' failed.", pid_file);
    return -1;
  }
  return 0;
}

/**
 * This function shall be treated as part of main.
 * It will terminate the process due to failure.
 */
static void Daemonize() {
  // use double fork
  pid_t pid = fork();
  if (pid < 0) {
    fmt::println("Failed to fork, errno={}, message={}", errno, strerror(errno));
    exit(-1);
  }

  // parent process exit
  if (pid > 0) exit(0);

  if (setsid() < 0) {
    fmt::println("Failed to create a new session, errno={}, message={}", errno, strerror(errno));
    exit(-1);
  }

  pid = fork();
  if (pid < 0) {
    fmt::println("Failed to fork again, errno={}, message={}", errno, strerror(errno));
    exit(-1);
  }

  if (pid > 0) exit(0);

  umask(0);

  // Create PID file
  if (CreatePidFile(kPidFile) != 0) {
    fmt::println("Fail to create PID file, exit.");
    exit(-1);
  }

  // 重定向 stdout到 ./stdout.log
  if (freopen("./stdout.log", "ae", stdout) == nullptr) {
    fmt::println("Failed to redirect stdout, errno={}, message={}", errno, strerror(errno));
    exit(-1);
  }

  // 重定向 stderr到 ./stderr.log
  if (freopen("./stderr.log", "ae", stderr) == nullptr) {
    fmt::println("Failed to redirect stderr, errno={}, message={}", errno, strerror(errno));
    exit(-1);
  }

  // 切换到根目录
  if (chdir("/") < 0) {
    fmt::println("Failed to change directory to root, errno={}, message={}", errno, strerror(errno));
    exit(-1);
  }

  // 将 stdin 关联到 /dev/null
  int null_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
  if (null_fd < 0) {
    fmt::println("Failed to open /dev/null, errno={}, message={}", errno, strerror(errno));
    exit(-1);
  }

  if (dup2(null_fd, STDIN_FILENO) < 0) {
    fmt::println("Failed to redirect stdin to /dev/null, errno={}, message={}", errno, strerror(errno));
    close(null_fd);
    exit(-1);
  }

  close(null_fd);
}

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_v)) {
    fmt::println("version: {}, build: {}, {}", VERSION, GIT_HASH, BUILD_TYPE);
    return 0;
  }
  std::string execute_cmd = absl::GetFlag(FLAGS_e);
  if (!execute_cmd.empty()) {
    fmt::println("Execute command: {}", execute_cmd);
    return 0;
  }

  std::string config_file = absl::GetFlag(FLAGS_c);
  if (config_file.empty()) {
    fmt::println("The configuration file cannot be empty");
    return -1;
  }

  std::shared_ptr<App::Process::Config> manager_config = std::make_shared<App::Process::Config>(config_file);
  auto &config = manager_config->GetConfig();

  if (config.daemon()) {
    Daemonize();
  } else {
    if (CreatePidFile(kPidFile) != 0) {
      fmt::println("Fail to create PID file, exit.");
      exit(-1);
    }
  }
  std::shared_ptr<App::Process::Manager> manager = std::make_shared<App::Process::Manager>(manager_config);

  std::unique_ptr<Core::Component::Container> container = std::make_unique<Core::Component::Container>();
  container->bind(manager->name(), {manager_config, manager});

  std::string is_connect_network = absl::GetFlag(FLAGS_n);
  std::unique_ptr<Core::Component::Discovery::ConfigClient> center_client;
  if (is_connect_network != "no") {
      center_client = std::make_unique<Core::Component::Discovery::ConfigClient>(nullptr, manager_config.get(), manager.get(),
                                                                               manager->getLoop());
      center_client->Start();
  }

  manager->start();

  return 0;
}