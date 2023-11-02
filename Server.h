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

#include "EpollOperate.h"

const int MAX_EVENT_NUM = 10000;  // max event number

// storage client info
struct clientInfo {
    int         fd;
    pthread_t   tid;
    sockaddr_in addr;
};

class Server {
public:
    Server();
    ~Server();

public:
    void init(uint16_t port);

    // socket
    void eventListen();
    void eventLoopHandle();

private:
    uint16_t m_port;
    int32_t  m_serverFd;
    int32_t  m_epollFd;

    epoll_event events[MAX_EVENT_NUM];  // epoll kernel event table

    EpollOperate epollOperate;

    bool m_stop_server;  // 服务器是否停止
};

#endif