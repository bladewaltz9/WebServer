#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll_operate/epoll_operate.h"
#include "http_conn/http_conn.h"
#include "thread_pool/thread_pool.h"

class Server {
public:
    static const int kMaxEventNum = 10000;  // max event number
    static const int kThreadNum   = 4;      // the num of thread
    static const int kMaxRequests = 1000;   // the max num of requests
    static const int kMaxFDNum    = 65535;  // the max num of fd

public:
    Server();
    ~Server();

public:
    void init(uint16_t port);
    void start();

private:
    // create socket and begin listenning
    void EventListen();
    // accept the connectiong of client and handle connection
    void EventLoopHandle();
    // initial thread pool
    void InitThreadPool();

private:
    bool m_stop_server;

    uint16_t m_port;
    int32_t  m_serverFd;

    int32_t                  m_epollFd;
    std::vector<epoll_event> m_events;  // epoll kernel event table
    std::vector<HttpConn>    m_clients;

    EpollOperate m_epollOperate;

    // TODO: 这里使用shared_ptr，但thread_pool.h 的内置线程类中没有使用，会不会有内存泄漏？
    std::shared_ptr<ThreadPool<HttpConn>> m_pool;
};

#endif