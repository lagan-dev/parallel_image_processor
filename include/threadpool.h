#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
 public:
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  explicit ThreadPool(size_t numThreads);
  ~ThreadPool();

  void enqueue(std::function<void()> task);
  void waitAll();
  size_t size() const;

 private:
  void workerLoop();

  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  bool stop;
  int pendingTasks = 0;
  std::mutex queueMtx;
  std::condition_variable cv;
  std::condition_variable doneCv;
};

#endif  // THREADPOOL_H