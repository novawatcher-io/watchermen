#include "process/health_check.h"
#include "http/http_request.h"
#include "http/http_action.h"
#include "http/http_manager.h"
#include "http/http_router.h"

namespace App {
namespace Process {

void HealthCheck::bind() {

    std::shared_ptr<Core::Http::HttpAction> action = std::make_shared<Core::Http::HttpAction>();

    //绑定action
    action->setUsers(std::bind(&HealthCheck::handle, shared_from_this(), _1, _2));

    //注入路由
    manager_->getRouter()->getRequest(path, action);
}

void HealthCheck::handle(Core::Http::HttpRequest & /*request*/, Core::Http::HttpResponse &response) {
    response.header("Content-Type", "application/json;charset=utf-8");
    response.response(200, R"({"status":"UP"})");
}

}
}
