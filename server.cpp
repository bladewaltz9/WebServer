#include "server.h"

void* working(void* arg) {
    clientInfo* cInfo = (struct clientInfo*)arg;

    // get client IP and Port
    std::string clientIP   = inet_ntoa(cInfo->addr.sin_addr);
    uint16_t    clientPort = ntohs(cInfo->addr.sin_port);

    std::cout << "clientIP : " << clientIP << ", "
              << "clientPort : " << clientPort << std::endl;

    char        recvBuf[1024];
    std::string sendBuf = "hello, I'm server!\n";

    while (true) {
        memset(recvBuf, 0, sizeof(recvBuf));
        // receive data from client
        int ret = read(cInfo->fd, recvBuf, sizeof(recvBuf));
        if (ret == -1) {
            perror("Failed to read from client");
            close(cInfo->fd);
            return NULL;
        } else if (ret == 0) {
            std::cout << "client " << clientIP << ":" << clientPort << " disconnected" << std::endl;
            close(cInfo->fd);
            return NULL;
        }
        std::cout << "recvBuf: " << recvBuf;

        // send data to client
        if (write(cInfo->fd, sendBuf.c_str(), sendBuf.size()) == -1) {
            perror("Failed to send to client");
            close(cInfo->fd);
            return NULL;
        }
    }

    close(cInfo->fd);

    return NULL;
}

Server::Server() {
    m_port        = -1;
    m_serverFd    = -1;
    m_stop_server = false;
}

Server::~Server() {
    close(m_serverFd);
}

void Server::init(uint16_t port) {
    m_port = port;
}

void Server::sockListen() {
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
}

void Server::loopHandle() {
    while (!m_stop_server) {
        // client address
        sockaddr_in clientAddr;
        socklen_t   clientAddrLen = sizeof(clientAddr);

        int clientFd = accept(m_serverFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientFd == -1) {
            perror("Failed to accept client connection");
            close(m_serverFd);
            exit(-1);
        }

        // storage client info(fd and addr), send to thread function
        clientInfo* cInfo = new clientInfo();
        cInfo->fd         = clientFd;
        // cInfo->addr = clientAddr;
        memcpy(&cInfo->addr, &clientAddr, clientAddrLen);

        // create thread
        pthread_create(&cInfo->tid, NULL, working, (void*)cInfo);

        // set thread detach
        // The thread will automatically release resources when it ends.
        pthread_detach(cInfo->tid);
    }
}