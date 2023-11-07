#include "http_conn.h"

int HttpConn::m_epoll_fd = -1;
int HttpConn::m_user_cnt = 0;

HttpConn::HttpConn() : m_fd(-1), m_read_idx(0) {}

HttpConn::~HttpConn() {}

void HttpConn::init(int fd, sockaddr_in& addr) {
    m_fd   = fd;
    m_addr = addr;

    // set port reuse
    int reuse = 1;
    setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // add clientFd to epoll
    m_epoll_operate.AddFd(m_epoll_fd, m_fd, false);
    ++m_user_cnt;

    InitState();
}

void HttpConn::InitState() {
    memset(m_read_buf, 0, kReadBufSize);

    m_read_idx = 0;
}

void HttpConn::process() {
    std::string clientIP   = inet_ntoa(m_addr.sin_addr);
    uint16_t    clientPort = ntohs(m_addr.sin_port);
    std::cout << "receive a message from " << clientIP << ":" << clientPort << std::endl;
    std::string recv_buf(m_read_buf);
    std::cout << recv_buf;
    m_epoll_operate.ModifyFd(m_epoll_fd, m_fd, EPOLLIN);
}

bool HttpConn::read() {
    InitState();
    if (m_read_idx >= kReadBufSize) { return false; }

    // read all data once
    while (true) {
        ssize_t bytesRead = recv(m_fd, m_read_buf + m_read_idx, kReadBufSize - m_read_idx, 0);
        if (bytesRead == -1) {
            // EAGAIN/EWOULDBLOCK indicates that there is no available data to receive
            // can call recv function again later
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return false;             // read error
        } else if (bytesRead == 0) {  // client has closed the connection
            return false;
        } else {
            m_read_idx += bytesRead;
        }
    }
    return true;
}