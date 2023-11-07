#include "server.h"

Server::Server() {
    m_port        = -1;
    m_serverFd    = -1;
    m_epollFd     = -1;
    m_stop_server = false;
    m_pool        = nullptr;

    clients = new HttpConn[kMaxFDNum];
}

Server::~Server() {
    close(m_serverFd);
    close(m_epollFd);
    delete[] clients;
    delete m_pool;
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
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd == -1) {
        perror("Failed to create socket");
        exit(-1);
    }

    // set port reuse
    int reuse = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // set server address
    sockaddr_in serverAddr;
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(m_port);

    if (bind(m_serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Failed to bind socket");
        close(m_serverFd);
        exit(-1);
    }

    if (listen(m_serverFd, 8) == -1) {
        perror("Failed to listen socket");
        close(m_serverFd);
        exit(-1);
    }

    // create epoll
    m_epollFd = epoll_create(1);
    if (m_epollFd == -1) {
        perror("Failed to create epoll");
        exit(-1);
    }

    epollOperate.AddFd(m_epollFd, m_serverFd, false);
}

void Server::EventLoopHandle() {
    char        recvBuf[1024];
    std::string sendBuf = "hello, I'm server!\n";

    while (!m_stop_server) {
        // wait event triggering
        int readyNum = epoll_wait(m_epollFd, events, kMaxEventNum, -1);
        if (readyNum == -1) {
            perror("Fail to epoll_wait");
            exit(-1);
        }

        // handle ready event
        for (int i = 0; i < readyNum; ++i) {
            int curFd = events[i].data.fd;
            if (curFd == m_serverFd) {  // there is a client requesting connection
                // accept client connection
                sockaddr_in clientAddr;
                socklen_t   clientAddrLen = sizeof(clientAddr);

                int clientFd = accept(m_serverFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
                if (clientFd == -1) {
                    perror("Failed to accept client connection");
                    close(m_serverFd);
                    exit(-1);
                }

                // print connected client info
                std::string clientIP   = inet_ntoa(clientAddr.sin_addr);
                uint16_t    clientPort = ntohs(clientAddr.sin_port);
                std::cout << "clientIP : " << clientIP << ", "
                          << "clientPort : " << clientPort << std::endl;

                // add clientFd to epoll
                epollOperate.AddFd(m_epollFd, clientFd, false);
                // init the info of new client
                clients[clientFd].init(clientFd, clientAddr);
            } else if (events[i].events & EPOLLIN) {  // read event
                memset(recvBuf, 0, sizeof(recvBuf));
                int ret = read(curFd, recvBuf, sizeof(recvBuf));
                if (ret == -1) {
                    perror("Failed to read from client");
                    close(curFd);
                    exit(-1);
                } else if (ret == 0) {
                    std::cout << "client disconnected" << std::endl;
                    close(curFd);
                    continue;
                }
                std::cout << "recvBuf: " << recvBuf;

                if (!m_pool->append(&clients[curFd])) { std::cout << "append failed" << std::endl; }
                sleep(1);

                // // send data to client
                // if (write(curFd, sendBuf.c_str(), sendBuf.size()) == -1) {
                //     perror("Failed to send to client");
                //     close(curFd);
                //     exit(-1);
                // }
            }
        }
    }
}

void Server::InitThreadPool() {
    m_pool = new ThreadPool<HttpConn>(kThreadNum, kMaxRequests);
    m_pool->init();
}