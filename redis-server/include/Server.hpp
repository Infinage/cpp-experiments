#pragma once

#include <atomic>
#include <stack>

#include "Cache.hpp"

namespace Redis {

    /* --------------- SERVER CLASS METHOD IMPLEMENTATIONS --------------- */

    class Server {
        private:
            using SOCKET_INFO_VALUE_TYPE = std::tuple<short, std::string, std::size_t, std::stack<std::tuple<char, std::size_t>>>;

            Cache cache;
            int server_fd;
            static std::unordered_map<int, SOCKET_INFO_VALUE_TYPE> socketInfo;
            static std::atomic<bool> serverRunning;

            // Helpers - Static
            static bool readRequest(int client_fd, std::string &request);
            static bool sendResponse(int client_fd, std::string &response, std::size_t &currPos);
            static void closeSockets(int);

            // Init Server socket
            int initServer(const char* serverIP, const int serverPort, int serverBacklog);

            // Create poll request from socketInfo
            std::vector<pollfd> createPollInput();

            // Private Handlers
            std::string handleCommandPing(std::vector<std::string> &args);
            std::string handleCommandEcho(std::vector<std::string> &args);
            std::string handleCommandSet(std::vector<std::string> &args);
            std::string handleCommandGet(std::vector<std::string> &args);
            std::string handleCommandExists(std::vector<std::string> &args);
            std::string handleCommandDel(std::vector<std::string> &args);
            std::string handleCommandLAdd(std::vector<std::string> &args, long by);
            std::string handleCommandTTL(std::vector<std::string> &args);
            std::string handleCommandLRange(std::vector<std::string> &args);
            std::string handleCommandPush(std::vector<std::string> &args, bool pushBack);
            std::string handleCommandLLen(std::vector<std::string> &args);
            std::string handleCommandSave(std::vector<std::string> &args, bool background = false);
            std::string handleRequest(std::string &request);

        public:
            Server(const char* serverIP, const int serverPort, int serverBacklog, const char* dbFP);
            int getServerFD() const;
            void run();
    };
}
