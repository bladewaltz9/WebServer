#include <iostream>

#include "Server.h"

int main() {
    uint16_t port = 10000;

    Server server;

    server.init(port);

    server.eventListen();

    server.eventLoopHandle();

    return 0;
}