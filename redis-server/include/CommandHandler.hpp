#pragma once

#include "Cache.hpp"

namespace Redis {
    class CommandHandler {
        private:
            Cache cache;
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

        public:
            CommandHandler(const char* dbFP);
            std::string handleRequest(std::string &request);
    };
}
