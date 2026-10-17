#include "app/movement_controls.h"
#include "camera_fixture.h"
#include "engine/runtime.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include <cmath>
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
    constexpr SDL_WindowID window = 8;
    SDL_Event key(SDL_Scancode code, bool down, SDL_WindowID id = window, bool repeat = false)
    {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID = id;
        e.key.scancode = code;
        e.key.down = down;
        e.key.repeat = repeat;
        return e;
    }
    SDL_Event button(Uint8 code, bool down, SDL_WindowID id = window)
    {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.windowID = id;
        e.button.button = code;
        e.button.down = down;
        return e;
    }
    SDL_Event motion(float x, float y, SDL_WindowID id = window)
    {
        SDL_Event e{};
        e.type = SDL_EVENT_MOUSE_MOTION;
        e.motion.windowID = id;
        e.motion.xrel = x;
        e.motion.yrel = y;
        return e;
    }
    SDL_Event focus(bool active, SDL_WindowID id = window)
    {
        SDL_Event e{};
        e.type = active ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
        e.window.windowID = id;
        return e;
    }
    bool keys()
    {
        MovementControls c;
        c.event(focus(true), window);
        c.event(key(SDL_SCANCODE_F, true), window);
        CHECK(c.sample_fire(false).primary && !c.sample_fire(false).alternate);
        CHECK(c.consumes(key(SDL_SCANCODE_F, true), false) && !c.consumes(key(SDL_SCANCODE_F, true), true));
        c.event(key(SDL_SCANCODE_G, true), window);
        CHECK(c.sample_fire(false).primary && c.sample_fire(false).alternate);
        c.event(key(SDL_SCANCODE_F, false, 99), window);
        CHECK(c.sample_fire(false).primary);
        c.event(key(SDL_SCANCODE_F, false), window);
        CHECK(!c.sample_fire(false).primary && c.sample_fire(false).alternate);
        c.event(key(SDL_SCANCODE_G, false), window);
        CHECK(!c.sample_fire(false).alternate);
        c.event(key(SDL_SCANCODE_F3, true, 99), window);
        CHECK(!c.mouse_requested);
        c.event(key(SDL_SCANCODE_F3, true), window);
        CHECK(c.mouse_requested);
        c.event(key(SDL_SCANCODE_F3, true, window, true), window);
        CHECK(c.mouse_requested);
        c.event(key(SDL_SCANCODE_F3, false), window);
        c.event(key(SDL_SCANCODE_F3, true), window);
        CHECK(!c.mouse_requested);
        return true;
    }
    bool mouse()
    {
        MovementControls c;
        c.event(focus(true), window);
        c.event(button(SDL_BUTTON_LEFT, true), window);
        c.event(motion(40, 20), window);
        PlayerRotation r;
        c.look(r, .01, false);
        CHECK(r.yaw == 0 && !c.sample_fire(false).primary);
        c.mouse_requested = c.mouse_captured = true; // Pure mapping test; real capture is tested with SDL below.
        c.event(motion(40, 20, 99), window);
        c.look(r, .01, false);
        CHECK(r.yaw == 0);
        c.event(motion(20, 10), window);
        c.event(motion(20, 10), window);
        c.look(r, .001, false);
        CHECK(std::abs(r.yaw - .1f) < .0001f && std::abs(r.pitch - .05f) < .0001f);
        c.look(r, .1, false);
        CHECK(std::abs(r.yaw - .1f) < .0001f); // One consumption, no delta replay.
        c.event(motion(NAN, 2), window);
        c.look(r, .1, false);
        CHECK(std::abs(r.yaw - .1f) < .0001f);
        c.event(motion(0, 10000), window);
        c.look(r, .1, false);
        CHECK(r.pitch == 1.48f);
        c.event(motion(0, -10000), window);
        c.look(r, .1, false);
        CHECK(r.pitch == -1.48f);
        c.event(button(SDL_BUTTON_LEFT, true), window);
        c.event(button(SDL_BUTTON_RIGHT, true), window);
        CHECK(c.sample_fire(false).primary && c.sample_fire(false).alternate && c.consumes(motion(1, 1), false));
        c.event(button(SDL_BUTTON_LEFT, false), window);
        CHECK(!c.sample_fire(false).primary && c.sample_fire(false).alternate);
        c.event(button(SDL_BUTTON_RIGHT, false), window);
        CHECK(!c.sample_fire(false).alternate);
        return true;
    }
    bool gates()
    {
        MovementControls c;
        c.event(focus(true), window);
        c.event(key(SDL_SCANCODE_F, true), window);
        CHECK(!c.sample_fire(true).primary && !c.sample_fire(false).primary); // Typing may not become a held trigger.
        c.event(key(SDL_SCANCODE_F, false), window);
        CHECK(!c.sample_fire(false).primary);
        c.event(key(SDL_SCANCODE_F, true), window);
        CHECK(c.sample_fire(false).primary);
        c.enabled = false;
        CHECK(!c.sample_fire(false).primary);
        c.enabled = true;
        CHECK(!c.sample_fire(false).primary);
        c.event(focus(false), window);
        c.event(focus(true), window);
        c.event(key(SDL_SCANCODE_F, true, window, true), window);
        CHECK(!c.sample_fire(false).primary);
        c.event(key(SDL_SCANCODE_F, true), window);
        CHECK(c.sample_fire(false).primary);
        c.mouse_captured = c.mouse_requested = true;
        c.event(motion(40, 30), window);
        c.event(key(SDL_SCANCODE_ESCAPE, true), window);
        CHECK(!c.mouse_requested && c.mouse_x == 0 && !c.sample_fire(false).primary);
        c.mouse_requested = true;
        c.event(key(SDL_SCANCODE_F1, true), window);
        CHECK(!c.mouse_requested);
        c.event(motion(20, 20), window);
        PlayerRotation r;
        c.look(r, .1, true);
        c.look(r, .1, false);
        CHECK(r.yaw == 0 && r.pitch == 0);
        c.event(key(SDL_SCANCODE_F, true), window);
        c.mouse_requested = true;
        c.player_changed();
        CHECK(!c.mouse_requested && !c.sample_fire(false).primary && !c.sample_use(false));
        return true;
    }
    bool private_input(const std::filesystem::path& root, bool gpu)
    {
        Runtime runtime;
        std::string error;
        if (gpu)
            CHECK(runtime.initialize(true, error));
        GameSession g;
        Config config;
        config.data_dir = root;
        CHECK(g.initialize() && g.resources.mount_game(config, error) && g.request_world("01_SubwayA", error) &&
              g.enter_player(error));
        CHECK(test_camera::complete_subway_intro(g));
        auto* p = g.server_shell.Player(g.local_client);
        auto* w = p->m_InventoryMgr.GetWeapon(1);
        MovementControls controls;
        const auto id = gpu ? SDL_GetWindowID(runtime.window) : window;
        auto event = [&](SDL_Event e)
        {
            if (gpu)
                return SDL_PushEvent(&e);
            controls.event(e, id);
            return true;
        };
        auto pump = [&]()
        {
            if (gpu)
            {
                SDL_Event e{};
                while (SDL_PollEvent(&e))
                    controls.event(e, id);
                if (!controls.sync_mouse(runtime.window, g.client_shell.m_bInWorld, false, error))
                    return false;
            }
            controls.look(g.client_shell.rotation, 1. / 60, false);
            const auto fire = controls.sample_fire(false);
            g.client_shell.weapon_input = {fire.primary, fire.alternate, false};
            const float hp_before = p->m_damage.m_fHitPoints;
            if (g.step(1. / 60))
                return true;
            std::cerr << "Input session failed at " << g.objects.time() << " hp before " << hp_before << '\n';
            for (const auto& line : g.trace)
                std::cerr << "Trace " << line << '\n';
            for (const auto& line : g.objects.diagnostics())
                std::cerr << "Object " << line << '\n';
            return false;
        };
        auto steps = [&](unsigned count)
        {
            for (unsigned i = 0; i < count; ++i)
                if (!pump())
                    return false;
            return true;
        };
        CHECK(event(focus(true, id)) && steps(60));
        const auto ammo = p->m_InventoryMgr.GetAmmoCount(1);
        CHECK(event(key(SDL_SCANCODE_F, true, id)) && steps(90) && w->shots > 0 &&
              p->m_InventoryMgr.GetAmmoCount(1) < ammo);
        CHECK(event(key(SDL_SCANCODE_F, false, id)) && steps(60));
        if (gpu)
        {
            CHECK(event(key(SDL_SCANCODE_F3, true, id)) && pump() && controls.mouse_captured &&
                  SDL_GetWindowRelativeMouseMode(runtime.window));
            const auto rotation = g.client_shell.rotation;
            CHECK(event(motion(40, 20, id)) && pump());
            CHECK(std::abs(std::remainder(g.client_shell.rotation.yaw - rotation.yaw, 6.28318530718f) - .1f) < .0001f &&
                  std::abs(g.client_shell.rotation.pitch - rotation.pitch - .05f) < .0001f);
            const auto look = g.client_shell.view.rotation;
            CHECK(std::abs(rotate_vector(look, {0, 0, 1}).y + std::sin(g.client_shell.rotation.pitch)) < .0001f);
            const auto before = w->shots;
            CHECK(event(button(SDL_BUTTON_LEFT, true, id)) && steps(90) && w->shots > before);
            CHECK(event(button(SDL_BUTTON_LEFT, false, id)) && steps(60));
            const auto alt = w->shots;
            CHECK(event(button(SDL_BUTTON_RIGHT, true, id)) && steps(90) && w->shots > alt);
            CHECK(event(button(SDL_BUTTON_RIGHT, false, id)) && steps(60));
            CHECK(event(key(SDL_SCANCODE_ESCAPE, true, id)) && pump() && !controls.mouse_captured &&
                  !SDL_GetWindowRelativeMouseMode(runtime.window));
            CHECK(event(button(SDL_BUTTON_LEFT, true, id)) && pump() && !g.client_shell.weapon_input.primary);
            CHECK(event(key(SDL_SCANCODE_F3, false, id)) && event(key(SDL_SCANCODE_F3, true, id)) && pump() &&
                  controls.mouse_captured);
            CHECK(event(focus(false, id)) && pump() && !controls.mouse_captured &&
                  !SDL_GetWindowRelativeMouseMode(runtime.window));
            CHECK(event(focus(true, id)) && pump() && !controls.mouse_captured && !g.client_shell.weapon_input.primary);
        }
        else
        {
            const auto before = w->shots;
            CHECK(event(key(SDL_SCANCODE_G, true, id)) && steps(90));
            std::cout << "Alternate input: hp=" << p->m_damage.m_fHitPoints << " dead=" << p->m_damage.IsDead()
                      << " ammo=" << p->m_InventoryMgr.GetAmmoCount(1) << " shots=" << w->shots << " before=" << before
                      << '\n';
            CHECK(w->shots > before);
            CHECK(event(key(SDL_SCANCODE_G, false, id)) && steps(60));
        }
        CHECK(w->sounds_played == w->shots && !w->sound_failures);
        CHECK(g.client_shell.weapons.front().fire_keys > 0);
        unsigned corrected_corpses = 0;
        for (auto identity : g.objects.snapshot())
        {
            const auto* body = g.objects.get(identity);
            const auto* npc = body ? dynamic_cast<const AI_Mgr*>(body->game.get()) : nullptr;
            if (npc && !npc->death_error.empty())
                std::cout << "Death placement failure: " << body_placement_name(npc->corpse_placement.result)
                          << " corrections=" << npc->corpse_placement.corrections
                          << " distance=" << npc->corpse_placement.distance
                          << " candidate=" << npc->corpse_placement.candidate.x << ','
                          << npc->corpse_placement.candidate.y << ',' << npc->corpse_placement.candidate.z
                          << " polygon=" << npc->corpse_placement.polygon << " error=" << npc->death_error << '\n';
            const auto* corpse = body ? dynamic_cast<const CCorpse*>(body->game.get()) : nullptr;
            if (!corpse)
                continue;
            CHECK(placement_succeeded(corpse->placement));
            corrected_corpses += corpse->placement.result == BodyPlacementResult::Adjusted;
            CollisionHit hit;
            CHECK(g.collision.sweep(body->state.m_Pos, body->state.m_Pos, body->dimensions, hit) ==
                  CollisionQuery::Miss);
            std::cout << "Corpse placement: " << body_placement_name(corpse->placement.result)
                      << " corrections=" << corpse->placement.corrections << " distance=" << corpse->placement.distance
                      << " requested=" << corpse->placement.requested.x << ',' << corpse->placement.requested.y << ','
                      << corpse->placement.requested.z << " fitted=" << corpse->placement.candidate.x << ','
                      << corpse->placement.candidate.y << ',' << corpse->placement.candidate.z << '\n';
        }
        if (!gpu)
            CHECK(corrected_corpses > 0); // Actual keyboard-fire encounter; no health/aim/AI override.
        std::cout << "Original FIRE_KEY: shots=" << w->shots << " ammo=" << p->m_InventoryMgr.GetAmmoCount(1)
                  << " voices=" << w->sounds_played << " mouse=" << gpu << "\n";
        CHECK(g.leave_player() && g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    const std::string mode = argv[1];
    if (mode == "keys")
        return keys() ? 0 : 1;
    if (mode == "mouse")
        return mouse() ? 0 : 1;
    if (mode == "gates")
        return gates() ? 0 : 1;
    if (argc == 3 && (mode == "private" || mode == "gpu"))
        return private_input(argv[2], mode == "gpu") ? 0 : 1;
    return 2;
}
