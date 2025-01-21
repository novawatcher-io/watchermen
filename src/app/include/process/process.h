/**
******************************************************************************
* @file           : process.h
* @author         : zhanglei
* @brief          : None
* @attention      : None
* @date           : 2024/2/11
******************************************************************************
*/
//
// Created by zhanglei on 2024/2/11.
//

#pragma once
#include "os/unix_cgroup.h"
#include "component/process/process.h"

namespace App {
namespace Process {
class Process :public Core::Component::Process::Process {
public:
    explicit Process(const std::string &command, const std::shared_ptr<Core::Event::EventLoop>& loop)
    : Core::Component::Process::Process(command, loop) {

    }

    void onCreate() override {

    };
    void onStart() override {

    };
    void onStop() override {

    };
    void onDestroy() override {

    };

    ~Process() {}

private:
};
}
}
