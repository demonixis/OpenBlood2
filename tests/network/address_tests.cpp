#include "engine/config.h"
#include "net/address.h"
#include <array>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    Net::ServerAddress address;
    std::string error;
    CHECK(Net::parse_address("127.0.0.1", 27888, address, error) && address.port == 27888);
    CHECK(Net::parse_address("my-host.local:65535", 27888, address, error) && address.host == "my-host.local" &&
          address.port == 65535);
    for (std::string_view bad : {"", ":27888", "[::1]", "host:", "host:0", "host:65536", "host:-1", "host:1:2",
                                 "host:1x", "host/path", "host name"})
    {
        CHECK(!Net::parse_address(bad, 27888, address, error));
        CHECK(address.host == "my-host.local" && address.port == 65535);
    }
    CHECK(!Net::parse_address("host", 0, address, error));
    Options options;
    const std::array<std::string_view, 6> host{"--host", "multi/bb_bloodarena", "--port", "0", "--name", "Caleb"};
    CHECK(parse_options(host, options, error) && options.host_world == "multi/bb_bloodarena" && !options.network_port &&
          options.network_name == "Caleb");
    const std::array<std::string_view, 2> connect{"--connect", "localhost:27888"};
    options = {};
    CHECK(parse_options(connect, options, error) && options.connect_address == "localhost:27888");
    for (const auto& args : {std::vector<std::string_view>{"--connect", "host", "--host", "world"},
                             {"--host", "world", "--demo"},
                             {"--host", "world", "--view-world"},
                             {"--connect", "host", "--port", "65536"},
                             {"--connect"},
                             {"--port", "0"}})
    {
        options = {};
        CHECK(!parse_options(args, options, error));
    }
    std::cout << "IPv4 endpoint parsing and network CLI isolation passed\n";
}
