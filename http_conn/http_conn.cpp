#include "http_conn.h"

const std::string HttpConn::kResourcePath = "./resources";

int HttpConn::m_epoll_fd = -1;
int HttpConn::m_user_cnt = 0;

HttpConn::HttpConn() : m_fd(-1), m_read_idx(0), m_parser(nullptr) {}

HttpConn::~HttpConn() {
    free(m_parser);
}

void HttpConn::init(int fd, sockaddr_in& addr) {
    m_fd   = fd;
    m_addr = addr;

    // set port reuse
    int reuse = 1;
    setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // add clientFd to epoll
    m_epoll_operate.AddFd(m_epoll_fd, m_fd, false);
    ++m_user_cnt;

    // init http parser, allocate memory
    m_parser = (http_parser*)malloc(sizeof(http_parser));

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
    // std::string recv_buf(m_read_buf);
    // std::cout << recv_buf;

    HTTP_CODE ret_request = ParseHttpRequest();
    if (ret_request == GET_REQUEST) { ret_request = GetRequestedFile(); }
    std::cout << "m_file_name = " << m_file_name << std::endl;
    std::cout << "ret_request = " << ret_request << std::endl;
}

int on_message_begin_cb(http_parser* parser) {
    std::cout << "Message complete" << std::endl;
    return 0;
}

int on_headers_complete_cb(http_parser* parser) {
    std::cout << "Headers complete" << std::endl;
    HttpRequestInfo* request = static_cast<HttpRequestInfo*>(parser->data);
    request->method          = std::string(http_method_str((http_method)parser->method));
    request->version =
        "HTTP/" + std::to_string(parser->http_major) + "." + std::to_string(parser->http_minor);
    return 0;
}

int on_message_complete_cb(http_parser* parser) {
    std::cout << "Message complete" << std::endl;
    return 0;
}

int on_url_cb(http_parser* parser, const char* at, size_t length) {
    std::cout << "URL: " << std::string(at, length) << std::endl;
    HttpRequestInfo* request = static_cast<HttpRequestInfo*>(parser->data);
    request->url             = std::string(at, length);
    return 0;
}

int on_header_field_cb(http_parser* parser, const char* at, size_t length) {
    std::cout << "Header field: " << std::string(at, length) << std::endl;
    HttpRequestInfo* request   = static_cast<HttpRequestInfo*>(parser->data);
    request->last_header_field = std::string(at, length);
    return 0;
}

int on_header_value_cb(http_parser* parser, const char* at, size_t length) {
    std::cout << "Header value: " << std::string(at, length) << std::endl;
    HttpRequestInfo* request = static_cast<HttpRequestInfo*>(parser->data);
    if (request->last_header_field == "Host") {
        request->host = std::string(at, length);
    } else if (request->last_header_field == "Connection") {
        // Determine whether it is Keep-Alive
        if (parser->http_major > 0 && parser->http_minor > 0) {  // HTTP 1.1 or above
            request->keep_alive = true;
        } else {  // HTTP 1.0 or below
            if (std::string(at, length) == "keep-alive") {
                request->keep_alive = true;
            } else {
                request->keep_alive = false;
            }
        }
    } else if (request->last_header_field == "Content-Type") {
        request->content_type = std::string(at, length);
    } else if (request->last_header_field == "Content-Length") {
        request->content_length = std::string(at, length);
    }
    return 0;
}

int on_body_cb(http_parser* parser, const char* at, size_t length) {
    std::cout << "Body: " << std::string(at, length) << std::endl;
    HttpRequestInfo* request = static_cast<HttpRequestInfo*>(parser->data);
    request->body            = std::string(at, length);
    return 0;
}

HttpConn::HTTP_CODE HttpConn::ParseHttpRequest() {
    http_parser_settings parser_set;

    // // test http request parsing
    // std::string test_request = "GET /index.html HTTP/1.1\r\n"
    //                            "Host:www.example.com\r\n"
    //                            "Connection:keep-alive\r\n"
    //                            "";
    // std::strcpy(m_read_buf, test_request.c_str());
    // m_read_idx = test_request.size();

    // http_parser callback functions
    http_parser_settings_init(&parser_set);
    parser_set.on_message_begin    = on_message_begin_cb;
    parser_set.on_header_field     = on_header_field_cb;
    parser_set.on_header_value     = on_header_value_cb;
    parser_set.on_url              = on_url_cb;
    parser_set.on_body             = on_body_cb;
    parser_set.on_headers_complete = on_headers_complete_cb;
    parser_set.on_message_complete = on_message_complete_cb;

    http_parser_init(m_parser, HTTP_REQUEST);
    // m_request_info 用于回调函数中保存解析到的 Http 请求信息。
    m_parser->data = &m_request_info;

    size_t parsed_len = http_parser_execute(m_parser, &parser_set, m_read_buf, m_read_idx);

    if ((int)parsed_len != m_read_idx || m_parser->http_errno != 0) {
        std::cout << "Error parsing HTTP request" << std::endl;
        return BAD_REQUEST;
    }
    return GET_REQUEST;

    // std::cout << "Method: " << m_request_info.method << std::endl;
    // std::cout << "URL: " << m_request_info.url << std::endl;
    // std::cout << "Version: " << m_request_info.version << std::endl;
    // std::cout << "Host: " << m_request_info.host << std::endl;
    // std::cout << "keep-alive: " << m_request_info.keep_alive << std::endl;
    // std::cout << "Content-Type: " << m_request_info.content_type << std::endl;
    // std::cout << "Content-Length: " << m_request_info.content_length << std::endl;
    // std::cout << "Body: " << m_request_info.body << std::endl;
}

HttpConn::HTTP_CODE HttpConn::GetRequestedFile() {
    // get requested file path from url
    m_file_name = m_request_info.url;
    if (m_file_name == "/") m_file_name += "index.html";
    std::string file_path = kResourcePath + m_file_name;

    // get file status, check if file exists
    if (stat(file_path.c_str(), &m_file_stat) == -1) {
        perror("file not exist");
        return NO_RESOURCE;
    }

    // check if file has read permission
    if ((m_file_stat.st_mode & S_IROTH) == 0) {
        perror("file has no read permission");
        return FORBIDDEN_REQUEST;
    }

    // check if it's a directory
    if (S_ISDIR(m_file_stat.st_mode)) {
        perror("file is a directory");
        return BAD_REQUEST;
    }
    return GET_FILE;
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