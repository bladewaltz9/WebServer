#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>
#include <iostream>

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

public:
    void init(int fd, sockaddr_in& addr);
    void process();
    bool read();

private:
    int         m_fd;
    sockaddr_in m_addr;

    std::string m_read_buf;  // read buffer
};

#endif