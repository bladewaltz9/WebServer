#include <iostream>

#include "server.h"

int main() {
    uint16_t port = 10000;

    Server server;

    server.init(port);
    server.start();

    return 0;
}