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

const int kMaxEventNum = 10000;  // max event number
const int kThreadNum   = 4;      // the num of thread
const int kMaxRequests = 1000;   // the max num of requests
const int kMaxFDNum    = 65535;  // the max num of fd

class Server {
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

    uint16_t  m_port;
    int32_t   m_serverFd;
    HttpConn* clients;

    int32_t      m_epollFd;
    epoll_event  events[kMaxEventNum];  // epoll kernel event table
    EpollOperate epollOperate;

    ThreadPool<HttpConn>* m_pool;
};

#endif