#include "game/original/AI_Mgr.h"
#include "game/server_adapter.h"
#include <iostream>
using namespace OpenBlood2;
int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    for (unsigned difficulty : {0u, 1u, 2u})
    {
        GameSession g;
        Config config;
        config.data_dir = argv[1];
        std::string error;
        if (!g.initialize() || !g.resources.mount_game(config, error))
            return 1;
        g.server.set_game_settings(difficulty, 0);
        if (!g.request_world("end", error))
        {
            std::cerr << error << '\n';
            return 1;
        }
        if (!g.enter_player(error))
        {
            std::cerr << error << '\n';
            return 1;
        }
        if (!g.step(.002))
            return 1;
        bool caleb_script = false;
        for (auto id : g.objects.snapshot())
        {
            auto* r = g.objects.get(id);
            auto* ai = dynamic_cast<AI_Mgr*>(r->game.get());
            if (!ai)
                continue;
            if (!ai->RuntimeReady())
                return 1;
            if (std::string_view(r->state.m_Name) == "CalebAI0")
            {
                const auto clip = g.server.GetAnimIndex(ai->m_hObject, "walk_nogun");
                if (ai->CurrentScriptCommand().argument != "walk_nogun" || clip == UINT32_MAX ||
                    ai->requested_animation != clip || r->animation.animation != clip || ai->animation_clip_retained ||
                    ai->animation_blocked || r->animation.looping)
                    return 1;
                caleb_script = true;
            }
        }
        if (!caleb_script)
        {
            std::cerr << "No authored Caleb walk script was observed\n";
            return 1;
        }
        for (unsigned i = 0; i < 180; ++i)
            if (!g.step(1. / 60))
                return 1;
        if (!g.run_world(error))
        {
            std::cerr << error << '\n';
            return 1;
        }
        std::cout << "End difficulty=" << difficulty << " script and runtime ready\n";
        if (!g.unload_world() || !g.objects.snapshot().empty() || g.server.message_count() || g.server.string_count())
            return 1;
    }
    return 0;
}
