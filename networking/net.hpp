/*
 * TODO:
 * - HttpRequest and HttpResponse - support duplicate keys use multi map?
 * - HttpRequest parse from string for server side http handling
 * - Write unit test cases
 * - Modify httpserver to use this module
 * - getaddrinfo() to iterate through results instead of returning first one
 * - Use C++ modules
 * - Windows support?
 */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../json-parser/json.hpp"

namespace net {
    // Abstracing SOCK_STREAM, SOCK_DGRAM; PF_INET, PF_INET6
    enum class IP { V4, V6 };
    enum class SOCKTYPE { TCP, UDP };

    // Custom exception class to automatically include the system error
    class SocketError: public std::runtime_error {
    public:
        SocketError(const std::string &msg, const std::string &sysMsg = "")
        : std::runtime_error{msg + ": " + (sysMsg.empty()? std::strerror(errno): sysMsg)} {}
    };

    class SSLSocketError : public std::runtime_error {
    public:
        SSLSocketError(const std::string &msg)
        : std::runtime_error{composeMessage(msg)} {}

    private:
        static std::string composeMessage(const std::string &msg) {
            std::string fullMsg = msg;

            // Append system error if errno is set
            if (errno) fullMsg += ": " + std::string{std::strerror(errno)};

            // Collect all OpenSSL errors currently on the queue
            unsigned long err;
            while ((err = ERR_get_error()) != 0) {
                char buf[256];
                ERR_error_string_n(err, buf, sizeof(buf));
                fullMsg += "\n  OpenSSL: ";
                fullMsg += buf;
            }

            return fullMsg;
        }
    };

    namespace utils {
        [[nodiscard]] inline std::string trimStr(std::string_view str) {
            auto first = str.find_first_not_of(' ');
            if (first == std::string::npos) return "";
            auto last = str.find_last_not_of(' ');
            return {str.begin() + first, str.begin() + last + 1};
        }

        [[nodiscard]] inline std::string toLower(std::string_view str) {
            std::string res(str.size(), '\0'); 
            std::ranges::transform(str, res.begin(), [](unsigned char ch) { return std::tolower(ch); }); 
            return res;
        }

        // Given a hostname such as 'google.com' and an optional 'port/service' 
        // resolves to an IP addr; Service eg: 'http', 'https', etc
        [[nodiscard]] inline std::string resolveHostname(std::string_view hostname, const char *service = nullptr,
            SOCKTYPE sockType = SOCKTYPE::TCP, IP ipType = IP::V4)
        {
            int domain {ipType == IP::V4? PF_INET: PF_INET6};
            int type {sockType == SOCKTYPE::TCP? SOCK_STREAM: SOCK_DGRAM};
            addrinfo hints{}, *res {}; hints.ai_family = domain; hints.ai_socktype = type;
            if (int status = getaddrinfo(hostname.data(), service, &hints, &res); status != 0)
                throw SocketError{"Failed to resolve hostname", gai_strerror(status)};

            // Ensure res is freed always by wrapping inside a smart pointer
            std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resGaurd {res, freeaddrinfo};

            unsigned int ipLen {static_cast<unsigned int>(ipType == IP::V4? INET_ADDRSTRLEN: INET6_ADDRSTRLEN)};
            std::string ipStr(ipLen, '\0'); const char *ret;
            if (ipType == IP::V4) {
                auto *addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
                ret = inet_ntop(AF_INET, &addr->sin_addr, ipStr.data(), ipLen);
            } else {
                auto *addr6 = reinterpret_cast<sockaddr_in6*>(res->ai_addr);
                ret = inet_ntop(AF_INET6, &addr6->sin6_addr, ipStr.data(), ipLen);
            }

            if (!ret) throw SocketError{"Failed to convert address to string"};
            ipStr.resize(std::strlen(ipStr.data()));
            return ipStr;
        }

        [[nodiscard]] inline std::string urlEncode(std::string_view str, 
            bool mapSpaceToPlus = true) 
        {
            std::ostringstream oss; oss.fill('0');
            for (char ch: str) {
                bool isUnreserved {
                    (ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '-' || ch == '.' || ch == '_' || ch == '~'
                };

                if (ch == ' ' && mapSpaceToPlus) oss << '+';
                else if (isUnreserved) oss << ch;
                else {
                    oss << '%' << std::hex << std::uppercase << std::setw(2) 
                        << static_cast<int>(static_cast<unsigned char>(ch))
                        << std::dec << std::nouppercase;
                }
            }
            return oss.str();
        }

        [[nodiscard]] inline std::string 
        getPathWithParams(const std::string &path, const std::vector<std::pair<std::string, std::string>> &params) {
            if (params.empty()) return path;
            std::ostringstream oss; oss << path << '?';
            for (const auto &kv: params) oss << kv.first << "=" << kv.second << '&';
            std::string res {oss.str()}; res.pop_back();
            return res;
        }

        [[nodiscard]] inline 
        std::tuple<std::string, std::unordered_map<std::string, std::string>, std::string> 
        parseHttpString(std::string_view raw) {
            // Extract request / status line
            std::size_t pos1 {raw.find("\r\n")}, pos2;
            if (pos1 == std::string::npos) throw std::runtime_error("Invalid Http string");
            std::string firstLine {trimStr(raw.substr(0, pos1))};

            // Seperate the body and headers
            pos2 = raw.find("\r\n\r\n", pos1 + 2);
            if (pos2 == std::string::npos) throw std::runtime_error("Invalid Http string");
            std::string body {raw.substr(pos2 + 4)};
            std::string_view headerRaw {
                raw.begin() + static_cast<long>(pos1) + 2, 
                raw.begin() + static_cast<long>(pos2) + 2
            };

            // Parse the headers
            std::unordered_map<std::string, std::string> headers;
            while (!headerRaw.empty()) {
                pos1 = headerRaw.find(':');
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid Http header");

                pos2 = headerRaw.find("\r\n", pos1 + 1);
                if (pos2 == std::string::npos) throw std::runtime_error("Invalid Http header");

                // Header keys are always in lower case
                std::string key {toLower(headerRaw.substr(0, pos1))}; 
                std::string value {headerRaw.substr(pos1 + 1, pos2 - pos1 - 1)};
                headers.insert({trimStr(key), trimStr(value)});

                headerRaw = headerRaw.substr(pos2 + 2);
            }

            return std::make_tuple(firstLine, headers, body);
        }
    };

    class Socket {
        protected:
            int _fd {-1};
            bool _blocking {false};
            SOCKTYPE _sockType {SOCKTYPE::TCP};
            IP _ipType {IP::V4};
            unsigned int _sockSize {sizeof(sockaddr_in)};

            [[nodiscard]] sockaddr_storage
            Sockaddr(std::string_view ip, std::uint16_t port) const {
                sockaddr_storage storage;

                if (_ipType == IP::V4) {
                    sockaddr_in *addr4 {reinterpret_cast<sockaddr_in*>(&storage)};
                    addr4->sin_port = htons(port); addr4->sin_family = AF_INET;
                    if (inet_pton(AF_INET, ip.data(), &addr4->sin_addr) <= 0)
                        throw SocketError{"Invalid IPV4 Address"};
                } else {
                    sockaddr_in6 *addr6 {reinterpret_cast<sockaddr_in6*>(&storage)};
                    addr6->sin6_port = htons(port); addr6->sin6_family = AF_INET6;
                    if (inet_pton(AF_INET6, ip.data(), &addr6->sin6_addr) <= 0)
                        throw SocketError{"Invalid IPV6 Address"};
                }

                return storage;
            }

            void extractSockaddr(sockaddr_storage &storage, std::string &host, 
                std::uint16_t &port) const 
            {
                const char *ret;
                if (_ipType == IP::V4) {
                    sockaddr_in *addr4 {reinterpret_cast<sockaddr_in*>(&storage)};
                    port = ntohs(addr4->sin_port); host.resize(INET_ADDRSTRLEN);
                    ret = inet_ntop(AF_INET, &addr4->sin_addr, host.data(), INET_ADDRSTRLEN);
                } else {
                    sockaddr_in6 *addr6 {reinterpret_cast<sockaddr_in6*>(&storage)};
                    port = ntohs(addr6->sin6_port); host.resize(INET6_ADDRSTRLEN);
                    ret = inet_ntop(AF_INET6, &addr6->sin6_addr, host.data(), INET6_ADDRSTRLEN);
                }

                if (ret == nullptr)
                    throw SocketError{"Failed to convert IP address to string in extractSockaddr"};

                // Trim to actual length
                host.resize(std::strlen(host.data()));
            }

        public:
            // Force close a socket if required
            void close() { if (_fd != -1) { ::close(_fd); _fd = -1; } }

            // This ctor is to wrap a socket created from accept into RAII
            explicit Socket(int _fd, SOCKTYPE _sockType, IP _ipType): 
                _fd{_fd}, _blocking{false}, _sockType{_sockType}, _ipType{_ipType} 
            {}

            ~Socket() { close(); }

            int fd() const { return _fd; }
            bool isBlocking() const { return _blocking; }
            SOCKTYPE socketType() const { return _sockType; }
            IP ipType() const { return _ipType; }
            bool ok() const { return _fd != -1; }

            // Disable copy ctor and assignment
            Socket(const Socket&) = delete;
            Socket &operator=(const Socket&) = delete;

            // Swap function for move assignment / ctors
            void swap(Socket &other) noexcept {
                using std::swap; swap(_fd, other._fd); 
                swap(_blocking, other._blocking);
                swap(_sockType, other._sockType);
                swap(_ipType, other._ipType);
                swap(_sockSize, other._sockSize);
            }

            // Implement move constructor
            Socket(Socket &&other) noexcept: 
                _fd{std::exchange(other._fd, -1)}, 
                _blocking{std::exchange(other._blocking, false)},
                _sockType{std::exchange(other._sockType, SOCKTYPE::TCP)},
                _ipType{std::exchange(other._ipType, IP::V4)},
                _sockSize{std::exchange(other._sockSize, sizeof(sockaddr_in))}
            {}

            // Implement move assignment operator
            Socket &operator=(Socket &&other) noexcept {
                Socket{std::move(other)}.swap(*this);
                return *this;
            }

            // Default ctor
            Socket(SOCKTYPE sockType = SOCKTYPE::TCP, IP ipType = IP::V4):
                _sockType{sockType}, _ipType{ipType}, 
                _sockSize{static_cast<unsigned int>(ipType == IP::V4? sizeof(sockaddr_in): sizeof(sockaddr_in6))}
            {
                int domain {_ipType == IP::V4? PF_INET: PF_INET6};
                int type {_sockType == SOCKTYPE::TCP? SOCK_STREAM: SOCK_DGRAM};
                _fd = socket(domain, type, 0);
                if (_fd == -1) throw SocketError{"Error creating socket object"};

                // Reuse socket address
                int opt {1};
                if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
                    throw SocketError{"Error setting opt 'SO_REUSEADDDR'"};
            }

            // Set socket as blocking / nonblocking
            void setNonBlocking(bool enable = true) {
                int fcntl_flags {fcntl(_fd, F_GETFL, 0)}; // Get currently set flags
                int fcntl_flags_modified = enable? fcntl_flags | O_NONBLOCK: fcntl_flags & ~O_NONBLOCK;
                if(fcntl_flags == -1 || fcntl(_fd, F_SETFL, fcntl_flags_modified) == -1)
                    throw SocketError{"Error setting client socket to non blocking mode to: " 
                            + std::to_string(enable)};
                _blocking = !enable;
            }

            // Set receive / send timeouts
            void setTimeout(long rcvTimeoutSec, long sndTimeoutSec) {
                struct timeval rcvTimeout {.tv_sec = rcvTimeoutSec, .tv_usec = 0};
                struct timeval sndTimeout {.tv_sec = sndTimeoutSec, .tv_usec = 0};
                if (::setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, &rcvTimeout, sizeof(rcvTimeout)) < 0 || 
                    ::setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, &sndTimeout, sizeof(sndTimeout)) < 0)
                    throw SocketError{"Failed to set socket timeouts"};
            }

            void bind(std::string_view serverIp, std::uint16_t port) {
                sockaddr_storage serverAddr {Sockaddr(serverIp, port)};
                int bindStatus {::bind(_fd, reinterpret_cast<sockaddr*>(&serverAddr), _sockSize)};
                if (bindStatus == -1) throw SocketError{"Error binding to the socket"};
            }

            void listen(unsigned short backlog = 5) {
                int listenStatus {::listen(_fd, backlog)};
                if (listenStatus == -1) throw SocketError{"Error listening on socket"};
            }

            [[nodiscard]] Socket accept() {
                int clientSocket {::accept(_fd, nullptr, nullptr)};
                if (clientSocket == -1) throw SocketError{"Failed to accept an incomming connection"};
                return Socket{clientSocket, _sockType, _ipType};
            }

            [[nodiscard]] Socket accept(std::string &host, std::uint16_t &port) {
                sockaddr_storage hostAddr {}; socklen_t hostAddrLen{_sockSize};
                int clientSocket {::accept(_fd, reinterpret_cast<sockaddr*>(&hostAddr), &hostAddrLen)};
                if (clientSocket == -1) throw SocketError{"Failed to accept an incomming connection"};
                extractSockaddr(hostAddr, host, port);
                return Socket{clientSocket, _sockType, _ipType};
            }

            void connect(std::string_view serverIp, std::uint16_t port) {
                sockaddr_storage serverAddr {Sockaddr(serverIp, port)};
                int connectStatus {::connect(_fd, reinterpret_cast<sockaddr*>(&serverAddr), _sockSize)};
                if (connectStatus == -1) throw SocketError{"Error connecting to server"};
            }

            // Send until all of the message is out, for blocking sockets guaranteed to 
            // either throw or have entire thing sent. For non blocking sockets we might
            // have data partially sent, verify againt return bytes
            long sendAll(std::string_view message) {
                long totalSent {};
                while (!message.empty()) {
                    long sentBytes {::send(_fd, message.data(), message.size(), MSG_NOSIGNAL)};
                    if (sentBytes <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) 
                        throw SocketError{"Failed to send"};
                    message.remove_prefix(static_cast<std::size_t>(sentBytes));
                    totalSent += sentBytes;
                }
                return totalSent;
            }

            [[nodiscard]] long send(std::string_view message) {
                long sentBytes {::send(_fd, message.data(), message.size(), MSG_NOSIGNAL)};
                if (sentBytes <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) 
                    throw SocketError{"Failed to send"};
                return sentBytes;
            }

            [[nodiscard]] std::string recv(std::size_t maxBytes = 2048) {
                std::vector<char> buffer(maxBytes);
                long recvBytes {::recv(_fd, buffer.data(), maxBytes, 0)};
                if (recvBytes == 0 || (recvBytes < 0 && errno == ECONNRESET)) close();
                else if (recvBytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    throw SocketError{"Failed to recv"};
                return {buffer.data(), static_cast<std::size_t>(recvBytes)};
            }

            // Recv until connection is closed or non blocking errcode is hit
            // Use with caution for blocking sockets, it will hang until con is closed
            [[nodiscard]] std::string recvAll() {
                std::string message; char buffer[2048] {}; long recvBytes;
                while ((recvBytes = ::recv(_fd, buffer, sizeof(buffer), 0)) > 0)
                    message.append(buffer, static_cast<std::size_t>(recvBytes));
                if (recvBytes == 0 || (recvBytes < 0 && errno == ECONNRESET)) close();
                else if (recvBytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    throw SocketError{"Failed to recv"};
                return message;
            }

            void sendTo(std::string_view message, std::string_view host, std::uint16_t port) {
                sockaddr_storage hostAddr {Sockaddr(host, port)};
                long sentBytes {::sendto(_fd, message.data(), message.size(), 0, 
                    reinterpret_cast<sockaddr*>(&hostAddr), _sockSize)};
                if (sentBytes <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) 
                    throw SocketError{"Failed to send"};
            }

            [[nodiscard]] std::string recvFrom(std::string &host, std::uint16_t &port, std::size_t maxBytes = 2048) {
                sockaddr_storage hostAddr {}; socklen_t hostAddrLen {_sockSize};
                std::vector<char> buffer(maxBytes); 
                long recvBytes {::recvfrom(_fd, buffer.data(), maxBytes, 0, reinterpret_cast<sockaddr*>(&hostAddr), &hostAddrLen)};
                if (recvBytes == 0 || (recvBytes < 0 && errno == ECONNRESET)) close();
                else if (recvBytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) 
                    throw SocketError{"Failed to recv"};
                extractSockaddr(hostAddr, host, port);
                return {buffer.data(), static_cast<std::size_t>(recvBytes)};
            }
    };

    class SSLSocket {
        public:
            // Support for creating SSL Socket from `accept`
            SSLSocket(Socket &&socket, SSL_CTX *ctx, SSL *ssl, bool isServer = false): 
                isServer{isServer}, socket{std::move(socket)}, 
                ctx{ctx}, ssl{ssl} 
            {}

            // Default constructor - certPath and keyPath.pem mandatory for SSLSocket Server, optional for client
            SSLSocket(
                bool isServer = false, 
                std::string_view certPath = "", 
                std::string_view keyPath = "",
                IP ipType = IP::V4
            ):
                isServer{isServer}, socket{SOCKTYPE::TCP, ipType},
                ctx{SSL_CTX_new(isServer? TLS_server_method(): TLS_client_method())},
                ssl {nullptr}
            {
                if (!isServer) {
                    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
                    SSL_CTX_set_default_verify_paths(ctx);
                    if (!certPath.empty() && !SSL_CTX_load_verify_locations(ctx, certPath.data(), nullptr))
                        throw SSLSocketError{"Failed to use certificate: " + std::string{certPath}};
                } 

                else {
                    if (certPath.empty() || SSL_CTX_use_certificate_chain_file(ctx, certPath.data()) <= 0)
                        throw SSLSocketError{"Failed to set certificate from path"};
                    if (keyPath.empty() || SSL_CTX_use_PrivateKey_file(ctx, keyPath.data(), SSL_FILETYPE_PEM) <= 0)
                        throw SSLSocketError{"Failed to set pem key from path"};
                }
            }

            // Force close a socket if required
            void close() {
                if (ssl != nullptr) { SSL_shutdown(ssl); SSL_free(ssl); ssl = nullptr; }
                if (ctx != nullptr) { SSL_CTX_free(ctx); ctx = nullptr; }
                socket.close();
            }

            // SSLSocket Destructor
            ~SSLSocket() { close(); }

            // Swap function for move assignment / ctors
            void swap(SSLSocket &other) noexcept {
                using std::swap; 
                swap(isServer, other.isServer);
                swap(socket, other.socket); 
                swap(ctx, other.ctx); 
                swap(ssl, other.ssl);
            }

            // Implement move constructor
            SSLSocket(SSLSocket &&other) noexcept: 
                isServer {std::exchange(other.isServer, false)},
                socket {std::move(other.socket)},
                ctx {std::exchange(other.ctx, nullptr)},
                ssl {std::exchange(other.ssl, nullptr)}
            {}

            // Implement move assignment operator
            SSLSocket &operator=(SSLSocket &&other) noexcept {
                SSLSocket{std::move(other)}.swap(*this);
                return *this;
            }

            // Forward function calls to the underlying socket obj
            bool ok() const { return socket.ok(); }
            int fd() const { return socket.fd(); }
            void bind(std::string_view serverIp, std::uint16_t port) { socket.bind(serverIp, port); }
            void listen(unsigned short backlog = 5) { socket.listen(backlog); }
            void setTimeout(long rcvTimeoutSec, long sndTimeoutSec) { socket.setTimeout(rcvTimeoutSec, sndTimeoutSec); }

            [[nodiscard]] SSLSocket accept(std::string &host, std::uint16_t &port) {
                Socket clientSocket {socket.accept(host, port)};

                // In case of a server, *ssl is always nullptr. Each client 
                // creates and associates a new *ssl for itself for RAII
                SSL *clientSSL {SSL_new(ctx)};
                if (!clientSSL) throw SSLSocketError{"Unable to create SSL Context"};
                SSL_set_fd(clientSSL, clientSocket.fd());
                if (SSL_accept(clientSSL) <= 0)
                    throw SSLSocketError{"Failed to accept an incomming SSL connection"};

                // Dont associate any context with the client, lifetime of ctx tied 
                // to SSL Server Socket. The SSL itself is associated to the client
                return SSLSocket{std::move(clientSocket), nullptr, clientSSL};
            }

            void connect(std::string_view serverIp, std::uint16_t port, std::string_view hostname = "") {
                socket.connect(serverIp, port);

                ssl = SSL_new(ctx);
                if (!ssl) throw SSLSocketError{"Unable to create SSL Context"};
                SSL_set_fd(ssl, socket.fd());

                if (!hostname.empty() && SSL_set_tlsext_host_name(ssl, hostname.data()) != 1)
                    throw SSLSocketError{"Failed to set SNI hostname"};

                if (SSL_connect(ssl) <= 0)
                    throw SSLSocketError{"Failed to establish TLS connection"};
            }

            // Send entire message to socket, guaranteed to send everything or throw an error
            void sendAll(std::string_view message) {
                while (!message.empty()) {
                    long sentBytes {SSL_write(ssl, message.data(), static_cast<int>(message.size()))};
                    if (sentBytes <= 0) throw SSLSocketError{"Failed to send"};
                    message.remove_prefix(static_cast<std::size_t>(sentBytes));
                }
            }

            [[nodiscard]] long send(std::string_view message) {
                long sentBytes {SSL_write(ssl, message.data(), static_cast<int>(message.size()))};
                if (sentBytes <= 0) throw SSLSocketError{"Failed to send"};
                return sentBytes;
            }

            [[nodiscard]] std::string recv(std::size_t maxBytes = 2048) {
                std::vector<char> buffer(maxBytes);
                long recvBytes {SSL_read(ssl, buffer.data(), static_cast<int>(maxBytes))};
                if (recvBytes == 0) close();
                else if (recvBytes < 0) throw SSLSocketError{"Failed to recv"};
                return {buffer.data(), static_cast<std::size_t>(recvBytes)};
            }

            // Recv until connection is closed; Use with caution, can block until connection is closed
            [[nodiscard]] std::string recvAll() {
                std::string message; char buffer[2048] {}; long recvBytes;
                while ((recvBytes = SSL_read(ssl, buffer, sizeof(buffer))) > 0)
                    message.append(buffer, static_cast<std::size_t>(recvBytes));
                if (recvBytes == 0) close();
                else if (recvBytes < 0) throw SSLSocketError{"Failed to recv"};
                return message;
            }

        private:
            bool isServer;
            Socket socket; 
            SSL_CTX *ctx; SSL* ssl; 
    };

    class HttpResponse {
        public:
            HttpResponse(const std::string &raw): raw{raw} {
                std::string statusLine;
                std::tie(statusLine, headers, body) = utils::parseHttpString(raw);

                // Extract status code from status line
                std::size_t pos1 {statusLine.find(" ")};
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid HttpResponse string");
                pos1 = statusLine.find_first_not_of(" ", pos1 + 1);
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid HttpResponse string");
                std::size_t pos2 = statusLine.find(" ", pos1); 
                if (pos2 == std::string::npos) pos2 = statusLine.size();
                statusCode = std::stoi(statusLine.substr(pos1, pos2 - pos1).data());
            }

            [[nodiscard]] inline bool ok() const { return statusCode >= 200 && statusCode < 400; }

            [[nodiscard]] JSON::JSONHandle json() const {
                return JSON::Parser::loads(body);
            }

        public:
            std::string raw, body; int statusCode;
            std::unordered_map<std::string, std::string> headers;
    };

    class HttpRequest {
        public:
            HttpRequest(
                const std::string &path = "/", 
                const std::string &method = "GET", 
                IP ipType = IP::V4
            ): path{path}, method{method}, ipType{ipType} {}

            /*
            HttpRequest(const std::string &raw, IP ipType = IP::V4) {
                std::string statusLine;
                std::tie(statusLine, headers, body) = utils::parseHttpString(raw);

                // Extract status code from status line
                std::size_t pos1 {statusLine.find(" ")};
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid HttpResponse string");
                pos1 = statusLine.find_first_not_of(" ", pos1 + 1);
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid HttpResponse string");
                std::size_t pos2 = statusLine.find(" ", pos1); 
                if (pos2 == std::string::npos) pos2 = statusLine.size();
                statusCode = std::stoi(statusLine.substr(pos1, pos2 - pos1).data());
            }
            */

            void setHeader(const std::string &key, const std::string &value) { 
                headers.push_back({key, value}); 
            }

            void setBody(std::string body) { this->body = std::move(body); }

            void setParam(const std::string &key, const std::string &value) { 
                urlParams.push_back({utils::urlEncode(key), utils::urlEncode(value)}); 
            }

            [[nodiscard]] std::string serialize() const {
                std::ostringstream oss;
                oss << method << ' ' << utils::getPathWithParams(path, urlParams) << " HTTP/1.1\r\n";
                for (const auto &kv: headers) oss << kv.first << ": " << kv.second << "\r\n";
                if (!body.empty()) oss << "Content-Length: " << body.size() << "\r\n\r\n" << body;
                else oss << "\r\n";
                return oss.str();
            }

            [[nodiscard]] HttpResponse 
            execute(std::string_view hostname, bool enableSSL = true, long timeoutSec = 5) {
                if (std::ranges::find(headers, "Host", &std::pair<std::string,std::string>::first) == headers.end())
                    setHeader("Host", std::string{hostname});
                std::string ipAddr {utils::resolveHostname(hostname, nullptr, SOCKTYPE::TCP, ipType)};
                if (enableSSL) return _executeSSL(ipAddr, 443, hostname, "");
                return _execute(ipAddr, 80, timeoutSec);
            }

            // If timeout set to a negative number, timeouts are ignored
            HttpResponse _execute(std::string_view ipAddr, std::uint16_t port, long timeoutSec) {
                net::Socket socket {SOCKTYPE::TCP, ipType};
                socket.connect(ipAddr, port);
                if (timeoutSec > 0) socket.setTimeout(timeoutSec, timeoutSec);
                socket.sendAll(serialize());
                return socket.recvAll();
            }

            HttpResponse _executeSSL(std::string_view ipAddr, std::uint16_t port, 
                std::string_view hostname = "", std::string_view certPath = "")
            {
                net::SSLSocket socket{false, certPath, "", ipType};
                socket.connect(ipAddr, port, hostname);
                socket.sendAll(serialize());
                return socket.recvAll();
            }

        private:
            std::vector<std::pair<std::string, std::string>> urlParams, 
                headers {{"Content-Type", "application/json"}, {"Connection", "close"}};
            std::string path, method, body;
            IP ipType {IP::V4};
    };

    class PollManager {
        public:
            enum class EventType { Unknown=0, Readable=1, Writable=2, Closed=4, Error=8 };

            friend EventType operator|=(EventType &e1, EventType e2) { return e1 = e1 | e2; }
            friend EventType operator|(EventType e1, EventType e2) {
                using T = std::underlying_type_t<EventType>;
                return static_cast<EventType>(static_cast<T>(e1) | static_cast<T>(e2)); 
            }

            // User almost certainly wanted to use '&' operator
            friend bool operator==(EventType, EventType) = delete;

            friend bool operator&(EventType e1, EventType e2) {
                using T = std::underlying_type_t<EventType>;
                return (static_cast<T>(e1) & static_cast<T>(e2)) != 0; 
            }

            void untrack(int fd) {
                auto it {sockets.find(fd)};
                if (it != sockets.end()) {
                    sockets.erase(fd);
                    auto newEnd {std::ranges::remove(pollFds, fd, &pollfd::fd)};
                    pollFds.erase(newEnd.begin(), newEnd.end());
                }
            }

            void track(Socket &&socket, EventType event = EventType::Readable | EventType::Writable) {
                int eventInt {}, fd {socket.fd()};
                sockets.emplace(fd, std::move(socket));
                if (event & EventType::Readable) eventInt |= POLLIN;
                if (event & EventType::Writable) eventInt |= POLLOUT;
                pollFds.emplace_back(fd, eventInt, 0);
            }

            void updateTracking(int fd, EventType event = EventType::Readable | EventType::Writable) {
                if (!sockets.contains(fd)) 
                    throw std::runtime_error("Socket FD is not tracked: " + std::to_string(fd));

                int eventInt {};
                if (event & EventType::Readable) eventInt |= POLLIN;
                if (event & EventType::Writable) eventInt |= POLLOUT;

                std::ranges::find(pollFds, fd, &pollfd::fd)->events 
                    = static_cast<short>(eventInt);
            }

            // To prevent ambiguity, lets delete this one
            void poll(bool) = delete;

            // Warning: While safe to `untrack` while iterating, Socket& can become dangling
            [[nodiscard]] std::vector<std::pair<Socket&, EventType>> 
            poll(int timeout = -1, bool raiseError = true) {
                // If poll failed return empty
                if (::poll(pollFds.data(), pollFds.size(), timeout) == -1) {
                    if (raiseError) throw SocketError{"Poll failed"};
                    return {};
                }

                // Store the result into events & clean any closed sockets
                std::vector<std::pair<Socket&, EventType>> result;
                std::vector<pollfd> pollFdsCleaned;
                pollFdsCleaned.reserve(pollFds.size());
                for (auto &pollFd: pollFds) {
                    if (int fd = pollFd.fd; getSocket(pollFd.fd).fd() == -1) {
                        sockets.erase(fd);
                    } else {
                        pollFdsCleaned.push_back(pollFd);
                        EventType event {EventType::Unknown};
                        if (pollFd.revents & POLLIN) 
                            event |= EventType::Readable;
                        if (pollFd.revents & POLLOUT)
                            event |= EventType::Writable;
                        if (pollFd.revents & POLLHUP)
                            event |= EventType::Closed;
                        if (pollFd.revents & POLLERR || pollFd.revents & POLLNVAL)
                            event |= EventType::Error;
                        if (!(event & EventType::Unknown))
                            result.emplace_back(getSocket(pollFd.fd), event);
                    }
                }

                std::swap(pollFds, pollFdsCleaned);
                pollFds.shrink_to_fit();

                return result;
            }

            [[nodiscard]] Socket &getSocket(int fd) {
                auto it {sockets.find(fd)};
                if (it == sockets.end()) 
                    throw std::runtime_error("No such file descriptor: " + std::to_string(fd));
                return it->second;
            }

        private:
            std::vector<pollfd> pollFds;
            std::unordered_map<int, Socket> sockets;
    };

    using PollEventType = PollManager::EventType;
}
