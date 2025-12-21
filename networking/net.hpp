/*
 * TODO:
 * - Support for proxy
 * - Write unit test cases
 * - Modify httpserver to use this module
 * - getaddrinfo() to iterate through results instead of returning first one
 * - Use C++ modules
 * - Windows support?
 */

#pragma once

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
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../json-parser/json.hpp"

namespace net {
    // Abstracing SOCK_STREAM, SOCK_DGRAM; AF_INET, AF_INET6
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
        [[nodiscard]] constexpr auto bswap(std::integral auto val) {
            if constexpr (std::endian::native == std::endian::little)
                return std::byteswap(val);
            return val;
        }

        template<typename ...Args>
        void inplace_bswap(Args &...args) { ((args = bswap(args)), ...); }

        [[nodiscard]] inline std::string ipBytesToString(std::string_view raw, IP ipType = IP::V4) {
            const unsigned int ipLen {static_cast<unsigned int>(ipType == IP::V4? INET_ADDRSTRLEN: INET6_ADDRSTRLEN)};
            const int ipFamily {ipType == IP::V4? AF_INET: AF_INET6};
            std::string ipStr(ipLen, '\0');
            if (!inet_ntop(ipFamily, raw.data(), ipStr.data(), ipLen))
                throw SocketError{"Failed to convert address to string"};
            ipStr.resize(std::strlen(ipStr.data()));
            return ipStr;
        }

        // Checks for V4 first then checks for V6. If neither returns empty
        [[nodiscard]] inline std::optional<IP> checkIPType(std::string_view ipAddr) {
            sockaddr_storage storage; // Check for IPV4
            if (inet_pton(AF_INET, ipAddr.data(), reinterpret_cast<sockaddr_in*>(&storage)) == 1)
                return IP::V4;

            std::memset(&storage, 0, sizeof(storage)); // Check for IPV6
            if (inet_pton(AF_INET6, ipAddr.data(), reinterpret_cast<sockaddr_in6*>(&storage)) == 1)
                return IP::V6;

            return {};
        }

        [[nodiscard]] constexpr std::string trimStr(std::string_view str) {
            auto first = str.find_first_not_of(' ');
            if (first == std::string::npos) return "";
            auto last = str.find_last_not_of(' ');
            return {str.begin() + first, str.begin() + last + 1};
        }

        [[nodiscard]] constexpr std::string toLower(std::string_view str) {
            std::string res(str.size(), '\0'); 
            std::ranges::transform(str, res.begin(), [](unsigned char ch) { return std::tolower(ch); }); 
            return res;
        }

        [[nodiscard]] inline std::unordered_map<std::string, std::vector<std::string>>
        parseHeadersFromString(std::string_view headerRaw) {
            std::size_t pos1, pos2;
            std::unordered_map<std::string, std::vector<std::string>> headers;
            while (!headerRaw.empty()) {
                pos1 = headerRaw.find(':');
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid Http header");

                pos2 = headerRaw.find("\r\n", pos1 + 1);
                if (pos2 == std::string::npos) throw std::runtime_error("Invalid Http header");

                // Header keys are always in lower case
                std::string key {trimStr(toLower(headerRaw.substr(0, pos1)))}; 
                std::string value {trimStr(headerRaw.substr(pos1 + 1, pos2 - pos1 - 1))};
                headers[key].push_back(value);

                headerRaw = headerRaw.substr(pos2 + 2);
            }

            return headers;
        }

        // Parse the input http string to see if we are good to feed const from it
        [[nodiscard]] inline bool isCompleteHttpRequest(std::string_view raw) {
            std::size_t pos {raw.find("\r\n")};
            if (pos == std::string::npos) return false;

            std::size_t headerEnd {pos};
            pos = raw.find("\r\n\r\n", headerEnd);
            if (pos == std::string::npos) return false;
            auto body = raw.substr(pos + 4);

            std::string_view headerRaw {raw.begin() + headerEnd + 2, raw.begin() + pos + 2};
            auto headers = parseHeadersFromString(headerRaw);

            if (headers.count("content-length")) {
                try {
                    auto length {headers.at("content-length")[0]};
                    return body.size() >= std::stoul(length);
                } catch (...) { return false; }
            }

            if (headers.count("transfer-encoding")) {
                std::size_t chunkSize;
                do {
                    pos = body.find("\r\n");
                    if (pos == std::string::npos) return false;
                    try { 
                        std::string chunkSizeRaw {body.substr(0, pos)};
                        chunkSize = std::stoul(chunkSizeRaw, nullptr, 16); 
                    }
                    catch (...) { return false; }
                    if (body.size() <= pos + 4 + chunkSize) return false;
                    body = body.substr(pos + chunkSize + 4);
                } while (chunkSize && !body.empty());
                return true;
            }

            return true;
        }

        [[nodiscard]] inline 
        std::tuple<std::string, std::unordered_map<std::string, std::vector<std::string>>, std::string> 
        parseHttpString(std::string_view raw) {
            // Extract request / status line
            std::size_t pos1 {raw.find("\r\n")}, pos2;
            if (pos1 == std::string::npos) throw std::runtime_error("Invalid Http string");
            std::string firstLine {trimStr(raw.substr(0, pos1))};

            // Seperate the body and headers (headers are optional)
            pos2 = raw.find("\r\n\r\n", pos1);
            if (pos2 == std::string::npos) throw std::runtime_error("Invalid Http string");
            std::string body {raw.substr(pos2 + 4)};
            std::string_view headerRaw {
                raw.begin() + static_cast<long>(pos1) + 2, 
                raw.begin() + static_cast<long>(pos2) + 2
            };

            // Parse the headers (no effect if headerRaw is empty)
            auto headers {parseHeadersFromString(headerRaw)};

            return std::make_tuple(firstLine, headers, body);
        }

        // Given a hostname such as 'google.com' and an optional 'port/service' resolves to an IP addr 
        // Service can be 'http', 'https', '443', etc and can be set to a nullptr
        [[nodiscard]] inline std::string resolveHostname(
            std::string_view hostname, const char *service = nullptr,
            SOCKTYPE sockType = SOCKTYPE::TCP, IP ipType = IP::V4)
        {
            int domain {ipType == IP::V4? AF_INET: AF_INET6};
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
                    addr4->sin_port = utils::bswap(port); addr4->sin_family = AF_INET;
                    if (inet_pton(AF_INET, ip.data(), &addr4->sin_addr) <= 0)
                        throw SocketError{"Invalid IPV4 Address"};
                } else {
                    sockaddr_in6 *addr6 {reinterpret_cast<sockaddr_in6*>(&storage)};
                    addr6->sin6_port = utils::bswap(port); addr6->sin6_family = AF_INET6;
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
                    port = utils::bswap(addr4->sin_port); host.resize(INET_ADDRSTRLEN);
                    ret = inet_ntop(AF_INET, &addr4->sin_addr, host.data(), INET_ADDRSTRLEN);
                } else {
                    sockaddr_in6 *addr6 {reinterpret_cast<sockaddr_in6*>(&storage)};
                    port = utils::bswap(addr6->sin6_port); host.resize(INET6_ADDRSTRLEN);
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
                _sockSize{static_cast<unsigned int>(ipType == IP::V4? 
                        sizeof(sockaddr_in): sizeof(sockaddr_in6))}
            {
                int domain {_ipType == IP::V4? AF_INET: AF_INET6};
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

            // For non blocking sockets, code will throw if there are non incoming connections.
            // Need to explicitly poll for READABLE event on serverfd before accept.
            // Note: Accepted socket inherits blocking mode from server socket
            [[nodiscard]] Socket accept() {
                int clientSocket {::accept(_fd, nullptr, nullptr)};
                if (clientSocket == -1) throw SocketError{"Failed to accept an incomming connection"};
                Socket client{clientSocket, _sockType, _ipType};
                if (!_blocking) client.setNonBlocking();
                return client;
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
                if (connectStatus == -1 && errno != EINPROGRESS) throw SocketError{"Error connecting to server"};
            }

            // Attempts to send the entire message buffer.
            // For **blocking sockets**, this will loop until all bytes are sent or an error occurs.
            // For **non-blocking sockets**, this may return early if the socket cannot accept more data
            // Partial sends are handled by adjusting the string_view prefix.
            // Verify partial sends for non blocking sockets against return result
            long sendAll(std::string_view message) {
                long totalSent {};
                while (!message.empty()) {
                    long sentBytes {this->send(message)};
                    if (sentBytes == 0) break;
                    message.remove_prefix(static_cast<std::size_t>(sentBytes));
                    totalSent += sentBytes;
                }
                return totalSent;
            }

            // Can send serialized char* messages, provided string_view is explicitly constructed
            // by passing the size. For eg: send({reinterpret_cast<char*>(&obj), sizeof(obj)});
            // For non-blocking sockets, a return value of 0 means the socket would block
            // Any other error causes a socketerror exception
            [[nodiscard]] long send(std::string_view message) {
                long sentBytes {::send(_fd, message.data(), message.size(), MSG_NOSIGNAL)};
                if (sentBytes <= 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
                    throw SocketError{"Failed to send"};
                }
                return sentBytes;
            }

            [[nodiscard]] std::string recv(std::size_t maxBytes = 2048) {
                std::vector<char> buffer(maxBytes);
                long recvBytes {::recv(_fd, buffer.data(), maxBytes, 0)};
                if (recvBytes == 0 || (recvBytes < 0 && errno == ECONNRESET)) close();
                else if (recvBytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    throw SocketError{"Failed to recv"};
                return {buffer.data(), static_cast<std::size_t>(std::max(0l, recvBytes))};
            }

            // Recv until connection is closed or non blocking errcode is hit
            // Use with caution for blocking sockets, it will hang until con is closed
            [[nodiscard]] std::string recvAll(std::size_t recvBatchSize = 2048) {
                long totalRecv {}; std::vector<char> buffer(recvBatchSize);
                while (_fd != -1) {
                    long recvBytes {::recv(_fd, buffer.data() + totalRecv, recvBatchSize, 0)};
                    if (recvBytes > 0) {
                        totalRecv += recvBytes;
                        buffer.resize(static_cast<std::size_t>(totalRecv) + recvBatchSize);
                    } else {
                        // recvBytes 0 implies client closed regardless of socket blocking type
                        if (recvBytes == 0 || (recvBytes < 0 && errno == ECONNRESET)) close();
                        // exit loop without closing socket
                        else if (errno == EAGAIN || errno == EWOULDBLOCK) break; 
                        else throw SocketError{"Failed to recv"};
                    }
                }
                return {buffer.data(), static_cast<std::size_t>(std::max(0l, totalRecv))};
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
                return {buffer.data(), static_cast<std::size_t>(std::max(0l, recvBytes))};
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
                    if (SSL_CTX_check_private_key(ctx) <= 0)
                        throw SSLSocketError("Private key does not match the certificate public key");
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
            [[nodiscard]] std::string recvAll(std::size_t recvBatchSize = 2048) {
                long recvBytes {}, totalRecv {};
                std::vector<char> buffer(recvBatchSize);
                while ((recvBytes = SSL_read(ssl, buffer.data() + totalRecv, static_cast<int>(recvBatchSize))) > 0) {
                    totalRecv += recvBytes;
                    if (recvBytes == 0) close();
                    else if (recvBytes < 0) throw SSLSocketError{"Failed to recv"};
                    else buffer.resize(static_cast<std::size_t>(totalRecv) + recvBatchSize);
                }
                return {buffer.data(), static_cast<std::size_t>(totalRecv)};
            }

        private:
            bool isServer;
            Socket socket; 
            SSL_CTX *ctx; SSL* ssl; 
    };

    class URL {
        private:
            // Given a path with optional parameters, it extracts and returns {path, params}
            // Params will be url encoded and would need to be decoded
            static std::pair<std::string, std::vector<std::pair<std::string, std::string>>>
            getParamsFromPath(std::string_view path) {
                // Seperate logic for extracting a single key=value pair
                auto extractKV {[](std::string_view raw) {
                    std::size_t sepPos {raw.find('=')};
                    std::string_view key {raw}, value {};
                    if (sepPos != std::string::npos) {
                        key = raw.substr(0, sepPos);
                        value = raw.substr(sepPos + 1);
                    } 
                    return std::pair(std::string{key}, std::string{value});
                }};

                // Extract all k=v separated by an ampersand
                std::vector<std::pair<std::string, std::string>> params;
                if (path.empty() || path.at(0) != '/') throw std::runtime_error("Invalid URI path");

                // Find '?', if not found no params -> /path1/path2/
                // If found seperate path and paramPath
                std::size_t pos {path.find('?')}; std::string_view paramPath {};
                if (pos != std::string::npos) {
                    paramPath = path.substr(pos + 1);
                    path = path.substr(0, pos);
                }

                // Iterate and extract all parameters
                while (!paramPath.empty()) {
                    pos = paramPath.find('&'); 
                    bool lastPiece {pos == std::string::npos};
                    params.emplace_back(extractKV(lastPiece? paramPath: paramPath.substr(0, pos)));
                    paramPath = lastPiece? std::string_view{} :paramPath.substr(pos + 1);
                }

                return {std::string{path}, params};
            }

            // Returns tuple of [protocol, domain, port, path], port is 0 if missing
            // Assumes pattern: `<protocol>:://[username:password@]<domain>[:port]/<path>`
            [[nodiscard]] inline 
            std::tuple<std::string, std::string, std::uint16_t, std::string>
            extractURLPieces(std::string_view url) {
                std::string protocol, path {"/"}; std::uint16_t port {};
                std::size_t pos {url.find("://")};
                if (pos == std::string::npos) throw std::runtime_error("Missing protocol: " + std::string{url});
                protocol = url.substr(0, pos); url = url.substr(pos + 3);

                // Extract the path out
                if ((pos = url.find('/')) != std::string::npos) {
                    path = url.substr(pos); url = url.substr(0, pos);
                }

                // Remove [username:password@] if exists
                if ((pos = url.find('@')) != std::string::npos) {
                    url = url.substr(pos + 1);
                }

                // Extract port if available
                if ((pos = url.find(':')) != std::string::npos) {
                    try {
                        port = static_cast<std::uint16_t>(std::stol(std::string{url.substr(pos + 1)}));
                    } catch(...) {
                        throw std::runtime_error("Invalid or out of range port : " + std::string{url});
                    }
                    url = url.substr(0, pos);
                }

                return {protocol, std::string{url}, port, path};
            }

        public:
            IP ipType {IP::V4};
            std::string protocol {"http"};
            std::string domain {"localhost"}, ipAddr {"127.0.0.0"};
            std::uint16_t port {0};
            std::string path {"/"};
            std::vector<std::pair<std::string, std::string>> params; 

        public:
            URL() = default;

            void swap(URL &other) noexcept {
                using std::swap;
                swap(this->ipType, other.ipType);
                swap(this->protocol, other.protocol);
                swap(this->domain, other.domain);
                swap(this->ipAddr, other.ipAddr);
                swap(this->port, other.port);
                swap(this->path, other.path);
                swap(this->params, other.params);
            }

            URL &operator=(std::string_view url) {
                URL{url}.swap(*this);
                return *this;
            }

            // URL: `<protocol>:://<domain>[:port]/<path>`
            // Constructs it lazily, you may want to call `resolve` to get its IP address
            URL(std::string_view url, IP ipType = IP::V4): ipType{ipType}, ipAddr{""} {
                std::tie(protocol, domain, port, path) = extractURLPieces(url);
                port = port != 0 || (protocol != "http" && protocol != "https")? port: protocol == "https"? 443: 80;
                this->setPath(path);
            }

            // URL translates to: [proto]://127.0.0.1:[port]/<path>
            // Unless port is set correctly, URL will remain unroutable
            [[nodiscard]] static URL localhost(
                std::uint16_t port, std::string_view path = "/", 
                std::string_view proto = "http", IP ipType = IP::V4
            ) {
                URL url;
                url.port = port;
                url.protocol = proto; 
                url.ipType = ipType;
                url.setPath(path);
                return url;
            }

            // Resolve the hostname to its ipaddr
            std::string resolve(bool force = false) {
                if ((ipAddr.empty() && domain.empty()) || port == 0) 
                    throw std::runtime_error("Not a valid URL: " + getFullPath());
                if (ipAddr.empty() || force) 
                    ipAddr = utils::resolveHostname(domain, nullptr, SOCKTYPE::TCP, ipType);
                return ipAddr;
            }

            // Set path along with params from input string
            void setPath(std::string_view path) {
                std::tie(this->path, this->params) = getParamsFromPath(path);
            }

            // Set path only ignoring the parameters
            void setPathWithoutParams(std::string_view path) {
                std::tie(this->path, std::ignore) = getParamsFromPath(path);
            }

            // Returns path with the encoded parameters, if access to just path 
            // is required, simply access the path member variable
            [[nodiscard]] inline std::string getPath() const { 
                if (params.empty()) return path;
                std::ostringstream oss; oss << path << '?';
                for (const auto &kv: params) oss << kv.first << "=" << kv.second << '&';
                std::string res {oss.str()}; res.pop_back();
                return res;
            }

            // For absolute returns: <protocol>://<hostname/ip>[:port]/<path>[?params]
            // For relative returns: /<path>[?params]
            [[nodiscard]] inline std::string getFullPath() const {
                const auto path_ {getPath()};
                const auto hostname_ {domain.empty()? ipAddr: domain};
                if (protocol.empty() || hostname_.empty()) return path_;
                return port != 0?
                    std::format("{}://{}:{}{}", protocol, hostname_, port, path_):
                    std::format("{}://{}{}", protocol, hostname_, path_);
            }

            // Adds to params post encoding input key & value
            void setParam(std::string_view key, std::string_view value) { 
                params.push_back({encode(key), encode(value)}); 
            }

            // Deletes all occurrences of a key
            std::size_t unsetParam(std::string_view key) {
                const auto key_ {encode(key)};
                return std::erase_if(params, [&](const auto &p) { return p.first == key_; });
            }

            // Returns all params post url decoding of key-value pairs
            [[nodiscard]] auto getParams() const {
                auto urlDecodePair {[](const std::pair<std::string, std::string> &pair) {
                    return std::make_pair(decode(pair.first), decode(pair.second));
                }};

                return std::ranges::views::transform(params, urlDecodePair);
            }

            [[nodiscard]] static std::string encode(std::string_view str, 
                bool mapSpaceToPlus = true) 
            {
                std::ostringstream oss; oss.fill('0');
                for (std::size_t i {}; i < str.size(); ++i) {
                    char ch {str.at(i)};
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

            [[nodiscard]] static std::string decode(std::string_view str) {
                std::ostringstream oss; std::string acc;
                for (std::size_t i {}; i < str.size(); ++i) {
                    char ch {str.at(i)};
                    if (!acc.empty()) {
                        if (acc.size() == 1) acc.push_back(ch);
                        else {
                            acc.push_back(ch);
                            oss << static_cast<unsigned char>(std::stoi(acc, 0, 16));
                            acc.clear();
                        }
                    }
                    else if (ch == '+') oss << ' ';
                    else if (ch != '%') oss << ch;
                    else acc.push_back('0');
                }
                return oss.str();
            }
    };

    class HttpResponse {
        public:
            HttpResponse() = default;

            static HttpResponse fromString(const std::string &raw) {
                HttpResponse resp; resp.raw = raw;
                std::string statusLine; // Must be a string (below func returns rvalue)
                std::tie(statusLine, resp.headers, resp.body) = utils::parseHttpString(raw);

                // Extract status code from status line
                // <HTTP-Version> <Status-Code> [<Reason-Phrase>]
                std::size_t pos1 {statusLine.find(' ')};
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid HttpResponse string");
                pos1 = statusLine.find_first_not_of(' ', pos1 + 1);
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid HttpResponse string");
                std::size_t pos2 = statusLine.find(' ', pos1); 
                if (pos2 == std::string::npos) pos2 = statusLine.size(); // since reason is optional
                resp.statusCode = std::stoi(statusLine.substr(pos1, pos2 - pos1).data());
                return resp;
            }

            [[nodiscard]] std::string toString() const {
                std::ostringstream oss;
                oss << "HTTP/1.1 " << statusCode << "\r\n";
                for (const auto &kv: headers) {
                    for (const auto &val: kv.second)
                        oss << kv.first << ": " << val << "\r\n";
                }
                if (body.empty()) oss << "\r\n";
                else {
                    oss << "content-length: " << body.size() 
                        << "\r\n\r\n" << body;
                }
                return oss.str();
            }

            [[nodiscard]] inline bool ok() const { return statusCode >= 200 && statusCode < 400; }

            [[nodiscard]] JSON::JSONHandle json() const {
                return JSON::Parser::loads(body);
            }

            [[nodiscard]] inline std::string unchunk() const {
                std::istringstream iss {body}; 
                std::ostringstream res; std::size_t chunkSize {};
                do {
                    iss >> std::hex >> chunkSize;
                    iss.seekg(2, std::ios::cur);
                    std::string acc(chunkSize, 0);
                    iss.read(acc.data(), static_cast<std::streamsize>(chunkSize));
                    res << acc;
                    iss.seekg(2, std::ios::cur);
                } while (chunkSize);
                return res.str();
            }

            // Convenience wrapper that returns the first value found, ensure 
            // you manually check for multiple values if required
            [[nodiscard]] std::string header(std::string_view key) const {
                auto it {headers.find(utils::toLower(key))};
                if (it == headers.end() || it->second.empty()) return "";
                else return it->second.front();
            }

        public:
            std::string raw, body, location; int statusCode {200};
            std::unordered_map<std::string, std::vector<std::string>> headers;
    };

    class HttpRequest {
        public:
            HttpRequest(auto &&url, const std::string &method = "GET"): 
                url{std::forward<decltype(url)>(url)}, method{method} {}

            static HttpRequest fromString(const std::string &raw) {
                // reqLine must be a string (below func returns rvalue)
                HttpRequest req {URL::localhost(0)}; std::string requestLine;
                std::tie(requestLine, req.headers, req.body) = utils::parseHttpString(raw);

                // Extract method, path and path params
                // <Method> <Request-URI> <HTTP-Version>
                std::size_t pos1 {requestLine.find(' ')};
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid HttpRequest string");
                req.method = requestLine.substr(0, pos1);
                pos1 = requestLine.find_first_not_of(' ', pos1 + 1);
                if (pos1 == std::string::npos) throw std::runtime_error("Invalid HttpRequest string");
                std::size_t pos2 {requestLine.find(' ', pos1)};
                if (pos2 == std::string::npos) throw std::runtime_error("Invalid HttpRequest string");
                std::string_view requestURI {requestLine.data() + pos1, pos2 - pos1};
                req.url.setPath(requestURI);
                return req;
            }

            // Clears any prev set header and inserts current params
            // Suitable when same key has multiple values (eg: 'set-cookie')
            template<typename Vec> 
            requires(std::is_same_v<std::remove_cvref<Vec>, std::vector<std::string>>)
            void setHeader(std::string_view key, Vec &&value) { 
                headers[utils::toLower(key)] = std::forward(value);
            }

            // Clears any previously set header and inserts current params
            void setHeader(std::string_view key, std::string value) { 
                headers[utils::toLower(key)] = {std::move(value)};
            }

            // Clears input key from the list of headers; returns true if found
            bool unsetHeader(std::string_view key) { return headers.erase(utils::toLower(key)); }

            decltype(auto) getURL(this auto &&self) { return self.url; }

            void setBody(std::string body) { this->body = std::move(body); }

            [[nodiscard]] auto &getHeaders() const { return headers; }

            [[nodiscard]] std::vector<std::string> getHeader(std::string_view key) const {
                auto it {headers.find(utils::toLower(key))};
                return it != headers.end()? it->second: std::vector<std::string>{};
            }

            [[nodiscard]] const std::string &getBody() const { return body; }
            [[nodiscard]] const URL &getURL() const { return url; }

            [[nodiscard]] std::string getMethod() const { return method; }
            [[nodiscard]] IP getIPType() const { return url.ipType; }

            [[nodiscard]] std::string toString() const {
                std::ostringstream oss;
                oss << method << ' ' << url.getPath() << " HTTP/1.1\r\n";
                for (const auto &kv: headers) {
                    for (const auto &val: kv.second)
                        oss << kv.first << ": " << val << "\r\n";
                }
                if (!body.empty()) oss << "content-length: " << body.size() << "\r\n\r\n" << body;
                else oss << "\r\n";
                return oss.str();
            }

            // If timeout set to a negative number, timeouts are ignored. They are applied for each redirect & not on a cumulative basis.
            // HttpResponse object will have the final url set post resolving any redirect
            [[nodiscard]] HttpResponse execute(long timeoutSec = 5, std::string_view certPath = "", std::size_t follow = 5) {
                url.resolve(); // Resolve the URL and throw on failure

                if (url.protocol != "http" && url.protocol != "https")
                    throw std::runtime_error("Unsupported protocol: " + url.protocol);

                if (headers.find("host") == headers.end()) setHeader("Host", url.domain);
                HttpResponse resp {_execute(timeoutSec, certPath, url.protocol == "https")};
                resp.location = url.getFullPath();

                // Handle redirects (relative urls such as '../../' are not supported)
                int status {resp.statusCode}; 
                if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308) && follow > 0) {
                    std::string redirectURL {resp.header("location")};
                    if (redirectURL.empty()) return resp;
                    if (status == 303) method = "GET";

                    // If abs path, modify entire URL to location header value
                    // If rel path, only modify the path. We reset the params
                    if (redirectURL.front() != '/') url = URL {redirectURL}; 
                    else url.setPath(redirectURL);

                    // Clear the host header to be set in the next call
                    unsetHeader("host");

                    return execute(timeoutSec, certPath, follow - 1);
                }

                return resp;
            }

        // Unlike httpresponse unintended modification of these values can mean trouble
        // So we keep them private and provide functions for access
        private: 
            URL url; std::string method, body;
            std::unordered_map<std::string, std::vector<std::string>> headers {{"connection", {"close"}}};

        private:
            // If timeout set to a negative number, timeouts are ignored
            HttpResponse _execute(long timeoutSec, std::string_view certPath, bool ssl) {
                if (!ssl) {
                    net::Socket socket {SOCKTYPE::TCP, url.ipType};
                    if (timeoutSec > 0) socket.setTimeout(timeoutSec, timeoutSec);
                    socket.connect(url.ipAddr, url.port);
                    socket.sendAll(this->toString());
                    return HttpResponse::fromString(socket.recvAll());
                } 
                else {
                    net::SSLSocket socket{false, certPath, "", url.ipType};
                    if (timeoutSec > 0) socket.setTimeout(timeoutSec, timeoutSec);
                    socket.connect(url.ipAddr, url.port, url.domain);
                    socket.sendAll(this->toString());
                    return HttpResponse::fromString(socket.recvAll());
                }
            }
    };

    class PollManager {
        public:
            enum class EventType { Unknown=0, Readable=1, Writable=2, Closed=4, Error=8 };

            friend EventType operator|=(EventType &e1, EventType e2) { return e1 = e1 | e2; }
            friend EventType operator|(EventType e1, EventType e2) {
                using T = std::underlying_type_t<EventType>;
                return static_cast<EventType>(static_cast<T>(e1) | static_cast<T>(e2)); 
            }

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

            int track(Socket &&socket, EventType event = EventType::Readable | EventType::Writable) {
                int eventInt {}, fd {socket.fd()};
                sockets.emplace(fd, std::move(socket));
                if (event & EventType::Readable) eventInt |= POLLIN;
                if (event & EventType::Writable) eventInt |= POLLOUT;
                pollFds.emplace_back(fd, eventInt, 0);
                return fd;
            }

            void updateTracking(int fd, EventType event = EventType::Readable | EventType::Writable) {
                if (!hasSocket(fd)) throw std::runtime_error("Socket FD is not tracked: " + std::to_string(fd));

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
                        if (event != EventType::Unknown)
                            result.emplace_back(getSocket(pollFd.fd), event);
                    }
                }

                std::swap(pollFds, pollFdsCleaned);
                pollFds.shrink_to_fit();

                return result;
            }

            [[nodiscard]] bool empty() const { return sockets.empty(); }
            [[nodiscard]] std::size_t size() const { return sockets.size(); }

            [[nodiscard]] bool hasSocket(int fd) const { return sockets.contains(fd); }

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
