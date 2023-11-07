#ifndef THREAD_SAFE_m_queueH
#define THREAD_SAFE_m_queueH

#include <mutex>
#include <queue>

// Thread safe implementation of a Queue using a std::queue
template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() {
        m_max_size = 1000;
    }
    ThreadSafeQueue(int max_size) : m_max_size(max_size) {}
    ThreadSafeQueue(ThreadSafeQueue&& other) {}  // TODO: 移动构造函数
    ~ThreadSafeQueue() {}

public:
    bool enqueue(T& t) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if ((int)m_queue.size() >= m_max_size) return false;
        m_queue.emplace(t);
        return true;
    }

    bool dequeue(T& t) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_queue.empty()) return false;

        t = std::move(m_queue.front());  // TODO: 这里使用 move ，是否会对性能有提升？
        m_queue.pop();
        return true;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    int size() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

private:
    std::queue<T> m_queue;
    std::mutex    m_mutex;
    int           m_max_size;
};

#endif