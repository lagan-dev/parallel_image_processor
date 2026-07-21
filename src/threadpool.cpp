#include "threadpool.h"

#include <stdexcept>

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
  if (numThreads == 0) numThreads = 1;
  for (size_t i = 0; i < numThreads; i++) {
    workers.emplace_back([this] { workerLoop(); });
  }
}

void ThreadPool::enqueue(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(queueMtx);
    if (stop) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }
    ++pendingTasks;
    tasks.push(std::move(task));
  }
  cv.notify_one();
}

void ThreadPool::waitAll() {
  std::unique_lock<std::mutex> lock(queueMtx);
  doneCv.wait(lock, [this] { return pendingTasks == 0; });
}

size_t ThreadPool::size() const { return workers.size(); }

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(queueMtx);
    stop = true;
  }
  cv.notify_all();
  for (auto& t : workers) {
    if (t.joinable()) t.join();
  }
}

void ThreadPool::workerLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(queueMtx);
      cv.wait(lock, [this] { return stop || !tasks.empty(); });
      if (stop && tasks.empty()) return;
      task = std::move(tasks.front());
      tasks.pop();
    }
    task();
    {
      std::lock_guard<std::mutex> lock(queueMtx);
      if (--pendingTasks == 0) doneCv.notify_all();
    }
  }
}