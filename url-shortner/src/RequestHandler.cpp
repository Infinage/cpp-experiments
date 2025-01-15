#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

#include "../include/RequestHandler.hpp"
#include "../include/HashUtils.hpp"

// Init static variable
const std::regex RequestHandler::URLRegex {R"((http(s)?:\/\/.)?(www\.)?[-a-zA-Z0-9@:%._\+~#=]{2,256}\.[a-z]{2,6}\b([-a-zA-Z0-9@:%_\+.~#?&//=]*))"};
const std::regex RequestHandler::ProtocolRegex {R"(^(http(s)?:\/\/)\S+)"};
std::size_t RequestHandler::Counter {0};

RequestHandler::RequestHandler(const std::string &ip, const int port):
    serverIP(ip), serverPort(port) {};

std::pair<unsigned short, std::string> RequestHandler::validatePostBody(std::string &body) {
    int keyCount {0};
    char quote, brace {'\0'};
    std::string key, value;
    for (std::size_t idx {0}; idx < body.size(); idx++) {
        char ch {body.at(idx)};
        if (ch == '{' || ch == '}') {
            if ((!brace && ch == '}') || brace == ch)
                return {400, "Invalid JSON"};
            else
                brace = ch;
        } else if (ch == '\'' || ch == '"') {
            quote = ch;
            std::string acc;
            while (++idx < body.size() && body[idx] != quote) {
                acc += body.at(idx);
                if (body.at(idx) == '\\')
                    acc += body.at(++idx);
            }

            if (acc.empty() || !value.empty()) 
                return {400, "Empty String"};
            else if (key.empty())
                key = acc;
            else
                value = acc;
        } else if (ch == ':') {
            keyCount++;
        }
    }

    if (keyCount != 1) 
        return {400, "Only 1 key allowed: 'url'"};
    else if (key != "url") 
        return {400,  "Only 1 key allowed: 'url'"};
    else if (value.empty()) 
        return {400, "Empty URL"};
    else if (brace != '}') 
        return {400, "Invalid JSON"};
    else if (!std::regex_match(value, URLRegex))
        return {400, "Invalid URL"};
    else 
        return {200, value};
}

std::string RequestHandler::shortenURL(std::string &longURL) {
    if (cache.find(longURL) == cache.end()) {
        std::size_t idx {Counter++};
        std::string shortURL {encryptSizeT(idx, "secret")};
        revCache[shortURL] = longURL;
        cache[longURL] = shortURL;
    }

    return cache[longURL];
}

std::string RequestHandler::extractRequestURL(const std::string &buffer, const std::string &requestTypeStr) {
    std::size_t reqTypeLen {requestTypeStr.size()};
    std::size_t URLStart {reqTypeLen + 2}, URLEnd {buffer.find(' ', reqTypeLen + 2)};
    return URLEnd == std::string::npos? "": buffer.substr(URLStart, URLEnd - URLStart);
}

std::string RequestHandler::readFile(const std::string &fpath) {
    std::ifstream ifs {fpath};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::string RequestHandler::processRequest(const std::string &buffer) {
    // Prepare string for response
    std::string responseHeaders {"Content-Type: application/json"};
    unsigned short responseCode {200};
    std::string responseBody;

    // Accept POST, GET, DELETE
    std::size_t pos {buffer.find("\r\n\r\n")};
    if (pos == std::string::npos) {
        responseCode = 400;
        responseBody = "\"Invalid HTTP format.\"";
    } 

    else if (buffer.starts_with("GET")) {
        std::string shortURL {extractRequestURL(buffer, "GET")};
        if (shortURL.empty()) {
            responseHeaders = "Content-Type: text/html";
            responseBody = readFile("static/index.html");
        } else if (shortURL == "ping") {
            responseBody = "{\"count\": " + std::to_string(cache.size()) + "}"; 
        } else if (shortURL.starts_with("static/")) {
            bool isValidPath {shortURL.find("..") == std::string::npos && std::filesystem::is_regular_file(shortURL)};
            if (!isValidPath) {
                responseBody = "\"Not a valid file path.\"";
                responseCode = 404;
            } else {
                responseBody = readFile(shortURL);
                if (shortURL.ends_with(".css"))
                    responseHeaders = "Content-Type: text/css";
                else if (shortURL.ends_with(".js"))
                    responseHeaders = "Content-Type: text/javascript";
                else
                    responseHeaders = "Content-Type: text/html";
            }
        } else if (revCache.find(shortURL) != revCache.end()) {
            responseCode = 302;
            responseHeaders += "\r\nlocation: " + revCache[shortURL];
        } else {
            responseCode = 404;
            responseBody = "\"URL not found\"";
        }
    } 

    else if (buffer.starts_with("POST")) {
        // Read the first '\r\n\r\n'
        std::string postBody {buffer.substr(pos + 4)};

        // Validate post body - only {'url': <long_url>} is supported
        std::pair<unsigned short, std::string> parsedPostBody {validatePostBody(postBody)};
        responseCode = parsedPostBody.first;
        if (responseCode == 200) {
            std::string longURL {parsedPostBody.second};
            if (!std::regex_match(longURL, ProtocolRegex))
                longURL = "http://" + longURL;
            if (cache.find(longURL) == cache.end())
                responseCode = 201;
            std::string shortURL {shortenURL(longURL)};
            responseBody = std::format(
                R"({{"key": "{}", "long_url": "{}", "short_url": "{}:{}/{}"}})", 
                shortURL, parsedPostBody.second, serverIP, serverPort, shortURL
            );
        } else {
            responseBody = '"' + parsedPostBody.second + '"';
        }
    } 

    else if (buffer.starts_with("DELETE")) {
        std::string shortURL {extractRequestURL(buffer, "DELETE")};
        if (shortURL.empty()) {
            responseCode = 400;
            responseBody = "\"Invalid request\"";
        } else if (revCache.find(shortURL) != revCache.end()) {
            std::string longURL {revCache[shortURL]};
            cache.erase(longURL);
            revCache.erase(shortURL);
        } else {
            responseCode = 404;
            responseBody = "\"URL Not found\"";
        }
    } 

    else {
        responseCode = 405;
        responseBody = "\"Request method unknown or is not supported\"";
    }

    // Craft the response
    responseBody += "\r\n";
    std::string response {"HTTP/1.1 " + std::to_string(responseCode)}; 
    response += "\r\n" + responseHeaders;
    response += "\r\nContent-Length: " + std::to_string(responseBody.size()) + "\r\n\r\n";
    response += responseBody;

    // Return the response as string
    return response;
}
