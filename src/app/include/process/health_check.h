#pragma once

#include <memory>

#include "watchermen/v1/manager.pb.h"
#include "http/http_request.h"
#include "http/http_response.h"
#include "http/http_manager.h"
#ifndef HEALTH_CHECK_NAME
#define HEALTH_CHECK_NAME "health"
#endif

using namespace std::placeholders;

namespace App {
namespace Process {

class HealthCheck :public Core::Noncopyable, public std::enable_shared_from_this<HealthCheck>{
public:
    explicit HealthCheck(const std::shared_ptr<Core::Http::HttpManager>& manager, const HttpHealthConfig& config)
    : path(config.path()), manager_(manager) {
    };

    ~HealthCheck() {};

    void bind();

    void handle(Core::Http::HttpRequest &request, Core::Http::HttpResponse &response);

private:
    std::string path = "/health";
    std::shared_ptr<Core::Http::HttpManager> manager_;
};
}
}
