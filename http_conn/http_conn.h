#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>
#include <iostream>

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

public:
    void init(int fd, sockaddr_in addr);
    void process();

private:
    int         m_fd;
    sockaddr_in m_addr;
};

#endif