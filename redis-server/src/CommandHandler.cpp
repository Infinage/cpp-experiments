#include <charconv>
#include <filesystem>
#include <iostream>
#include <sstream>

#include "../include/CommandHandler.hpp"
#include "../include/Utils.hpp"

namespace Redis {
    CommandHandler::CommandHandler(const char* dbFP) {
        // Try loading data on startup
        if (!std::filesystem::exists(dbFP))
            std::cout << "No existing save found. Creating a new instance.\n";
        else if (cache.load(dbFP))
            std::cout << "Load successful.\n";
        else
            std::cout << "Restore failed. Creating a new instance.\n";
    }

    std::string CommandHandler::handleCommandPing(std::vector<std::string> &args) {
        if (args.size() == 1) {
            return Redis::PlainRedisNode("PONG").serialize();
        } else if (args.size() == 2) {
            return Redis::VariantRedisNode(args.back()).serialize();
        } else {
            return Redis::PlainRedisNode("Wrong number of arguments for 'ping' command", false).serialize();
        }
    }

    std::string CommandHandler::handleCommandEcho(std::vector<std::string> &args) {
        if (args.size() == 2) {
            return Redis::VariantRedisNode(args.back()).serialize();
        } else {
            return Redis::PlainRedisNode("Wrong number of arguments for 'echo' command", false).serialize();
        }
    }

    std::string CommandHandler::handleCommandSet(std::vector<std::string> &args) {
        if (args.size() >= 3) {
            // Store the value at specified key
            std::string &key {args[1]};
            std::string &value {args[2]};
            cache.setValue(key, std::make_shared<Redis::VariantRedisNode>(value));

            // Set the expiry
            if (args.size() >= 5) {
                for (std::size_t i {3}; i < args.size() - 1; i++) {
                    // Extract this and the next
                    std::string &expiryCode {args[i]};
                    std::string &expiry {args[i + 1]};

                    // To lower case
                    Redis::lower(expiryCode);

                    // Check if code is valid and expiry code
                    bool isValidCode {expiryCode == "ex" || expiryCode == "exat" || expiryCode == "px" || expiryCode == "pxat"}, 
                         isValidExpiry {isValidCode && Redis::allDigitsUnsigned(expiry.cbegin(), expiry.cend())};

                    if (!isValidCode)        { continue; }
                    else if (!isValidExpiry) { return Redis::PlainRedisNode("Invalid syntax", false).serialize(); }
                    else if (expiryCode ==   "ex") {    cache.setTTLS(key, std::stoul(expiry)); break; }
                    else if (expiryCode ==   "px") {   cache.setTTLMS(key, std::stoul(expiry)); break; }
                    else if (expiryCode == "exat") {  cache.setTTLSAt(key, std::stoul(expiry)); break; }
                    else if (expiryCode == "pxat") { cache.setTTLMSAt(key, std::stoul(expiry)); break; }
                }
            }

            // Send response
            return Redis::PlainRedisNode("OK").serialize();

        } else {

            return Redis::PlainRedisNode("Wrong number of arguments for 'set' command", false).serialize();
        }
    }

    std::string CommandHandler::handleCommandGet(std::vector<std::string> &args) {
        if (args.size() == 2) {
            std::string &key {args[1]};
            return cache.getValue(key)->serialize();
        } else {
            return Redis::PlainRedisNode("Wrong number of arguments for 'get' command", false).serialize();
        }
    }
    
    std::string CommandHandler::handleCommandExists(std::vector<std::string> &args) {
        long result {0};
        for (std::size_t i{1}; i < args.size(); i++) {
            std::string &arg {args[i]};
            if (cache.exists(arg)) {
                result++;
            }
        }
        return Redis::VariantRedisNode(result).serialize();
    }

    std::string CommandHandler::handleCommandDel(std::vector<std::string> &args) {
        long result {0};
        for (std::size_t i{1}; i < args.size(); i++) {
            std::string &arg {args[i]};
            if (cache.exists(arg)) {
                result += !cache.expired(arg);
                cache.erase(arg); 
            }
        }
        return Redis::VariantRedisNode(result).serialize();
    }

    std::string CommandHandler::handleCommandLAdd(std::vector<std::string> &args, long by) {
        if (args.size() == 2) {
            std::string &key {args[1]};
            if (!cache.exists(key) || cache.expired(key)) {
                cache.setValue(key, std::make_shared<Redis::VariantRedisNode>(std::to_string(by)));
                return cache.getValue(key)->serialize();
            } else {
                const std::string &value {cache.getValue(key)->cast<Redis::VariantRedisNode>()->str()};
                if (Redis::allDigitsSigned(value.cbegin(), value.cend())) {
                    cache.setValue(key, std::make_shared<Redis::VariantRedisNode>(std::to_string(std::stol(value) + by)));
                    return cache.getValue(key)->serialize();
                } else {
                    return Redis::PlainRedisNode("value is not an integer or out of range", false).serialize();
                }
            }
        } else {
            return Redis::PlainRedisNode("Wrong number of arguments for 'incr' command", false).serialize();
        }
    }

    std::string CommandHandler::handleCommandTTL(std::vector<std::string> &args) {
       if (args.size() == 2) {
            std::string &key {args[1]};
            long ttlVal {cache.getTTL(key)};
            return Redis::VariantRedisNode(ttlVal > 0? ttlVal / 1000: ttlVal).serialize();
       } else {
            return Redis::PlainRedisNode("Wrong number of arguments for 'ttl' command", false).serialize();
       }
    }

    std::string CommandHandler::handleCommandLRange(std::vector<std::string> &args) {
        if (args.size() == 4) {
            long left, right;
            std::string &key {args[1]};
            std::from_chars_result parseResult1 {std::from_chars(args[2].c_str(), args[2].c_str() + args[2].size(), left)};
            std::from_chars_result parseResult2 {std::from_chars(args[3].c_str(), args[2].c_str() + args[3].size(), right)};
            if (parseResult1.ec != std::errc() || parseResult2.ec != std::errc()) { 
                return Redis::PlainRedisNode("Value is not an integer or out of range", false).serialize();
            } else if (!cache.exists(key) || cache.expired(key)) {
                return Redis::AggregateRedisNode().serialize();
            } else if (cache.getValue(key)->getType() != Redis::NODE_TYPE::AGGREGATE) {
                return Redis::PlainRedisNode("WRONGTYPE Operation against a key holding the wrong kind of value", false).serialize();
            } else {
                std::shared_ptr<Redis::AggregateRedisNode> value {cache.getValue(key)->cast<Redis::AggregateRedisNode>()};
                long N{static_cast<long>(value->size())};
                if (left < 0) left = N + left;
                if (right < 0) right = N + right;
                left = std::max(left, 0L); right = std::min(right, N - 1);
                std::ostringstream oss;
                oss << "*" << (left<= right? right - left + 1: 0) << Redis::SEP;
                for (long curr {left}; curr <= right; curr++)
                    oss << (*value)[curr]->serialize();
                return oss.str();
            }
        } else {
            return Redis::PlainRedisNode("ERR wrong number of arguments for command", false).serialize();
        }
    }

    std::string CommandHandler::handleCommandPush(std::vector<std::string> &args, bool pushBack) {
        if (args.size() >= 3) {
            std::string &key {args[1]};
            bool exists {cache.exists(key) && !cache.expired(key)}, 
                 isAggNode {exists && cache.getValue(key)->getType() == Redis::NODE_TYPE::AGGREGATE};

            // Exists but of wrong data type
            if (exists && !isAggNode)
                return Redis::PlainRedisNode("WRONGTYPE Operation against a key holding the wrong kind of value", false).serialize();

            // If it doesn't exist, create new
            if (!exists)
                cache.setValue(key, std::make_shared<Redis::AggregateRedisNode>());

            // Start inserting into the aggNode
            long result {0};
            std::shared_ptr<Redis::AggregateRedisNode> aggNode {cache.getValue(key)->cast<Redis::AggregateRedisNode>()};
            for (std::size_t i{2}; i < args.size(); i++) {
                if (pushBack) aggNode->push_back(std::make_shared<Redis::VariantRedisNode>(args[i])); 
                else aggNode->push_front(std::make_shared<Redis::VariantRedisNode>(args[i])); 
                result++;
            }
            return Redis::VariantRedisNode(result).serialize();

        } else {
            return Redis::PlainRedisNode("ERR wrong number of arguments for command", false).serialize();
        }
    }

    std::string CommandHandler::handleCommandLLen(std::vector<std::string> &args) {
        if (args.size() == 2) {
            std::string &key {args[1]};
            if (!cache.exists(key) || cache.expired(key)) {
                return Redis::VariantRedisNode(0).serialize();
            } else if (cache.getValue(key)->getType() != Redis::NODE_TYPE::AGGREGATE) {
                return Redis::PlainRedisNode("WRONGTYPE Operation against a key holding the wrong kind of value", false).serialize();
            } else {
                std::size_t N {cache.getValue(key)->cast<Redis::AggregateRedisNode>()->size()};
                return Redis::VariantRedisNode(static_cast<long>(N)).serialize();
            }
        } else {
            return Redis::PlainRedisNode("ERR wrong number of arguments for command", false).serialize();
        }
    }

    std::string CommandHandler::handleCommandSave(std::vector<std::string> &args, bool background) {
        if (args.size() != 1) {
            return Redis::PlainRedisNode("ERR wrong number of arguments for command", false).serialize();
        } else if (background) {
            int pid {fork()};
            if (pid == -1) {
                return Redis::PlainRedisNode("Save failed", false).serialize();
            } else if (pid == 0) {
                bool status {cache.save("dump.rdb")}; std::exit(!status);
            } else {
                return Redis::PlainRedisNode("OK").serialize();
            }
        } else {
            if (cache.save("dump.rdb")) return Redis::PlainRedisNode("OK").serialize();
            else return Redis::PlainRedisNode("Save failed", false).serialize();
        }
    }

    std::string CommandHandler::handleRequest(std::string &request) {
        // Process the request
        std::shared_ptr<Redis::RedisNode> reqNode {Redis::RedisNode::deserialize(request)};
        std::vector<std::string> args; std::string command;
        if (reqNode->getType() != Redis::NODE_TYPE::AGGREGATE) {
            command = "missing";
        } else {
            args = reqNode->cast<Redis::AggregateRedisNode>()->vector();
            command = args[0];
        }

        // Convert to lower case
        Redis::lower(command);

        // Prepare a suitable response
        std::string serializedResponse;
        if (command == "ping")
            serializedResponse = handleCommandPing(args);
        else if (command == "echo")
            serializedResponse = handleCommandEcho(args);
        else if (command == "set")
            serializedResponse = handleCommandSet(args);
        else if (command == "get")
            serializedResponse = handleCommandGet(args);
        else if (command == "exists")
            serializedResponse = handleCommandExists(args);
        else if (command == "del")
            serializedResponse = handleCommandDel(args);
        else if (command == "incr")
            serializedResponse = handleCommandLAdd(args, 1);
        else if (command == "decr")
            serializedResponse = handleCommandLAdd(args, -1);
        else if (command == "ttl")
            serializedResponse = handleCommandTTL(args);
        else if (command == "lrange")
            serializedResponse = handleCommandLRange(args);
        else if (command == "lpush")
            serializedResponse = handleCommandPush(args, false);
        else if (command == "rpush")
            serializedResponse = handleCommandPush(args, true);
        else if (command == "llen")
            serializedResponse = handleCommandLLen(args);
        else if (command == "save")
            serializedResponse = handleCommandSave(args);
        else if (command == "bgsave")
            serializedResponse = handleCommandSave(args, true);
        else
            serializedResponse = Redis::PlainRedisNode("Not supported", false).serialize();

        // Return serialized response
        return serializedResponse;
    }
}
