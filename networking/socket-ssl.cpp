#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <unistd.h>

int main() {
    const char *url {"google.com"};

    addrinfo hints{}, *res {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(url, "https", &hints, &res) != 0)
        throw std::runtime_error("Failed to resolve hostname");

    int fd {socket(res->ai_family, res->ai_socktype, res->ai_protocol)};
    if (fd == -1) throw std::runtime_error("Failed to create a socket");

    if (connect(fd, res->ai_addr, res->ai_addrlen) == -1)
        throw std::runtime_error("Error connecting to the specified socket");

    char ipStr[INET_ADDRSTRLEN]; sockaddr_in *addrObj {reinterpret_cast<sockaddr_in*>(res->ai_addr)};
    inet_ntop(AF_INET, &addrObj->sin_addr, ipStr, sizeof(ipStr));
    std::cout << "IP: " << ipStr << '\n' << "PORT: " << ntohs(addrObj->sin_port) << '\n';

    freeaddrinfo(res);

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) throw std::runtime_error("Unable to create SSL Context");
    SSL* ssl = SSL_new(ctx); SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) <= 0) 
        throw std::runtime_error("Failed to establish TLS connection");

    std::string msg {
        "GET / HTTP/1.1\r\n"
        "Host: www.google.com\r\n"     // Mandatory starting for http 1.1
        "Connection: close\r\n\r\n"    // Ensure server closes connection once resp is sent
    };

    if (SSL_write(ssl, msg.c_str(), static_cast<int>(msg.size())) < 0)
        throw std::runtime_error("Failed to send");

    std::string response;
    char buf[4096]; int readBytes;
    while ((readBytes = SSL_read(ssl, buf, sizeof(buf))) > 0) {
        response.append(buf, static_cast<std::size_t>(readBytes));
        std::cout << readBytes << '\n' << buf << '\n';
    }

    std::cout << "Received from server: " << response << '\n';

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(fd);
    SSL_CTX_free(ctx);
}
