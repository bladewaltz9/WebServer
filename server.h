#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <unistd.h>

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
    void sockListen();
    void loopHandle();

private:
    uint16_t m_port;
    int32_t  m_serverFd;

    bool m_stop_server;  // 服务器是否停止
};

#endif