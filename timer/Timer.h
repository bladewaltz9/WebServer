#ifndef TIMER_H
#define TIMER_H

#include <list>
#include <time.h>

struct TimerNode {
    int    m_epoll_fd;
    int    m_sock_fd;
    time_t m_expire;
    void   (*m_callback)(int, int);
};

class Timer {
public:
    Timer() {}
    ~Timer() {}

public:
    void AddTimer(TimerNode* timer_node) {
        m_list.push_back(timer_node);
    }

    void DelTimer(TimerNode* timer_node) {
        m_list.remove(timer_node);
        delete timer_node;
    }

    void UpdataTimer(TimerNode* timer_node) {
        m_list.remove(timer_node);
        m_list.push_back(timer_node);
    }

    // timer event handler
    void Tick() {
#ifdef ENABLE_DEBUG
        std::cout << "timer tick" << std::endl;
#endif
        if (m_list.empty()) { return; }
        time_t cur_time = time(NULL);
        while (!m_list.empty()) {
            TimerNode* timer_node = m_list.front();
            // if current timer node is not expired, then break
            if (timer_node->m_expire > cur_time) { break; }
            timer_node->m_callback(timer_node->m_epoll_fd, timer_node->m_sock_fd);
            m_list.pop_front();
        }
    }

private:
    std::list<TimerNode*> m_list;
};

#endif