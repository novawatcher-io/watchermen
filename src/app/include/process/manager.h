#pragma once
#include <memory>

#include "config.h"
#include "component/discovery/component.h"
#include "process.h"
#include "http/http_manager.h"
#include "component/process/manager.h"

namespace App {
namespace Process {
using namespace Core;
class Manager :public Core::Component::Process::Manager {
public:
    explicit Manager(std::shared_ptr<App::Process::Config> config) : config_(std::move(config)) {

    }


    /**
     * 启动整个manager
     */
    void start() final;

    /**
     * 停止整个manager
     */
    void stop() final;

    static void onStop(evutil_socket_t /*sig*/, short /*events*/, void *param) {
        if (!param) {
            return;
        }
        auto manager = static_cast<Manager*>(param);
        manager->stop();
    }

    void onCreate() {};
    void onStart() {};
    void onDestroy() {};
    void onReload() {};
    void onExit() {};



    void finish() final {

    }

    std::string name() final {
        return "manager";
    }

    void startProcess(const std::string& name);

    ~Manager() {}
private:
    // 启动进程池
    void startProcessPool();
    //卸载http服务
    void unInstallHttpServer();
    // 安装http服务
    void setupHttpServer();
    // 停止部分进程
    void destroyPartProcess(const std::map<std::string, ProcessConfig>& processConfMap);
    // 启动部分进程
    void startPartProcess(const std::map<std::string, ProcessConfig>& processConfMap);
    friend class Config;
    std::shared_ptr<App::Process::Config> config_;
    std::shared_ptr<Core::Http::HttpManager> httpManager_;
    std::shared_ptr<Core::Component::Discovery::Component> discovery;
};
}
}
