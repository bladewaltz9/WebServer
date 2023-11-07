#include "http_conn.h"

HttpConn::HttpConn() {}

HttpConn::~HttpConn() {}

void HttpConn::init(int fd, sockaddr_in addr) {
    m_fd   = fd;
    m_addr = addr;
}

void HttpConn::process() {
    std::string clientIP   = inet_ntoa(m_addr.sin_addr);
    uint16_t    clientPort = ntohs(m_addr.sin_port);
    std::cout << "receive a message from " << clientIP << ":" << clientPort << std::endl;
}