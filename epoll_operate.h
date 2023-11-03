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
    void AddFd(int epollFd, int fd, bool oneShot);
    void DeleteFd(int epollFd, int fd);
    void ModifyFd(int epollFd, int fd, int events);

    void SetNonBlocking(int fd);
};

#endif