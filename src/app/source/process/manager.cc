/**
******************************************************************************
* @file           : manager.cc
* @author         : zhanglei
* @brief          : None
* @attention      : None
* @date           : 2024/2/11
******************************************************************************
*/
//
// Created by zhanglei on 2024/2/11.
//
#include "process/manager.h"

#include "component/process/process.h"
#include "process/health_check.h"
#include "http/http_manager.h"
#include "process/process_http_helper.h"

namespace App {
namespace Process {
void Manager::start() {
    // 设置信号集
    sigset->remove(SIGTERM);
    sigset->remove(SIGINT);
    sigset->remove(SIGQUIT);
    sigset->remove(SIGUSR1);
    sigset->remove(SIGUSR2);
    loop->sigAdd(SIGTERM, onStop, this);

    // start process pool
    startProcessPool();

    // 开启配置发现
    discovery = std::make_shared<Core::Component::Discovery::Component>(config_->Path());
    discovery->setListener(config_);
    discovery->start();
    auto channel = discovery->channel(loop);
    loop->updateChannel(channel);
    channel->enableReading(-1);
    config_->BindProcessManager(this);

    // 建立http服务
    setupHttpServer();


    // 建立配置中心 reload 配置
    Core::Component::Process::Manager::start();
}

void Manager::setupHttpServer() {
    auto httpConfig = std::make_shared<Core::Http::HttpConfig>();
    httpManager_ = std::make_shared<Http::HttpManager>(loop, nullptr, httpConfig);
    auto healthCheck = std::make_shared<App::Process::HealthCheck>(httpManager_, config_->GetConfig().http_server().health_config());
    healthCheck->bind();
    auto processHelper = std::make_shared<App::Process::ProcessHttpHelper>(httpManager_, this);
    processHelper->bind();
    httpManager_->init();
    httpManager_->start();
}

void Manager::unInstallHttpServer() {
    httpManager_->stop();
}

void Manager::stop()  {
    // 设置信号集
    httpManager_->stop();
    loop->quit();
    // 停止config watcher
    if (discovery) {
        discovery->stop();
    }
    // 批量停止进程
    Core::Component::Process::Manager::stop();
}

void Manager::startProcessPool() {
    // start process
    std::shared_ptr<OS::CGroup> cgroup;
    if (config_->GetConfig().cgroup().enabled() && !config_->GetConfig().cgroup().name().empty()) {
        cgroup = std::make_shared<OS::CGroup>(config_->GetConfig().cgroup().name());
        cgroup->setCpuRate(config_->GetConfig().cgroup().cpu());
        cgroup->setMemoryLimit(config_->GetConfig().cgroup().memory());
        cgroup->run();
    }
    if (config_->GetConfig().service_size() > 0) {
        auto iter = config_->GetConfig().service().begin();
        for (; iter != config_->GetConfig().service().end(); iter++) {
            auto process = std::make_unique<App::Process::Process>(iter->command(), loop);
            if (iter->cgroup().enabled()) {
                std::string cgroupName = config_->GetConfig().cgroup().name();
                if (cgroupName.empty()) {
                    cgroupName = iter->process_name();
                }
                auto processCGroup = std::make_shared<OS::CGroup>(cgroupName);
                processCGroup->setMemoryLimit(iter->cgroup().memory());
                processCGroup->setCpuRate(iter->cgroup().cpu());
                process->setCGroup(processCGroup);
            } else {
                process->setCGroup(cgroup);
            }
            process->setName(iter->process_name());
            process->execute();
            Core::Component::Process::Manager::addProcess(std::move(process));
        }
    }
}

void Manager::destroyPartProcess(const std::map<std::string, ProcessConfig> &processConfMap) {
    auto& processes = this->all();
    if (processes.empty()) {
        return;
    }
    std::map<std::string, Core::Component::Process::Process*> processMap;
    for (auto iter = processes.begin(); iter != processes.end(); iter++) {
        processMap[iter->second->name()] = iter->second.get();;
    }

    for (auto iter = processConfMap.begin(); iter != processConfMap.end(); iter++) {
        auto processIter = processMap.find(iter->first);
        if (processIter == processMap.end()) {
            continue;
        }
        processIter->second->remove();
    }
}

void Manager::startPartProcess(const std::map<std::string, ProcessConfig> &processConfMap) {
// start process
    std::shared_ptr<OS::CGroup> cgroup;
    if (config_->GetConfig().cgroup().enabled() && !config_->GetConfig().cgroup().name().empty()) {
        cgroup = std::make_shared<OS::CGroup>(config_->GetConfig().cgroup().name());
        cgroup->setCpuRate(config_->GetConfig().cgroup().cpu());
        cgroup->setMemoryLimit(config_->GetConfig().cgroup().memory());
        cgroup->run();
    }
    if (!processConfMap.empty()) {
        auto iter = processConfMap.begin();
        for (; iter != processConfMap.end(); iter++) {
            auto process = std::make_unique<App::Process::Process>(iter->second.command(), loop);
            if (iter->second.cgroup().enabled()) {
                std::string cgroupName = config_->GetConfig().cgroup().name();
                if (cgroupName.empty()) {
                    cgroupName = iter->second.process_name();
                }
                auto processCGroup = std::make_shared<OS::CGroup>(cgroupName);
                processCGroup->setMemoryLimit(iter->second.cgroup().memory());
                processCGroup->setCpuRate(iter->second.cgroup().cpu());
                processCGroup->run();
            } else {
                process->setCGroup(cgroup);
            }
            process->setName(iter->second.process_name());
            process->execute();
            Core::Component::Process::Manager::addProcess(std::move(process));
        }
    }
}

void Manager::startProcess(const std::string &name) {
  // start process
  std::shared_ptr<OS::CGroup> cgroup;
  if (config_->GetConfig().cgroup().enabled() && !config_->GetConfig().cgroup().name().empty()) {
    cgroup = std::make_shared<OS::CGroup>(config_->GetConfig().cgroup().name());
    cgroup->setCpuRate(config_->GetConfig().cgroup().cpu());
    cgroup->setMemoryLimit(config_->GetConfig().cgroup().memory());
    cgroup->run();
  }

  if (config_->GetConfig().service_size() > 0) {
    for (auto iter = config_->GetConfig().service().begin(); iter != config_->GetConfig().service().end(); iter++) {
      if (iter->process_name() != name) continue;
      auto process = std::make_unique<App::Process::Process>(iter->command(), loop);
      if (iter->cgroup().enabled()) {
        std::string cgroupName = config_->GetConfig().cgroup().name();
        if (cgroupName.empty()) {
          cgroupName = iter->process_name();
        }
        auto processCGroup = std::make_shared<OS::CGroup>(cgroupName);
        processCGroup->setMemoryLimit(iter->cgroup().memory());
        processCGroup->setCpuRate(iter->cgroup().cpu());
        process->setCGroup(processCGroup);
      } else {
        process->setCGroup(cgroup);
      }
      process->setName(iter->process_name());
      process->execute();
      Core::Component::Process::Manager::addProcess(std::move(process));
    }
  }
}
}
}
