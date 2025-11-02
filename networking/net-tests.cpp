#include <cassert>
#include "net.hpp"

int main() {
    // net::util::bswap is constexpr version of _bswap 
    // Useful to convert from bigendian to little endian & vice versa
    {
        using namespace net;
        static_assert(utils::bswap(uint16_t{0x1234}) == 0x3412);
        static_assert(utils::bswap(uint32_t{0x12345678}) == 0x78563412);
        static_assert(utils::bswap(utils::bswap(uint16_t{0x1234})) == 0x1234);
    }

    // Converting raw integers to ip style strings (note: returned ipv6 is always the shorterned)
    {
        using namespace net;
        auto ipBytes = [](std::string_view ip, net::IP type) {
            std::array<unsigned char, 16> buf{};
            if (inet_pton(type == net::IP::V4 ? AF_INET : AF_INET6, ip.data(), buf.data()) != 1)
                throw std::runtime_error("invalid ip");
            return std::string(reinterpret_cast<const char*>(buf.data()), type == net::IP::V4 ? 4 : 16);
        };

        assert(utils::ipBytesToString(ipBytes("0.0.0.0", net::IP::V4), net::IP::V4) == "0.0.0.0");
        assert(utils::ipBytesToString(ipBytes("127.0.0.1", net::IP::V4), net::IP::V4) == "127.0.0.1");
        assert(utils::ipBytesToString(ipBytes("::1", net::IP::V6), net::IP::V6) == "::1");
        assert(utils::ipBytesToString(ipBytes("2001:db8:85a3::8a2e:370:7334", net::IP::V6), net::IP::V6) 
                == "2001:db8:85a3::8a2e:370:7334");
    }

    // URL encoding and decoding of strings
    {
        using namespace net;
        assert(URL::encode("Hello world!") == "Hello+world%21");
        assert(URL::encode("Hello world!", false) == "Hello%20world%21");
        assert(URL::decode(URL::encode(R"(AB123!@#$%^&*()-=_+[]{}|\:;"',.<>/?`~ )")) 
                == R"(AB123!@#$%^&*()-=_+[]{}|\:;"',.<>/?`~ )");
        assert(URL::decode(URL::encode(R"(AB123!@#$%^&*()-=_+[]{}|\:;"',.<>/?`~ )", false)) 
                == R"(AB123!@#$%^&*()-=_+[]{}|\:;"',.<>/?`~ )");
    }

    // Extract various portions of a URL
    {
        net::URL url {
            "udp://tracker.coppersurfer.tk:6969/"
            "announce?info_hash=062c43b1b47e25c7bee1fefc3b945758bd11318b&peer_id"
            "=leH8z33e9V0ODjlHZD4z&uploaded=0&downloaded=0&compact=1&left=124234"
        };
        assert(url.protocol == "udp");
        assert(url.domain == "tracker.coppersurfer.tk");
        assert(url.port == 6969);
        assert(url.path == "/announce");
        assert(url.getPath() == 
            "/announce?info_hash=062c43b1b47e25c7bee1fefc3b945758bd11318b&peer_id"
            "=leH8z33e9V0ODjlHZD4z&uploaded=0&downloaded=0&compact=1&left=124234"
        );
    }

    // Parsing HTTP Request from strings
    {
        const char *rawReq =
            "GET /test?foo=bar&num=42 HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Connection: close\r\n"
            "\r\n";

        auto req {net::HttpRequest::fromString(rawReq)};
        assert(req.getMethod() == "GET");
        assert(req.getURL().path == "/test");
        assert(req.getURL().getParams().size() == 2);
        assert(req.getHeaders().size() == 2);
        assert(req.getHeader("host")[0] == "example.com");

        // Test serialization
        std::string reqStr {req.toString()};
        assert(reqStr.find("GET /test?foo=bar&num=42") == 0);
        assert(reqStr.find("host: example.com") != std::string::npos);
        assert(net::HttpRequest::fromString(reqStr).getURL().getFullPath() 
                == req.getURL().getFullPath());
    }

    // Parsing HTTP Response from strings
    {
        const char *rawResp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "{\"key\":\"val\"}";

        net::HttpResponse resp = net::HttpResponse::fromString(rawResp);
        assert(resp.statusCode == 200);
        assert(resp.header("content-type") == "application/json");
        assert(resp.body == "{\"key\":\"val\"}");
        assert(resp.ok());
    }
}
