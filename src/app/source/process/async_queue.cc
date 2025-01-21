#include "process/async_queue.h"
#include <spdlog/spdlog.h>
#include <sys/eventfd.h>
#include <unistd.h>

Core::Event::AsyncQueue::AsyncQueue(Core::Event::EventLoop *loop) : loop_(loop) {
  event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (event_fd_ == -1) {
    SPDLOG_ERROR("Failed to create event fd");
    throw std::runtime_error("Failed to create event fd");
  }

  event *ev = event_new(loop_->getEventBase(), event_fd_, EV_READ | EV_PERSIST, CallbackFn, this);
  if (ev == nullptr) {
    SPDLOG_ERROR("Failed to create event");
    throw std::runtime_error("Failed to create event");
  }
  event_.Reset(ev);
  event_add(ev, nullptr);
}

Core::Event::AsyncQueue::~AsyncQueue() {
  if (event_fd_ != -1) {
    close(event_fd_);
    event_fd_ = -1;
  }
}

void Core::Event::AsyncQueue::Push(std::function<void()> &&task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push(std::move(task));
  }
  Notify();
}

void Core::Event::AsyncQueue::Process() {
  std::queue<std::function<void()>> current_tasks;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    current_tasks.swap(tasks_);
  }
  while (!current_tasks.empty()) {
    auto task = std::move(current_tasks.front());
    current_tasks.pop();
    task();
  }
}

void Core::Event::AsyncQueue::CallbackFn(evutil_socket_t fd, short, void *handler) {
  uint64_t buffer[128];
  while (true) {
    ssize_t ret = read(fd, &buffer, sizeof(buffer));
    if (ret == 0) break;
    if (ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      SPDLOG_ERROR("Failed to read from eventfd, errno: {}", errno);
    }
  }

  auto queue = reinterpret_cast<AsyncQueue *>(handler);
  queue->Process();
}

void Core::Event::AsyncQueue::Notify() const {
  uint64_t one = 1;
  ssize_t ret = write(event_fd_, &one, sizeof(one));
  if (ret != sizeof(one)) {
    SPDLOG_ERROR("write event_fd_ failed, ret: {}", ret);
  }
}
