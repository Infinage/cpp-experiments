#include <atomic>
#include <unordered_map>

#include "include/Server.hpp"

// Init static variables for the server
std::atomic<bool> Redis::Server::serverRunning {true};
std::unordered_map<int, Redis::Server::SOCKET_INFO_VALUE_TYPE> Redis::Server::socketInfo{};

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
