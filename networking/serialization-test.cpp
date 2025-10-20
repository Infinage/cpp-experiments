#include "net.hpp"
#include <cstring>
#include <print>

struct Dummy {
    double d; int i;
    operator std::string() const {
        return std::format("d: {}; i: {}", d, i);
    }
};

int main(int argc, char **argv) {
    net::Socket socket {};
    if (argc == 2 && std::strcmp(argv[1], "recv") == 0) {
        socket.bind("0.0.0.0", 8080);
        socket.listen();
        auto client {socket.accept()};
        std::string raw {client.recv()};
        if (raw.size() != sizeof(Dummy))
            throw std::runtime_error{std::format("Expected bytes: {}; Recv bytes: {}", sizeof(Dummy), raw.size())};

        Dummy *d {reinterpret_cast<Dummy*>(raw.data())};
        std::println("Received: {}", std::string{*d});
    } 

    else if (argc == 2 && std::strcmp(argv[1], "send") == 0) {
        socket.connect("0.0.0.0", 8080);
        Dummy d {1234.223, -1310};
        long sentBytes {socket.send({reinterpret_cast<char*>(&d), sizeof(Dummy)})};
        if (sentBytes != sizeof(Dummy))
            throw std::runtime_error{std::format("Expected bytes: {}; Sent bytes: {}", sizeof(Dummy), sentBytes)};
        std::println("Sent: {}", std::string{d});
    }

    else {
        std::println("Usage: serialization-test (send, recv)");
    }
}
