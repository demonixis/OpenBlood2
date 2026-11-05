#include "game/game_settings.h"
#include "server/session.h"
#include <iostream>

int main(int argc, char** argv)
{
    using namespace OpenBlood2;
    if (argc != 2)
        return 2;
    Config config;
    config.data_dir = path_from_utf8(argv[1]);
    Resources resources;
    std::string error;
    if (!resources.mount_game(config, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    unsigned count = 0, passed = 0;
    for (const auto& [path, resource] : resources.index())
    {
        (void)resource;
        const bool ctf = path.starts_with("worlds/multi/ctf_");
        if ((!ctf && !path.starts_with("worlds/multi/bb_")) || !path.ends_with(".dat"))
            continue;
        ++count;
        ServerSession session;
        bool ok = session.start(config, path, ctf ? GAMETYPE_CTF : GAMETYPE_BLOODBATH, error);
        for (unsigned tick = 0; ok && tick < 180; ++tick)
            ok = session.tick(error);
        passed += ok;
        std::cout << (ok ? "PASS " : "FAIL ") << path << " zero clients / 180 ticks";
        if (!ok)
            std::cout << ": " << error;
        std::cout << '\n' << std::flush;
    }
    std::cout << passed << '/' << count << " dedicated world simulations; no player admission\n";
    return count == 22 && passed == count ? 0 : 1;
}
