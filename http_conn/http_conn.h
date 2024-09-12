#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

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
    static const std::string kOk200Title;
    static const std::string kError400Title;
    static const std::string kError400FilePath;
    static const std::string kError403Title;
    static const std::string kError403FilePath;
    static const std::string kError404Title;
    static const std::string kError404FilePath;
    static const std::string kError500Title;
    static const std::string kError500FilePath;

    static const int         kReadBufSize  = 2048;  // read buffer size
    static const int         kWriteBufSize = 1024;  // write buffer size
    static const std::string kResourcePath;         // resource path

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
    void CloseConn();
    void process();
    // read messages sent by client
    bool read();
    // send messages to client
    bool write();

    sockaddr_in* GetClientAddr() {
        return &m_addr;
    }

private:
    void InitState();
    /**
     * @brief parse the HTTP request message
     * @return HTTP_CODE possible results:
     *          - BAD_REQUEST: there is an error in request,
     *          - FILE_REQUEST: parse successfully and then request the file
     */
    HTTP_CODE ParseHttpRequest();
    /**
     * @brief get the file requested by client
     * @return HTTP_CODE possible results:
     *          - GET_FILE: successfully get the file
     *          - NO_RESOURCE: the server has no resources
     *          - FORBIDDEN_REQUEST: client doesn't have access
     *          - BAD_REQUEST: there is an error in request
     */
    HTTP_CODE GetRequestedFile();

    // generate HTTP response message
    bool GenerateHttpResponse(HTTP_CODE ret);
    // add a line to write buffer
    bool AddToWriteBuf(const std::string& line);
    // add status line to write buffer
    bool AddStatusLine(int status, const std::string& title);
    // add headers to write buffer
    bool AddHeaders(int content_length);
    // map the file to memory
    bool MapContentFile(std::string file_path);
    void UnmapContentFile();

private:
    int         m_fd;
    sockaddr_in m_addr;

    char m_read_buf[kReadBufSize];
    int  m_read_idx;
    char m_write_buf[kWriteBufSize];
    int  m_write_idx;

    http_parser*    m_parser;
    HttpRequestInfo m_request_info;

    std::string m_file_name;  // requested file name
    void*       m_file_addr;  // mmap address of requested file
    struct stat m_file_stat;  // requested file status

    // iovec is used to write data to socket
    // m_iv[0]: write buffer data
    // m_iv[1]: file data
    struct iovec m_iv[2];

    size_t m_bytes_to_send;
    size_t m_bytes_have_send;
};

#endif