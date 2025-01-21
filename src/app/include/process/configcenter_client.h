#pragma once
#include "component/timer_channel.h"
#include "generated/grpc/agent/v1/controller.grpc.pb.h"
#include "generated/grpc/agent/v1/controller.pb.h"
#include "process/async_queue.h"
#include "process/config.h"
#include "process/manager.h"
#include <grpcpp/alarm.h>
#include <memory>
#include <string>
#include <utility>

namespace Core::Component::Discovery {
/**
 * This onSuccess will be passed to ConfigClient, client will trigger
 * corresponding function when event occur
 */
struct ConfigureCallback {
  virtual void OnNewConfig(const std::string &config_id, const std::string &config) = 0;
  // todo: need update
  virtual void OnServerCommand() {};
  virtual void OnRegistered() {}
  virtual void OnUnregistered() {}
  virtual ~ConfigureCallback() = default;
};

class ConfigClient {
public:
  explicit ConfigClient(ConfigureCallback *callback, App::Process::Config *config_listener,
                        App::Process::Manager *manager, Core::Event::EventLoop *loop);
  void Start();

private:
  void AgentUnregisterAsync();
  void AgentHeartbeatAsync();
  void AgentOperateAsync();
  void AgentRegisterAsync();
  void AgentGetConfigAsync();

  void OnRegisterResponse(const grpc::Status &s, const agent::AgentRegisterRes &reply);
  void OnGetConfigResponse(const grpc::Status &s, const agent::AgentGetConfigRes &response);
  void OnUnregisterResponse(const grpc::Status &s, const agent::AgentUnregisterRes &response);
  void OnHeartbeatResponse(const grpc::Status &s, const agent::AgentHeartbeatRes &response);
  void OnServerOperate(const agent::AgentOperateRes &cmd);
  void OnHealthCheck();

  void SetupHeartbeat();

private:
  void Connect(bool keepalive);

private:
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<agent::AgentControllerService::Stub> stub_;
  std::string server_address_;
  std::string config_uuid_;
  std::string config_;
  std::string hostname_;
  std::string ipv4_;
  std::string ipv6_;
  ConfigureCallback *callback_ = nullptr;
  App::Process::Config *config_listener_;
  App::Process::Manager *manager_;
  Core::Event::EventLoop *loop_;
  std::unique_ptr<Core::Component::TimerChannel> heartbeat_timer_;
  std::unique_ptr<Core::Component::TimerChannel> register_timer_;
  std::unique_ptr<Core::Component::TimerChannel> health_check_timer_;
  Core::Event::AsyncQueue async_queue_;
  int heartbeat_fail_cnt_ = 0;
  int last_timeout_ = 0;
  uint64_t object_id_ = 0;
};
} // namespace Core::Component::Discovery
