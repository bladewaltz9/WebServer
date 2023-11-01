#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <unistd.h>

// storage client info
struct clientInfo {
    int         fd;
    pthread_t   tid;
    sockaddr_in addr;
};

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
        // receive data from client
        if (read(cInfo->fd, recvBuf, sizeof(recvBuf)) == -1) {
            perror("Failed to read from client");
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

int main() {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        perror("Failed to create socket");
        return -1;
    }

    // set port reuse
    int reuse = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // set server address
    sockaddr_in serverAddr;
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(10000);

    if (bind(serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Failed to bind socket");
        close(serverFd);
        return -1;
    }

    if (listen(serverFd, 8) == -1) {
        perror("Failed to listen socket");
        close(serverFd);
        return -1;
    }

    while (true) {
        // client address
        sockaddr_in clientAddr;
        socklen_t   clientAddrLen = sizeof(clientAddr);

        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientFd == -1) {
            perror("Failed to accept client connection");
            close(serverFd);
            return -1;
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

    close(serverFd);

    return 0;
}