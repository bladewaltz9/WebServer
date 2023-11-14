#include "http_conn.h"

const std::string HttpConn::kOk200Title       = "OK";
const std::string HttpConn::kError400Title    = "Bad Request";
const std::string HttpConn::kError400FilePath = "/error_400.html";
const std::string HttpConn::kError403Title    = "Forbidden";
const std::string HttpConn::kError403FilePath = "/error_403.html";
const std::string HttpConn::kError404Title    = "Not Found";
const std::string HttpConn::kError404FilePath = "/error_404.html";
const std::string HttpConn::kError500Title    = "Internal Error";
const std::string HttpConn::kError500FilePath = "/error_500.html";

const std::string HttpConn::kResourcePath = "./resources";

int HttpConn::m_epoll_fd = -1;
int HttpConn::m_user_cnt = 0;

HttpConn::HttpConn()
    : m_fd(-1),
      m_read_idx(0),
      m_write_idx(0),
      m_parser(nullptr),
      m_bytes_to_send(0),
      m_bytes_have_send(0) {}

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
    EpollOperate::AddFd(m_epoll_fd, m_fd, false);
    ++m_user_cnt;

    // init http parser, allocate memory
    m_parser = (http_parser*)malloc(sizeof(http_parser));

    InitState();
}

void HttpConn::InitState() {
    // clear read/write buffer
    memset(m_read_buf, 0, kReadBufSize);
    memset(m_write_buf, 0, kWriteBufSize);
    m_read_idx  = 0;
    m_write_idx = 0;

    m_bytes_have_send = 0;
    m_bytes_to_send   = 0;
}

void HttpConn::CloseConn() {
    if (m_fd != -1) {
        EpollOperate::DeleteFd(m_epoll_fd, m_fd);
        m_fd = -1;
        --m_user_cnt;
    }
}

void HttpConn::process() {
#ifdef ENABLE_LOG
    // std::string clientIP   = inet_ntoa(m_addr.sin_addr);
    // uint16_t    clientPort = ntohs(m_addr.sin_port);
    // std::cout << "receive a message from " << clientIP << ":" << clientPort << std::endl;
    // std::string recv_buf(m_read_buf);
    // std::cout << recv_buf;
#endif

    HTTP_CODE ret_request = ParseHttpRequest();
    if (ret_request == GET_REQUEST) { ret_request = GetRequestedFile(); }

    bool ret_response = GenerateHttpResponse(ret_request);
    if (!ret_response) {
        perror("GenerateHttpResponse error");
        CloseConn();
        return;
    }

    // register write event
    EpollOperate::ModifyFd(m_epoll_fd, m_fd, EPOLLOUT);
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

bool HttpConn::write() {
    while (true) {
        if (m_bytes_to_send <= 0) {
            UnmapContentFile();
            // register read event and reset EPOLLONESHOT
            EpollOperate::ModifyFd(m_epoll_fd, m_fd, EPOLLIN);
            // check if need to keep alive
            if (m_request_info.keep_alive) {
                InitState();
                return true;
            } else {
                return false;
            }
        }

        // write data to socket
        ssize_t bytesWrite = writev(m_fd, m_iv, 2);
        if (bytesWrite == -1) {
            // EAGAIN/EWOULDBLOCK indicates that the socket buffer is full
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                EpollOperate::ModifyFd(m_epoll_fd, m_fd, EPOLLOUT);
                return true;
            }
            perror("write error");
            UnmapContentFile();
            return false;
        } else {
            m_bytes_to_send -= bytesWrite;
            m_bytes_have_send += bytesWrite;

            if (m_bytes_have_send >= m_iv[0].iov_len) {  // m_iv[0] had been sent
                m_iv[0].iov_len = 0;
                m_iv[1].iov_base =
                    static_cast<char*>(m_iv[1].iov_base) + (m_bytes_have_send - m_iv[0].iov_len);
                m_iv[1].iov_len = m_bytes_to_send;
            } else {  // m_iv[0] had not been sent
                m_iv[0].iov_base = static_cast<char*>(m_iv[0].iov_base) + bytesWrite;
                m_iv[0].iov_len -= bytesWrite;
            }
        }
    }
}

int on_message_begin_cb(http_parser* parser) {
    // std::cout << "Message complete" << std::endl;
    return 0;
}

int on_headers_complete_cb(http_parser* parser) {
    // std::cout << "Headers complete" << std::endl;
    HttpRequestInfo* request = static_cast<HttpRequestInfo*>(parser->data);
    request->method          = std::string(http_method_str((http_method)parser->method));
    request->version =
        "HTTP/" + std::to_string(parser->http_major) + "." + std::to_string(parser->http_minor);
    // Determine whether it is Keep-Alive
    if (parser->http_major > 0 && parser->http_minor > 0) {  // HTTP 1.1 or above
        request->keep_alive = true;
    }
    return 0;
}

int on_message_complete_cb(http_parser* parser) {
    // std::cout << "Message complete" << std::endl;
    return 0;
}

int on_url_cb(http_parser* parser, const char* at, size_t length) {
    // std::cout << "URL: " << std::string(at, length) << std::endl;
    HttpRequestInfo* request = static_cast<HttpRequestInfo*>(parser->data);
    request->url             = std::string(at, length);
    return 0;
}

int on_header_field_cb(http_parser* parser, const char* at, size_t length) {
    // std::cout << "Header field: " << std::string(at, length) << std::endl;
    HttpRequestInfo* request   = static_cast<HttpRequestInfo*>(parser->data);
    request->last_header_field = std::string(at, length);
    return 0;
}

int on_header_value_cb(http_parser* parser, const char* at, size_t length) {
    // std::cout << "Header value: " << std::string(at, length) << std::endl;
    HttpRequestInfo* request = static_cast<HttpRequestInfo*>(parser->data);
    if (request->last_header_field == "Host") {
        request->host = std::string(at, length);
    } else if (!request->keep_alive && request->last_header_field == "Connection") {
        // Determine whether it is Keep-Alive
        if (std::string(at, length) == "keep-alive") { request->keep_alive = true; }
    } else if (request->last_header_field == "Content-Type") {
        request->content_type = std::string(at, length);
    } else if (request->last_header_field == "Content-Length") {
        request->content_length = std::string(at, length);
    }
    return 0;
}

int on_body_cb(http_parser* parser, const char* at, size_t length) {
    // std::cout << "Body: " << std::string(at, length) << std::endl;
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
#ifdef ENABLE_LOG
        std::cout << "Error parsing HTTP request" << std::endl;
#endif
        return BAD_REQUEST;
    }
    return GET_REQUEST;
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

bool HttpConn::GenerateHttpResponse(HTTP_CODE ret) {
    switch (ret) {
        case BAD_REQUEST:
            AddStatusLine(400, kError400Title);
            MapContentFile(kError400FilePath);
            AddHeaders(m_file_stat.st_size);
            break;
        case NO_RESOURCE:
            AddStatusLine(404, kError404Title);
            MapContentFile(kError404FilePath);
            AddHeaders(m_file_stat.st_size);
            break;
        case FORBIDDEN_REQUEST:
            AddStatusLine(403, kError403Title);
            MapContentFile(kError403FilePath);
            AddHeaders(m_file_stat.st_size);
            break;
        case INTERNAL_ERROR:
            AddStatusLine(500, kError500Title);
            MapContentFile(kError500FilePath);
            AddHeaders(m_file_stat.st_size);
            break;
        case GET_FILE:
            AddStatusLine(200, kOk200Title);
            MapContentFile(m_file_name);
            AddHeaders(m_file_stat.st_size);
            break;
        default: return false;
    }
    // iovec point to the data to be sent
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len  = m_write_idx;
    m_iv[1].iov_base = m_file_addr;
    m_iv[1].iov_len  = m_file_stat.st_size;

    m_bytes_to_send = m_write_idx + m_file_stat.st_size;

#ifdef DEBUG
    // Print the contents of the buffer pointed by m_read_buf and m_file_addr
    printf("m_wirte_buf:\n%.*s\n", m_write_idx, m_write_buf);
    printf("m_file_addr:\n%.*s\n", (int)m_file_stat.st_size, m_file_addr);
#endif

    return true;
}

bool HttpConn::AddToWriteBuf(const std::string& line) {
    if (m_write_idx + line.size() > kWriteBufSize) { return false; }
    strncpy(m_write_buf + m_write_idx, line.c_str(), line.size());
    m_write_idx += line.size();
    return true;
}

bool HttpConn::AddStatusLine(int status, const std::string& title) {
    std::string status_line = "HTTP/1.1 " + std::to_string(status) + " " + title + "\r\n";
    return AddToWriteBuf(status_line);
}

bool HttpConn::AddHeaders(int content_length) {
    bool success = true;
    success &= AddToWriteBuf("Content-Length: " + std::to_string(content_length) + "\r\n");
    success &= AddToWriteBuf("Content-Type: text/html; charset=UTF-8\r\n");
    success &= AddToWriteBuf("Connection: ");
    if (m_request_info.keep_alive) {
        success &= AddToWriteBuf("keep-alive\r\n");
    } else {
        success &= AddToWriteBuf("close\r\n");
    }
    success &= AddToWriteBuf("\r\n");
    return success;
}

bool HttpConn::MapContentFile(std::string file_path) {
    file_path = kResourcePath + file_path;
    // open file in read-only mode
    int file_fd = open(file_path.c_str(), O_RDONLY);
    if (file_fd == -1) {
        perror("open file error");
        return false;
    }
    stat(file_path.c_str(), &m_file_stat);
    // map file to memory
    m_file_addr = (char*)mmap(nullptr, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (m_file_addr == MAP_FAILED) {
        perror("mmap error");
        return false;
    }
    close(file_fd);
    return true;
}

void HttpConn::UnmapContentFile() {
    if (m_file_addr) {
        munmap(m_file_addr, m_file_stat.st_size);
        m_file_addr = nullptr;
    }
}