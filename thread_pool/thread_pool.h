#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "thread_safe_queue.h"

template <typename T>
class ThreadPool {
public:
    ThreadPool(int num_threads, int max_requests)
        : m_threads(num_threads),
          m_queue(std::make_unique<ThreadSafeQueue<T*>>(max_requests)),
          m_shutdown(false) {}

    ThreadPool(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() {}

private:
    // Built-in thread worker class
    class ThreadWorker {
    public:
        ThreadWorker(ThreadPool* pool, int id) : m_pool(pool), m_id(id) {
            std::cout << "Success to start thread " << m_id << std::endl;
        }

        // overload () operation, use object() to call
        void operator()() {
            bool dequeued;  // whether dequeue successful

            while (!m_pool->m_shutdown) {
                // lock for conditional variable
                std::unique_lock<std::mutex> lock(m_pool->m_condition_mutex);

                if (m_pool->m_queue->empty()) {
                    m_pool->m_condition.wait(lock);  // wait conditional variable notify
                }

                T* request;
                dequeued = m_pool->m_queue->dequeue(request);

                // if dequeue successfully, call worker function
                if (dequeued) { request->process(); }
            }
        }

    private:
        ThreadPool* m_pool;  // thread pool which it belongs
        int         m_id;    // worker id
    };

public:
    void init();              // start thread pool
    void shutdown();          // shutdown thread pool
    bool append(T* request);  // add request to request queue

private:
    std::vector<std::thread>             m_threads;  // work threads queue
    std::unique_ptr<ThreadSafeQueue<T*>> m_queue;    // tasks queue
    std::mutex                           m_condition_mutex;
    std::condition_variable              m_condition;

    bool m_shutdown;  // whether shutdown thread pool
};

template <typename T>
void ThreadPool<T>::init() {
    for (int i = 0; i < (int)m_threads.size(); ++i) {
        m_threads[i] = std::thread(ThreadWorker(this, i));
        m_threads[i].detach();
    }
}

template <typename T>
void ThreadPool<T>::shutdown() {
    m_shutdown = true;
    m_condition.notify_all();
}

template <typename T>
bool ThreadPool<T>::append(T* request) {
    if (!m_queue->enqueue(request)) return false;
    m_condition.notify_one();
    return true;
}

#endif