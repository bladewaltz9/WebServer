#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>

#include "../epoll_operate/epoll_operate.h"

class HttpConn {
public:
    static const int kReadBufSize = 2048;  // read buffer size

    static int m_epoll_fd;
    static int m_user_cnt;

public:
    HttpConn();
    ~HttpConn();

public:
    void init(int fd, sockaddr_in& addr);
    void process();
    // read messages sent by client
    bool read();

private:
    void InitState();

private:
    int         m_fd;
    sockaddr_in m_addr;

    char m_read_buf[kReadBufSize];
    int  m_read_idx;

    EpollOperate m_epoll_operate;
};

#endif