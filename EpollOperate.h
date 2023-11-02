#ifndef EPOLLFUNCS_H
#define EPOLLFUNCS_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

class EpollOperate {
public:
    EpollOperate();
    ~EpollOperate();

public:
    void addFd(int epollFd, int fd, bool oneShot);
    void delFd(int epollFd, int fd);
    void modFd(int epollFd, int fd, int events);

    void setNonBlocking(int fd);
};

#endif