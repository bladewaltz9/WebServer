#include "server.h"

// defined outside the class: avoid make_shared<> compile wrong in InitThreadPool();
const int Server::kMaxRequests;
const int Server::kThreadNum;

Server::Server()
    : m_stop_server(false),
      m_port(-1),
      m_server_fd(-1),
      m_epoll_fd(-1),
      m_events(kMaxEventNum),
      m_clients(kMaxFDNum),
      m_pool(nullptr) {}

Server::~Server() {
    close(m_server_fd);
    close(m_epoll_fd);
}

void Server::init(uint16_t port) {
    m_port = port;
}

void Server::start() {
    InitThreadPool();
    EventListen();
    EventLoopHandle();
}

void Server::EventListen() {
    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd == -1) {
        perror("Failed to create socket");
        exit(-1);
    }

    // set port reuse
    int reuse = 1;
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // set server address
    sockaddr_in server_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(m_port);

    if (bind(m_server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Failed to bind socket");
        close(m_server_fd);
        exit(-1);
    }

    if (listen(m_server_fd, 8) == -1) {
        perror("Failed to listen socket");
        close(m_server_fd);
        exit(-1);
    }

    // create epoll
    m_epoll_fd = epoll_create(1);
    if (m_epoll_fd == -1) {
        perror("Failed to create epoll");
        exit(-1);
    }
    HttpConn::m_epoll_fd = m_epoll_fd;

    m_epoll_operate.AddFd(m_epoll_fd, m_server_fd, false);
}

void Server::EventLoopHandle() {
    while (!m_stop_server) {
        // wait event triggering
        int ready_num = epoll_wait(m_epoll_fd, &m_events[0], Server::kMaxEventNum, -1);
        if (ready_num == -1) {
            perror("Fail to epoll_wait");
            exit(-1);
        }

        // handle ready event
        for (int i = 0; i < ready_num; ++i) {
            int cur_fd = m_events[i].data.fd;
            if (cur_fd == m_server_fd) {  // there is a client requesting connection
                // accept client connection
                sockaddr_in client_addr;
                socklen_t   client_addr_len = sizeof(client_addr);

                int client_fd =
                    accept(m_server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
                if (client_fd == -1) {
                    perror("Failed to accept client connection");
                    close(m_server_fd);
                    exit(-1);
                }

                // print connected client info
                std::string client_ip   = inet_ntoa(client_addr.sin_addr);
                uint16_t    client_port = ntohs(client_addr.sin_port);
                std::cout << "client_ip : " << client_ip << ", "
                          << "client_port : " << client_port << std::endl;

                // init the info of new client
                m_clients[client_fd].init(client_fd, client_addr);
            } else if (m_events[i].events & EPOLLIN) {  // read event
                if (m_clients[cur_fd].read()) { m_pool->append(&m_clients[cur_fd]); }
            }
        }
    }
}

void Server::InitThreadPool() {
    m_pool = std::make_shared<ThreadPool<HttpConn>>(kThreadNum, kMaxRequests);
    m_pool->init();
}