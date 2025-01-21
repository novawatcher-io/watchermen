#pragma once

#include <memory>
#include <functional>

#include "http/http_request.h"
#include "http/http_response.h"
#include "http/http_manager.h"

namespace App {
namespace Process {
using namespace std::placeholders;
class Manager;

class ProcessHttpHelper :public Core::Noncopyable, public std::enable_shared_from_this<ProcessHttpHelper>{
public:
    explicit ProcessHttpHelper(const std::shared_ptr<Core::Http::HttpManager>& manager,
                               Manager* processManager)
            :manager_(manager), processManager(processManager) {
    };

    ~ProcessHttpHelper() {};

    void bind();

    void handle(Core::Http::HttpRequest &request, Core::Http::HttpResponse &response);

private:
    std::string path = "/process/list";
    const std::shared_ptr<Core::Http::HttpManager>& manager_;
    Manager* processManager = nullptr;
};
}
}
