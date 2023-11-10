#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <sys/stat.h>

#include "../epoll_operate/epoll_operate.h"
#include "../http_parser/http_parser.h"

struct HttpRequestInfo {
    std::string method;
    std::string url;
    std::string version;
    std::string host;
    bool        keep_alive;  // keep-alive or close
    std::string content_type;
    std::string content_length;
    std::string body;

    std::string last_header_field;  // the last header field
};

class HttpConn {
public:
    static const int         kReadBufSize = 2048;  // read buffer size
    static const std::string kResourcePath;        // resource path

    static int m_epoll_fd;
    static int m_user_cnt;

    // HTTP request message parsing results
    enum HTTP_CODE {
        BAD_REQUEST,        // there is an error in request
        GET_REQUEST,        // successfully get a complete request
        GET_FILE,           // successfully get the file
        NO_RESOURCE,        // the server has no resources
        FORBIDDEN_REQUEST,  // client doesn't have access
        INTERNAL_ERROR,     // server internal error
    };

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
    /**
     * @brief parse the HTTP request message
     * @return HTTP_CODE possible results:
     *          - BAD_REQUEST: there is an error in request,
     *          - FILE_REQUEST: parse successfully and then request the file
     */
    HTTP_CODE ParseHttpRequest();
    HTTP_CODE GetRequestedFile();

private:
    int         m_fd;
    sockaddr_in m_addr;

    char m_read_buf[kReadBufSize];
    int  m_read_idx;

    EpollOperate    m_epoll_operate;
    http_parser*    m_parser;
    HttpRequestInfo m_request_info;

    std::string m_file_name;
    struct stat m_file_stat;
};

#endif