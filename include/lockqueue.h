#pragma once
#include <condition_variable> // pthread_condition_t
#include <mutex>              // pthread_mutex_t
#include <queue>
#include <thread>

// 异步写日志的日志队列
template <typename T> class LockQueue {
public:
  void Push(const T &data) { // 多个 worker 线程都会写日志 queue
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(data);
    m_condvariable.notify_one();
  }

  T Pop() { // 一个线程读日志 queue，写日志文件
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_queue.empty()) // 日志队列为空，线程进入 wait 状态
      m_condvariable.wait(lock);
    T data = m_queue.front();
    m_queue.pop();
    return data;
  }

private:
  std::queue<T> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_condvariable;
};