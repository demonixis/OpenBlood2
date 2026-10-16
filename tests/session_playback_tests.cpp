#include "app/session_playback.h"
#include "game/original/AI_Mgr.h"
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool policy()
    {
        GameSession g;
        CHECK(g.initialize());
        CHECK(!waiting_for_player(g) && step_app_session(g, .1) && g.objects.time() == .1);
        g.world_path = "synthetic policy fixture";
        CHECK(waiting_for_player(g));
        for (int frame = 0; frame < 120; ++frame)
            CHECK(step_app_session(g, .1));
        CHECK(g.objects.time() == .1);
        const auto h = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(h && g.objects.remove(h) && g.objects.get(h));
        CHECK(step_app_session(g, .1) && !g.objects.get(h) && g.objects.time() == .1);
        CHECK(!waiting_for_player(g, true) && step_app_session(g, .1, true) && g.objects.time() == .2);
        return true;
    }
    bool npcs_ready(GameSession& g, bool untouched)
    {
        unsigned count = 0;
        for (auto id : g.objects.snapshot())
            if (const auto* a = dynamic_cast<AI_Mgr*>(g.objects.get(id)->game.get()))
            {
                if (a->runtime_failed)
                    std::cerr << a->runtime_error << '\n';
                CHECK(a->RuntimeReady() && (!untouched || !a->runtime_updates));
                ++count;
            }
        CHECK(count == 15);
        return true;
    }
    bool private_entry(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        ObjectId previous{};
        for (int load = 0; load < 2; ++load)
        {
            CHECK(g.request_world("01_SubwayA", error) && !g.objects.get(previous));
            const auto time = g.objects.time();
            const auto before = g.objects.snapshot();
            for (int frame = 0; frame < 180; ++frame)
                CHECK(waiting_for_player(g) && step_app_session(g, 1. / 60));
            CHECK(g.objects.time() == time && g.objects.snapshot() == before && npcs_ready(g, true));
            CHECK(g.enter_player(error) && !waiting_for_player(g));
            auto* player = g.server_shell.Player(g.local_client);
            CHECK(player);
            previous = g.objects.get(player->m_hObject)->id;
            // Run the real camera sequence; do not disable its gameplay gate.
            CHECK(g.client_shell.view.object && !g.client_shell.view.settings.player_movement);
            bool second_camera = false;
            for (int frame = 0; frame < 780; ++frame)
            {
                CHECK(step_app_session(g, 1. / 60));
                if (const auto* camera = g.objects.get(g.client_shell.view.object))
                    second_camera |= std::string_view(camera->state.m_Name) == "camera2";
            }
            CHECK(second_camera && !g.client_shell.view.object && npcs_ready(g, false));
            // Explicit encounter fixture within the real train: move only the player.
            // Authored NPC positions, weapons, states, perception and activation stay intact.
            DVector encounter{-32, 170, -1880};
            CHECK(g.server.TeleportObject(player->m_hObject, &encounter) == DE_OK &&
                  player->TeleportClientToServerPos());
            const float hp = player->m_damage.m_fHitPoints;
            for (int frame = 0; frame < 300; ++frame)
                CHECK(step_app_session(g, 1. / 60));
            unsigned shooters = 0;
            std::uint64_t shots = 0;
            for (auto id : g.objects.snapshot())
                if (const auto* a = dynamic_cast<AI_Mgr*>(g.objects.get(id)->game.get()))
                {
                    shooters += a->fire_shots > 0;
                    shots += a->fire_shots;
                }
            CHECK(npcs_ready(g, false) && shooters > 0 && shots > 0 && player->m_damage.m_fHitPoints < hp);
            std::cout << "SubwayA entry " << load << ": waited 180 frames without ticking; shooters=" << shooters
                      << " shots=" << shots << " player HP=" << player->m_damage.m_fHitPoints << '\n';
            CHECK(g.leave_player());
            const double left_at = g.objects.time();
            for (int frame = 0; frame < 60; ++frame)
                CHECK(waiting_for_player(g) && step_app_session(g, 1. / 60));
            CHECK(g.objects.time() == left_at && npcs_ready(g, false));
            CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.objects.get(previous));
            CHECK(!AI_Mgr::m_dwNumCabal && !AI_Mgr::m_dwNumMonster && !g.server.client_count() &&
                  !g.server.string_count() && !g.server.message_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "policy")
        return policy() ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "private")
        return private_entry(argv[2]) ? 0 : 1;
    return 2;
}
