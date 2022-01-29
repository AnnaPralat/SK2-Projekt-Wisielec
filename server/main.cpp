#include "server.h"

Server* server;

void signalHandler(int signal) {
    delete server;
    exit(0);
}

int main(int argc, char** argv) {
    // exit with CTRL + C
    signal(SIGINT, signalHandler);
    // prevent dead sockets from throwing pipe errors on write
    signal(SIGPIPE, SIG_IGN);

    // init server
    server = new Server();

    server->loop();
}
