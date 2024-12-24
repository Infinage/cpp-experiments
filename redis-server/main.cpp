#include "include/Server.hpp"

int main() {

    constexpr const char* SERVER_IP {"0.0.0.0"};
    constexpr const char* DB_SAVE_FP {"dump.rdb"};
    constexpr int PORT {6379};
    constexpr int SOCKET_BACKLOG {10};

    // Init redis server and start loop
    Redis::Server server(SERVER_IP, PORT, SOCKET_BACKLOG, DB_SAVE_FP);
    server.run();

    return 0;
}
