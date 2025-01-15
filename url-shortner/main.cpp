#include "include/Server.hpp"

int main() {

    constexpr const char* SERVER_IP {"127.0.0.1"};
    constexpr int PORT {8080};
    constexpr int SERVER_BACKLOG {10};

    Server server(SERVER_IP, PORT, SERVER_BACKLOG);
    server.run();
}
