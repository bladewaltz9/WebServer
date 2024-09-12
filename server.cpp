#include "server.h"

// defined outside the class: avoid make_shared<> compile wrong in InitThreadPool();
const int Server::kMaxRequests;
const int Server::kThreadNum;

void TimerCallback(int epoll_fd, int sock_fd) {
    EpollOperate::DeleteFd(epoll_fd, sock_fd);
    HttpConn::m_user_cnt--;
#ifdef ENABLE_LOG_1
    std::cout << "close fd : " << sock_fd << std::endl;
#endif
}

Server::Server()
    : m_stop_server(false),
      m_port(-1),
      m_server_fd(-1),
      m_epoll_fd(-1),
      m_events(kMaxEventNum),
      m_clients(kMaxFDNum),
      m_pool(nullptr),
      m_timer_fd(-1) {}

Server::~Server() {
    close(m_server_fd);
    close(m_epoll_fd);
    close(m_timer_fd);
}

void Server::init(uint16_t port) {
    m_port = port;
}

void Server::start() {
    InitThreadPool();
    InitTimer();
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

    EpollOperate::AddFd(m_epoll_fd, m_server_fd, false);
    EpollOperate::AddFd(m_epoll_fd, m_timer_fd, false);
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
#ifdef ENABLE_LOG_1
                std::cout << "connect to client " << inet_ntoa(client_addr.sin_addr) << ":"
                          << ntohs(client_addr.sin_port) << std::endl;
#endif
                // init the info of new client
                m_clients[client_fd].init(client_fd, client_addr);

                // create timer node, and add it to timer and timer_map
                TimerNode* timer_node  = new TimerNode;
                timer_node->m_epoll_fd = m_epoll_fd;
                timer_node->m_sock_fd  = client_fd;
                timer_node->m_expire   = time(NULL) + kConnectTimeout;
                timer_node->m_callback = TimerCallback;

                m_timer.AddTimer(timer_node);
                m_timer_map[client_fd] = timer_node;
            } else if (cur_fd == m_timer_fd) {  // timer event
                uint64_t expirations;
                if (read(m_timer_fd, &expirations, sizeof(expirations)) == -1) {
                    perror("Failed to read timer");
                    exit(-1);
                }
                m_timer.Tick();
            }
            // EPOLLHUP: client close connection
            // EPOLLRDHUP: client shutdown write
            // EPOLLERR: error
            else if (m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                m_timer.DelTimer(m_timer_map[cur_fd]);
                m_clients[cur_fd].CloseConn();
            } else if (m_events[i].events & EPOLLIN) {  // read event
                if (m_clients[cur_fd].read()) {
#ifdef ENABLE_LOG_1
                    std::cout << "receive data from client "
                              << inet_ntoa(m_clients[cur_fd].GetClientAddr()->sin_addr) << ":"
                              << ntohs(m_clients[cur_fd].GetClientAddr()->sin_port) << std::endl;
#endif
                    m_pool->append(&m_clients[cur_fd]);
                    m_timer_map[cur_fd]->m_expire = time(NULL) + kConnectTimeout;
                    m_timer.UpdataTimer(m_timer_map[cur_fd]);
                } else {
                    m_timer.DelTimer(m_timer_map[cur_fd]);
                    m_clients[cur_fd].CloseConn();
                }
            } else if (m_events[i].events & EPOLLOUT) {  // write event
                if (m_clients[cur_fd].write()) {
#ifdef ENABLE_LOG_1
                    std::cout << "send data to client "
                              << inet_ntoa(m_clients[cur_fd].GetClientAddr()->sin_addr) << ":"
                              << ntohs(m_clients[cur_fd].GetClientAddr()->sin_port) << std::endl;
#endif
                    m_timer_map[cur_fd]->m_expire = time(NULL) + kConnectTimeout;
                    m_timer.UpdataTimer(m_timer_map[cur_fd]);
                } else {
                    m_timer.DelTimer(m_timer_map[cur_fd]);
                    m_clients[cur_fd].CloseConn();
                }
            } else {
                perror("Unexpected event");
            }
        }
    }
}

void Server::InitThreadPool() {
    m_pool = std::make_shared<ThreadPool<HttpConn>>(kThreadNum, kMaxRequests);
    m_pool->init();
}

void Server::InitTimer() {
    // create timer
    m_timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
    if (m_timer_fd == -1) {
        perror("Failed to create timer");
        exit(-1);
    }

    // set timer timeout
    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec     = kTimerInterval;  // first timeout
    timer_spec.it_value.tv_nsec    = 0;
    timer_spec.it_interval.tv_sec  = kTimerInterval;  // interval between timeouts
    timer_spec.it_interval.tv_nsec = 0;

    if (timerfd_settime(m_timer_fd, 0, &timer_spec, nullptr) == -1) {
        perror("Failed to set timer");
        exit(-1);
    }
}