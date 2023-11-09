#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "../epoll_operate/epoll_operate.h"
#include "../http_parser/http_parser.h"

struct HttpRequestInfo {
    std::string method;
    std::string url;
    std::string version;
    std::string host;
    std::string connection;  // keep-alive or close
    std::string content_type;
    std::string content_length;
    std::string body;

    std::string last_header_field;  // the last header field
};

class HttpConn {
public:
    static const int kReadBufSize = 2048;  // read buffer size

    static int m_epoll_fd;
    static int m_user_cnt;

    // // HTTP request message parsing results
    // enum HTTP_CODE {
    //     NO_REQUEST,         // the request is incomplete
    //     GET_REQUEST,        // get a complete request
    //     BAD_REQUEST,        // there is an error in request
    //     NO_RESOURCE,        // the server has no resources
    //     FORBIDDEN_REQUEST,  // client doesn't have access
    //     FILE_REQUEST,       // successfully get the file
    //     INTERNAL_ERROR,     // server internal error
    //     CLOSED_CONNECTION   // client has closed connection
    // };

public:
    HttpConn();
    ~HttpConn();

public:
    void init(int fd, sockaddr_in& addr);
    void process();
    bool ParseHttpRequest();

    // read messages sent by client
    bool read();

private:
    void InitState();

private:
    int         m_fd;
    sockaddr_in m_addr;

    char m_read_buf[kReadBufSize];
    int  m_read_idx;

    EpollOperate    m_epoll_operate;
    http_parser*    m_parser;
    HttpRequestInfo m_request_info;
};

#endif