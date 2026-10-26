// 03_Museum's third Gideon hologram: Trigger3 spawns two Fanatics just above the skylight
// glass SL42-SL44 and KILLs the glass at the same instant (MessageDelay 7). The spawned
// body starts inside the still-present brush, whose removal is deferred to the frame end;
// the strict start-penetration failure stopped the whole simulation (tester crash report).
#include "game/server_adapter.h"
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << "\n";                                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool museum(const char* root)
    {
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game->resources.mount_game(config, error));
        CHECK(GameSession::start_new_game(game, "03_Museum", 1, 0, error));
        auto& g = *game;
        for (int i = 0; i < 30; ++i)
            CHECK(g.step(1.0 / 60));
        // Into hologideon3trigger and Trigger3, which start the cutscene and the spawn.
        auto* player = g.server_shell.Player(g.local_client);
        DVector feet{176, 448, 3200};
        feet.y += g.objects.get(g.client_shell.player)->dimensions.y + .01f;
        CHECK(g.server.TeleportObject(g.client_shell.player, &feet) == DE_OK && player->TeleportClientToServerPos() &&
              g.pump_player_messages(error));
        bool camera = false;
        for (int i = 0; i < 20 * 60; ++i)
        {
            if (!g.step(1.0 / 60))
            {
                for (const auto& line : g.step_errors())
                    std::cerr << line << '\n';
                return false;
            }
            camera |= g.client_shell.view.object != nullptr;
        }
        unsigned fanatics = 0, passes = 0;
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive && r->type->name == "Fanatic")
            {
                ++fanatics;
                passes += r->brush_start_passes > 0;
            }
        std::cout << "fanatics " << fanatics << " passed out of a brush " << passes << '\n';
        CHECK(camera && passes > 0 && !g.client_shell.view.object);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view name = argv[1];
    if (name == "museum")
        return museum(argv[2]) ? 0 : 1;
    return 2;
}
