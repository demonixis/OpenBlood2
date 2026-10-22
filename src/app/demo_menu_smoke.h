#pragma once
#include "app/demo_smoke.h"
namespace OpenBlood2
{
    // Real frontend traversal with isolated preferences. Authored exits are diagnostic commands.
    struct DemoMenuSmoke
    {
        bool confirmed = false;
        double frozen_time = 0;
        std::uint64_t frozen_audio = 0, menu_audio = 0;
        float frozen_focus = 0;
        static std::uint64_t phase(std::uint64_t frame)
        {
            return frame >= 216 ? frame - 180 : frame;
        }
        static bool capture(std::uint64_t frame)
        {
            if (frame >= 36 && frame < 216)
                return frame == 40;
            const auto p = phase(frame);
            return p == 2 || p == 4 || p == 8 || p == 10 || p == 15 || p == 19 || p == 23 || p == 27 || p == 32 ||
                   p == 54 || p == 61 || p == 63 || p == 72;
        }
        bool before(std::uint64_t frame, SDL_Window* window, GameSession& game, const Menu& menu, MenuInput& input,
                    const MenuAudio& audio, std::string& error)
        {
            const auto p = phase(frame);
            auto fail = [&](const char* why)
            {
                error = "Demo menu smoke frame " + std::to_string(frame) + ": " + why;
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
            if (!menu.error.empty() || !audio.error.empty() || audio.failures)
                return fail("menu/resource/audio failure");
            if (frame == 0)
            {
                if (!menu.active || !menu.demo() || game.client_shell.m_bInWorld || input.demo_mode)
                    return fail("initial menu ownership");
                focus(true);
            }
            if (frame == 36)
            {
                if (!menu.active || !game.server.audio.paused() || input.controls.mouse_requested)
                    return fail("pause admission");
                frozen_time = game.objects.time();
                frozen_audio = game.server.audio.mixed_frames;
                menu_audio = audio.mixer.mixed_frames;
                frozen_focus = game.server_shell.Player(game.local_client)->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS);
            }
            if (frame > 36 && frame <= 216)
            {
                if (game.objects.time() != frozen_time || game.server.audio.mixed_frames != frozen_audio ||
                    game.server_shell.Player(game.local_client)->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) !=
                        frozen_focus)
                    return fail("paused simulation/audio/Focus changed");
                if (frame == 216 && audio.mixer.mixed_frames < menu_audio + 48000 * 3)
                    return fail("menu mixer did not advance independently");
            }
            if (frame >= 36 && frame < 216)
                return true; // More than three seconds of menu pause.
            if (p == 4 && menu.promotion != MenuPromotion::Bloodbath)
                return fail("Bloodbath promotion");
            if (menu.promotion != MenuPromotion::None && audio.ambient)
                return fail("promotion retained menu ambience");
            if (p == 7 && (menu.promotion != MenuPromotion::None || menu.selected != 0))
                return fail("promotion return/selection");
            if (p == 10 && menu.page != MenuPage::Display)
                return fail("display page");
            if (p == 15 && (menu.page != MenuPage::Audio || menu.settings.sound_volume != 45))
                return fail("audio option");
            if (p == 19 && (menu.page != MenuPage::Mouse || menu.items.size() != 3 ||
                            menu.settings.sensitivity != 170 || !menu.settings.invert_y))
                return fail("mouse options");
            if (p == 23 && (menu.promotion != MenuPromotion::Info || menu.promotion_exit))
                return fail("Prime Gibs promotion");
            if (p == 27 && menu.promotion != MenuPromotion::Modes)
                return fail("Chosen promotion");
            // Each StartNewWorld presents the loading screen, then the demo's load screen IDs wait.
            if ((p == 31 || p == 54 || p == 67) && (!input.loading_screen || input.loading_wait))
                return fail("loading screen before the load");
            if (p == 32 && (!game.client_shell.m_bInWorld || menu.active || game.world_path != "worlds/demo_01.dat" ||
                            game.server.GetVarValueFloat(game.server.GetGameConVar("GAME_DIFFICULTY")) != 2 ||
                            !input.loading_wait))
                return fail("hard Caleb new game");
            if (p == 43 && (menu.active || game.objects.time() != frozen_time))
                return fail("resume catch-up");
            if (p == 44 && game.objects.time() <= frozen_time)
                return fail("resume simulation");
            if (p == 47 && !game.server_shell.Player(game.local_client)->m_bDead)
                return fail("source death");
            if (p == 49 && (game.server_shell.Player(game.local_client)->m_bDead || game.world_revision < 2 ||
                            game.server.GetVarValueFloat(game.server.GetGameConVar("GAME_DIFFICULTY")) != 2))
                return fail("Enter retry/difficulty");
            if (p == 55 && (game.world_path != "worlds/demo_02.dat" ||
                            game.level_change.selected_start != "05_egress" || !input.loading_wait))
                return fail("authored NEW_LEVEL");
            if (p == 61 &&
                (game.client_shell.m_bInWorld || !menu.active || menu.in_world ||
                 menu.promotion != MenuPromotion::Info || menu.promotion_exit || !game.server.audio.paused()))
                return fail("EndGame unload/promotion");
            if (p == 63 && (!menu.active || menu.page != MenuPage::Main || menu.promotion != MenuPromotion::None))
                return fail("EndGame returns to main menu");
            if (p == 68 && (!game.client_shell.m_bInWorld || menu.active || game.world_path != "worlds/demo_01.dat" ||
                            game.server.GetVarValueFloat(game.server.GetGameConVar("GAME_DIFFICULTY")) != 0 ||
                            !input.loading_wait))
                return fail("new game after ending");
            if (p == 72)
            {
                if (menu.promotion != MenuPromotion::Info || !menu.promotion_exit)
                    return fail("quit promotion");
                confirmed = true;
            }
            switch (p)
            {
                case 3:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 5:
                    key(SDL_SCANCODE_SPACE, true);
                    break;
                case 6:
                    key(SDL_SCANCODE_SPACE);
                    break;
                case 8:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 9:
                    key(SDL_SCANCODE_RETURN);
                    key(SDL_SCANCODE_LEFT);
                    key(SDL_SCANCODE_RIGHT);
                    break;
                case 12:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 13:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 14:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_LEFT);
                    break;
                case 16:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 17:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 18:
                    key(SDL_SCANCODE_RIGHT);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 20:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 21:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 22:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 24:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 25:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 26:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 28:
                    key(SDL_SCANCODE_SPACE);
                    break;
                case 29:
                    key(SDL_SCANCODE_RETURN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 30:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    key(SDL_SCANCODE_RETURN, true);
                    break;
                case 32: // GS_WAITING: any key continues.
                case 55:
                case 68:
                    key(SDL_SCANCODE_SPACE);
                    break;
                case 35:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 38:
                    focus(false);
                    break;
                case 39:
                    focus(true);
                    break;
                case 40:
                    key(SDL_SCANCODE_F10);
                    break;
                case 41:
                    key(SDL_SCANCODE_F10);
                    break;
                case 42:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 46:
                {
                    auto* player = game.server_shell.Player(game.local_client);
                    auto message = game.server.StartMessageToObject(player, player->m_hObject, damage_message);
                    if (!message ||
                        game.server.WriteDamage(
                            message, {{0, 0, 1}, {}, 1000, DamageType::Normal, player->m_hObject}) != DE_OK ||
                        game.server.EndMessage(message) != DE_OK)
                        return fail("lethal damage delivery");
                    break;
                }
                case 48:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 53:
                case 60:
                {
                    HOBJECT target = nullptr;
                    for (auto id : game.objects.snapshot())
                        if (auto* r = game.objects.get(id); r && dynamic_cast<ExitTrigger*>(r->game.get()))
                        {
                            target = r->handle;
                            break;
                        }
                    if (!target)
                        return fail("authored exit absent");
                    auto text = game.server.CreateString("TRIGGER");
                    auto message = game.server.StartMessageToObject(
                        game.server.HandleToObject(game.client_shell.player), target, 1000);
                    const bool sent = message && game.server.WriteToMessageHString(message, text) == DE_OK &&
                                      game.server.EndMessage(message) == DE_OK;
                    game.server.FreeString(text);
                    if (!sent || !game.pump_player_messages(error))
                        return fail("authored exit delivery");
                    break;
                }
                case 62:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 64:
                case 65:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 66:
                    key(SDL_SCANCODE_UP);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 70:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 71:
                    key(SDL_SCANCODE_UP);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 73:
                    key(SDL_SCANCODE_SPACE);
                    break;
                default:
                    break;
            }
            return p <= 73 || fail("quit was not consumed");
        }
    };
} // namespace OpenBlood2
