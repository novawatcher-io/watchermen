#include "process/process_http_helper.h"

#include <nlohmann/json.hpp>

#include "http/http_request.h"
#include "http/http_action.h"
#include "http/http_manager.h"
#include "process/manager.h"
#include "http/http_router.h"
namespace App {
namespace Process {
void ProcessHttpHelper::bind() {
    std::shared_ptr<Core::Http::HttpAction> action = std::make_shared<Core::Http::HttpAction>();

    //绑定action
    action->setUsers(std::bind(&ProcessHttpHelper::handle, shared_from_this(), _1, _2));

    //注入路由
    manager_->getRouter()->getRequest(path, action);
}

void ProcessHttpHelper::handle(Core::Http::HttpRequest &/*request*/, Core::Http::HttpResponse &response) {
    response.header("Content-Type", "application/json;charset=utf-8");
    nlohmann::json j;
    nlohmann::json processList;
    if (processManager) {
        auto& list = processManager->all();
        for (auto iter = list.begin(); iter != list.end(); iter++) {
            processList.push_back({
                {"name", iter->second->name()},
                {"pid", iter->second->getPid()},
                {"status", iter->second->getStatus()}
            });
        }
    }
    j.push_back({
            {"process", processList},
            {"status", {
                                {"UNKNOWN", Core::Component::Process::UNKNOWN},
                                {"RUN", Core::Component::Process::RUN},
                                {"RUNNING", Core::Component::Process::RUNNING},
                                {"STOPPED", Core::Component::Process::STOPPED},
                                {"STOPPING", Core::Component::Process::STOPPING},
                                {"RELOAD", Core::Component::Process::RELOAD},
                                {"RELOADING", Core::Component::Process::RELOADING},
                                {"EXITED", Core::Component::Process::EXITED},
                                {"DELETING", Core::Component::Process::DELETING},
                                {"DELETED", Core::Component::Process::DELETED},

                        }}
    });
    response.response(200, j.dump());
}

}
}