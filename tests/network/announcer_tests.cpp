#include "client/announcer.h"
#include <iostream>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        Resources resources;
        std::string error;
        if (data)
        {
            Config config;
            config.data_dir = data;
            CHECK(resources.mount_game(config, error));
        }
        ClientAnnouncer announcer;
        for (unsigned i = 0; i < 8; ++i)
        {
            const auto first = announcer.bloodbath({static_cast<std::uint16_t>(i)}, resources);
            const auto wrapped = announcer.bloodbath({static_cast<std::uint16_t>(i + 65528)}, resources);
            CHECK(first.character == 4 && first.file == wrapped.file && first.text == wrapped.text);
            CHECK(first.file == "Sounds\\Voice\\Voice" + std::to_string(248 + i) + ".wav");
            CHECK(!first.text.empty() && first.text.size() < 500);
            if (data)
                CHECK(resources.find(first.file));
        }
        CHECK(announcer.messages == 16);
        if (data)
            CHECK(announcer.error.empty() && announcer.fallback_texts == 0);
        else
            CHECK(!announcer.error.empty() && announcer.fallback_texts == 16);
        ClientAnnouncer other;
        CHECK(other.messages == 0);
        announcer = {};
        CHECK(announcer.messages == 0 && announcer.error.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc > 2)
        return 2;
    return run(argc == 2 ? argv[1] : nullptr) ? 0 : 1;
}
