#pragma once
#include <event/event_loop.h>
#include <event/event_smart_ptr.h>
#include <functional>
#include <mutex>
#include <queue>

namespace Core::Event {
class AsyncQueue {
  static void CallbackFn(evutil_socket_t, short, void *handler);

public:
  explicit AsyncQueue(EventLoop *loop);
  ~AsyncQueue();

  // Push a task to the queue, thread safe
  void Push(std::function<void()> &&task);

private:
  void Notify() const;
  void Process();

private:
  int event_fd_;
  EventLoop *loop_;
  EventPtr event_;
  std::mutex mutex_;
  std::queue<std::function<void()>> tasks_;
};
} // namespace Core::Event