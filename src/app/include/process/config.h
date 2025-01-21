/**
******************************************************************************
* @file           : config.h
* @author         : zhanglei
* @brief          : None
* @attention      : None
* @date           : 2024/2/9
******************************************************************************
*/
//
// Created by zhanglei on 2024/2/9.
//
#pragma once

#include <memory>
#include <sstream>
#include <utility>

#include "component/api.h"
#include "component/timer_channel.h"
#include "event/event_loop.h"
#include "process.h"
#include "watchermen/v1/manager.pb.h"
#include <shared_mutex>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

namespace App::Process {
struct IpInfo {
  std::string ipv4;
  std::string ipv6;
};

/**
 * diffProcessPoolPair
 * 第一个参数是 新增的process
 * 第二个参数是 减少的process
 */
using DiffProcessPoolPair = std::pair<std::map<std::string, ProcessConfig>, std::map<std::string, ProcessConfig>>;

class TimerGuard;
class Manager;

/**
 * manager的配置，在配置发生变化的时候会重载manager
 */
class Config : public Core::Component::BaseConfig {
public:
  explicit Config(const std::string &);

  void onUpdate() override { OnLogFileChanged(); };
  void onCreate() override { OnLogFileChanged(); };
  void onDelete() override { OnLogFileChanged(); };

  void OnServerConfig(const std::string &new_config);
  std::string Path() const { return path_; }

  const ManagerConfig &GetConfig() { return config_; } // dangerous

  void BindProcessManager(Manager *m) { m_ = m; }
  IpInfo GetIpInfo() const;

  /**
   * 比较 配置service 变化
   * @param oldService 旧的service
   * @param newService 新的service
   * @return
   */
  static DiffProcessPoolPair DiffProcessPool(const google::protobuf::RepeatedPtrField<::ProcessConfig> &oldService,
                                             const google::protobuf::RepeatedPtrField<::ProcessConfig> &newService);

private:
  static bool ReadConfig(const std::string &file, ManagerConfig &config);
  void OnLogFileChanged();
  bool ReloadConfig(ManagerConfig &new_config);
  void SaveConfig();

  void UpdateLogPath(bool daemon, const std::string &path, const std::string &level);

private:
  mutable std::shared_mutex rw_lock_; // 保护config_
  ManagerConfig config_{};
  std::string path_;
  Manager *m_ = nullptr;
  spdlog::sink_ptr stdout_sink_;
  spdlog::sink_ptr file_sink_;
  spdlog::sink_ptr syslog_sink_;
  std::shared_ptr<spdlog::logger> logger_;
};
} // namespace App::Process
