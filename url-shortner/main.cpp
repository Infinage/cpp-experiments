#include "include/Server.hpp"

int main() {
    const char* SERVER_IP {"0.0.0.0"};
    int PORT {8080}, SERVER_BACKLOG {10};
    Server server(SERVER_IP, PORT, SERVER_BACKLOG);
    server.run();
}
