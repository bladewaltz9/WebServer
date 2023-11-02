#include "EpollOperate.h"

EpollOperate::EpollOperate() {}
EpollOperate::~EpollOperate() {}

/**
 * @brief Register read event in the kernel event table
 * @param epollFd epoll fd
 * @param fd the fd triggered event
 * @param oneShot Whether to add EPOLLONESHOT option
 */
void EpollOperate::addFd(int epollFd, int fd, bool oneShot) {
    epoll_event event;
    event.data.fd = fd;
    event.events  = EPOLLIN | EPOLLRDHUP;

    if (oneShot) event.events |= EPOLLONESHOT;

    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);

    setNonBlocking(fd);
}

/**
 * @brief delete event from the kernel event table
 * @param epollFd epoll fd
 * @param fd the fd to delete
 */
void EpollOperate::delFd(int epollFd, int fd) {
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

/**
 * @brief modify fd events
 * @attention need to reset fd's EPOLLONESHOT event
 * @param epollFd epoll fd
 * @param fd the fd to modify
 * @param events events to modify
 */
void EpollOperate::modFd(int epollFd, int fd, int events) {
    epoll_event event;
    event.data.fd = fd;
    event.events  = events | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event);
}

/**
 * @brief set fd to non-blocking mode.
 */
void EpollOperate::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}