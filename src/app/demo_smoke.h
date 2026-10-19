#pragma once
#include "app/menu_input.h"
#include "app/menu_smoke.h"
#include "game/original/ExitTrigger.h"

namespace OpenBlood2
{
    // Labelled diagnostic: invokes authored ExitTriggers, not a claimed full player walkthrough.
    struct DemoSmoke
    {
        bool confirmed = false;
        double frozen_time = 0;
        std::uint64_t frozen_audio = 0;
        float frozen_focus = 0;
        bool before(std::uint64_t frame, SDL_Window* window, GameSession& game, MenuInput& input, std::string& error)
        {
            auto fail = [&](const char* reason)
            {
                error = "Demo smoke frame " + std::to_string(frame) + ": " + reason;
                return false;
            };
            auto key = [&](SDL_Scancode code, bool repeat = false)
            {
                SDL_Event e{};
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.windowID = SDL_GetWindowID(window);
                e.key.which = MenuSmoke::device;
                e.key.scancode = code;
                e.key.repeat = repeat;
                SDL_PushEvent(&e);
                e.type = SDL_EVENT_KEY_UP;
                e.key.repeat = false;
                SDL_PushEvent(&e);
            };
            auto focus = [&](bool gained)
            {
                SDL_Event e{};
                e.type = gained ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
                e.window.windowID = SDL_GetWindowID(window);
                e.window.data1 = int(MenuSmoke::device);
                SDL_PushEvent(&e);
            };
            if (frame == 0)
            {
                if (!game.client_shell.m_bInWorld || game.world_path != "worlds/demo_01.dat" || input.inspector)
                    return fail("direct first-world ownership");
                focus(true);
            }
            if (frame == 6)
                key(SDL_SCANCODE_ESCAPE);
            if (frame == 7)
            {
                if (!input.demo.paused() || !game.server.audio.paused())
                    return fail("pause admission");
                frozen_time = game.objects.time();
                frozen_audio = game.server.audio.mixed_frames;
                frozen_focus = game.server_shell.Player(game.local_client)->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS);
            }
            if (frame >= 8 && frame <= 15)
            {
                if (game.objects.time() != frozen_time || game.server.audio.mixed_frames != frozen_audio ||
                    game.server_shell.Player(game.local_client)->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) !=
                        frozen_focus)
                    return fail("simulation/audio/Focus advanced in pause or on resume frame");
            }
            if (frame == 9)
                key(SDL_SCANCODE_ESCAPE, true);
            if (frame == 14)
                key(SDL_SCANCODE_ESCAPE);
            if (frame == 16 && game.objects.time() <= frozen_time)
                return fail("resume did not advance");
            if (frame == 18)
                focus(false);
            if (frame == 19 && !input.demo.paused())
                return fail("focus loss did not pause");
            if (frame == 20)
                focus(true);
            if (frame == 21 && !input.demo.paused())
                return fail("focus gain resumed automatically");
            if (frame == 22)
                key(SDL_SCANCODE_ESCAPE);
            if (frame == 26)
            {
                auto* player = game.server_shell.Player(game.local_client);
                if (!player)
                    return fail("death player missing");
                auto message = game.server.StartMessageToObject(player, player->m_hObject, damage_message);
                if (!message ||
                    game.server.WriteDamage(message, {{0, 0, 1}, {}, 1000, DamageType::Normal, player->m_hObject}) !=
                        DE_OK ||
                    game.server.EndMessage(message) != DE_OK)
                    return fail("lethal damage delivery");
            }
            if (frame == 27 && !game.server_shell.Player(game.local_client)->m_bDead)
                return fail("source death not committed");
            if (frame == 28)
                key(SDL_SCANCODE_RETURN);
            if (frame == 29)
            {
                const auto* player = game.server_shell.Player(game.local_client);
                if (!player || player->m_bDead || player->m_damage.IsDead() ||
                    game.world_path != "worlds/demo_01.dat" || game.world_revision < 2)
                    return fail("fresh same-world restart");
            }
            if (frame == 40 || frame == 60)
            {
                HOBJECT target = nullptr;
                for (auto id : game.objects.snapshot())
                    if (auto* r = game.objects.get(id); r && dynamic_cast<ExitTrigger*>(r->game.get()))
                    {
                        target = r->game->m_hObject;
                        break;
                    }
                if (!target)
                    return fail("authored ExitTrigger absent");
                auto text = game.server.CreateString("TRIGGER");
                auto message = game.server.StartMessageToObject(game.server.HandleToObject(game.client_shell.player),
                                                                target, 1000);
                const bool sent = message && game.server.WriteToMessageHString(message, text) == DE_OK &&
                                  game.server.EndMessage(message) == DE_OK;
                game.server.FreeString(text);
                if (!sent || !game.pump_player_messages(error))
                    return fail("authored exit delivery");
            }
            if (frame == 41 && (game.world_path != "worlds/demo_02.dat" || !game.resources.demo() ||
                                game.level_change.selected_start != "05_egress"))
                return fail("second-world transfer");
            if (frame == 61)
            {
                if (!input.demo.finished() || game.client_shell.m_bInWorld || !game.server.audio.paused())
                    return fail("terminal ownership/audio");
                confirmed = true;
            }
            if (frame == 65)
                key(SDL_SCANCODE_RETURN);
            if (frame > 65)
                return fail("terminal input failed to quit");
            return true;
        }
    };
} // namespace OpenBlood2
