#include "net.hpp"
#include <print>
#include <thread>

int main() try {
    // Resolve IPV6 domain and get resp (curl -6 https://api6.ipify.org?format=json)
    {
        const char *url {"https://api6.ipify.org"};
        std::string ipAddr {net::utils::resolveURL(url, net::SOCKTYPE::TCP, net::IP::V6)};

        net::HttpRequest req {"/", "GET", net::IP::V6}; 
        req.setParam("format", "json");
        net::HttpResponse resp {req.execute(url)};
        std::println("Resolved IPV6 Addr: {}\nResponse: {}\n", 
            ipAddr, resp.json().str(false));
    }

    // Send a https get request to API (i.e with SSL support)
    {
        net::HttpRequest req {"/users/1"};
        net::HttpResponse resp {req.execute("https://jsonplaceholder.typicode.com")};
        JSON::JSONHandle json {resp.json()};
        std::println(
            "Status Code: {}\nContent Type: {}\n\n"
            "Name: {}\nusername: {}\n"
            "User Company Name: {}\n",
            resp.statusCode, 
            resp.headers["content-type"], 
            json["name"].str(),
            json["username"].str(), 
            json["company"]["name"].str()
        );
    }

    // Send a http get request to API (i.e without SSL support)
    // Github forces redirect to https
    {
        net::HttpRequest req;
        net::HttpResponse resp {req.execute("http://github.com")};
        std::println("Response Status: {}\nRequest URL redirected to: {}\n", 
            resp.statusCode, resp.location);
    }

    // Send UDP packets to `nc -l -u 4444`, ensure running
    {
        net::Socket client {net::SOCKTYPE::UDP};
        client.connect("0.0.0.0", 4444);
        for (std::size_t i {}; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            long _ {client.send("Message: " + std::to_string(i) + '\n')};
        }
    }

    // Utils to extract pieces of an url
    {
        auto [protocol, domain, port, path] {
            net::utils::extractURLPieces(
                "udp://tracker.coppersurfer.tk:6969/"
                "announce?info_hash=062c43b1b47e25c7bee1fefc3b945758bd11318b&peer_id"
                "=leH8z33e9V0ODjlHZD4z&uploaded=0&downloaded=0&compact=1&left=124234"
            )
        };

        std::println("Protocol: {}\nDomain: {}\nPort: {}\nPath: {}",
            protocol, domain, port, path);
    }
} catch (std::exception &ex) {
    std::println("{}", ex.what());
}
