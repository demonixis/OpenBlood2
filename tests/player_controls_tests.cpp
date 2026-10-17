#include "app/movement_controls.h"
#include "camera_fixture.h"
#include "engine/runtime.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include "game/original/CultistAI.h"
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
    bool entry()
    {
        MovementControls c;
        std::string error;
        c.enabled = false;
        c.keys[SDL_SCANCODE_F] = true;
        c.player_changed(true); // Ordinary inspector entry, not an F3 shortcut.
        CHECK(c.enabled && c.mouse_requested && !c.mouse_captured && !c.keys[SDL_SCANCODE_F]);
        // No native call while focus is pending: startup must not lose play intent.
        CHECK(c.sync_mouse(nullptr, true, false, error) && c.mouse_requested);
        CHECK(c.sync_mouse(nullptr, true, false, error) && c.mouse_requested);
        c.event(focus(false), window);
        CHECK(!c.mouse_requested); // Explicit loss cancels even a pending capture.
        c.event(focus(true), window);
        CHECK(!c.mouse_requested);
        c.enabled = false;
        c.event(key(SDL_SCANCODE_F3, true), window);
        CHECK(c.enabled && c.mouse_requested);
        c.player_changed(); // Leave/unload must not carry input to the next player.
        CHECK(!c.mouse_requested);
        c.request_play();
        CHECK(c.sync_mouse(nullptr, false, false, error) && !c.mouse_requested);
        c.request_play();
        CHECK(c.sync_mouse(nullptr, true, true, error) && !c.mouse_requested);
        return true;
    }
    bool combat(const std::filesystem::path& root, bool gpu, bool clearance = false)
    {
        Runtime runtime;
        std::string error;
        if (gpu)
            CHECK(runtime.initialize(true, error));
        GameSession g;
        Config config;
        config.data_dir = root;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        ObjectId previous_target{}, previous_player{}, previous_corpse{};
        for (unsigned run = 0; run < 2; ++run)
        {
            const bool alternate = !clearance && run == 1;
            const double dt = clearance && run == 0 ? 1. / 30 : 1. / 60;
            const float stop = clearance ? -2350 : -2200;
            const float aim_offset = clearance ? -30 : 0;
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            CHECK(!g.objects.get(previous_target) && !g.objects.get(previous_player) &&
                  !g.objects.get(previous_corpse));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p && test_camera::complete_subway_intro(g));
            auto* w = p->m_InventoryMgr.GetWeapon(1);
            CHECK(w);
            const auto id = gpu ? SDL_GetWindowID(runtime.window) : window;
            MovementControls controls;
            controls.player_changed(true); // Same entry action as the interactive application.
            CHECK(!controls.focused && controls.mouse_requested);
            CHECK(controls.sync_mouse(gpu ? runtime.window : nullptr, true, false, error));
            CHECK(controls.mouse_requested && !controls.mouse_captured);
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
                    if (!controls.sync_mouse(runtime.window, true, false, error))
                        return false;
                }
                else
                    controls.mouse_captured = controls.mouse_requested; // Headless mapping only.
                controls.look(g.client_shell.rotation, dt, false);
                const auto fire = controls.sample_fire(false);
                g.client_shell.weapon_input = {fire.primary, fire.alternate, false};
                g.client_shell.movement_input = controls.sample(false);
                g.client_shell.use_down = controls.sample_use(false);
                return g.step(dt);
            };
            CHECK(event(focus(true, id)) && pump() && controls.mouse_captured);
            if (gpu)
                CHECK(SDL_GetWindowRelativeMouseMode(runtime.window));
            for (unsigned encounter = 0; encounter < (clearance ? 2u : 1u); ++encounter)
            {
                const float approach = encounter ? -1750 : stop;
                const float beyond = encounter ? -1350 : -1870;
                // Walk from the actual spawn into the first carriage, using original
                // player USE and door rules. Never teleport or alter an authored NPC.
                CHECK(event(key(SDL_SCANCODE_W, true, id)));
                for (int frame = 0; frame < 720 && g.objects.get(p->m_hObject)->state.m_Pos.z < approach; ++frame)
                {
                    if (frame % 60 == 0 || frame % 60 == 1)
                        CHECK(event(key(SDL_SCANCODE_E, frame % 60 == 0, id)));
                    CHECK(pump() && !p->m_damage.IsDead());
                }
                CHECK(event(key(SDL_SCANCODE_W, false, id)) && event(key(SDL_SCANCODE_E, false, id)) && pump());
                CHECK(g.objects.get(p->m_hObject)->state.m_Pos.z >= approach);
                std::cout << "Approached encounter at z=" << g.objects.get(p->m_hObject)->state.m_Pos.z << '\n';
                // Select an actual visible authored Cultist. No NPC health, flags, pose,
                // target, activation or inventory are changed.
                ObjectId target{};
                float distance = 1e9f;
                for (auto identity : g.objects.snapshot())
                {
                    const auto* record = g.objects.get(identity);
                    const auto* npc = record ? dynamic_cast<const CultistAI*>(record->game.get()) : nullptr;
                    if (!npc || !npc->RuntimeReady() || npc->m_damage.IsDead())
                        continue;
                    DVector head;
                    DRotation rotation;
                    CHECK(g.server.GetModelNodeTransform(record->handle, "head", &head, &rotation));
                    SegmentHit hit;
                    const auto ray = g.server.IntersectSegment(
                        g.client_shell.view.position, head, p->m_hObject, hit, false, SegmentObjects::SolidOrRayHit,
                        SegmentFilter::IgnoreVolumes, SegmentStart::SkipInterior);
                    const auto delta = vector_subtract(head, g.client_shell.view.position);
                    const auto d = vector_dot(delta, delta);
                    if (ray == SegmentResult::Hit && hit.object == record->handle && d < distance)
                    {
                        distance = d;
                        target = identity;
                    }
                }
                CHECK(target);
                const auto* record = g.objects.get(target);
                const std::string name = record->state.m_Name;
                const auto target_handle = record->handle;
                const auto initial_hp = dynamic_cast<const CultistAI*>(record->game.get())->m_damage.m_fHitPoints;
                const float ammo = p->m_InventoryMgr.GetAmmoCount(1);
                const auto shots_before = w->shots;
                bool damaged = false, dead = false, player_attributed = false;
                HOBJECT corpse = nullptr;
                std::uint64_t dodges = 0;
                CHECK(event(button(alternate ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT, true, id)));
                for (int frame = 0; frame < 600 && g.objects.get(target); ++frame)
                {
                    auto* actor = dynamic_cast<CultistAI*>(g.objects.get(target)->game.get());
                    CHECK(actor && !actor->runtime_failed && !p->m_damage.IsDead());
                    DVector head;
                    DRotation pose;
                    CHECK(g.server.GetModelNodeTransform(target_handle, "head", &head, &pose));
                    head.y += encounter
                                  ? 0
                                  : aim_offset; // Body-aim regression uses actual mouse motion, not damage injection.
                    const auto delta = vector_subtract(head, g.client_shell.view.position);
                    const float yaw = std::atan2(delta.x, delta.z);
                    const float pitch = -std::atan2(delta.y, std::hypot(delta.x, delta.z));
                    CHECK(event(motion(std::remainder(yaw - g.client_shell.rotation.yaw, 6.28318530718f) / .0025f,
                                       (pitch - g.client_shell.rotation.pitch) / .0025f, id)));
                    CHECK(pump()); // Only SDL mouse motion/buttons feed view and original FIRE_KEY.
                    if (auto* current = g.objects.get(target))
                    {
                        actor = dynamic_cast<CultistAI*>(current->game.get());
                        dodges = std::max(dodges, actor->dodge_updates);
                        damaged |= actor->m_damage.m_fHitPoints < initial_hp;
                        dead |= actor->m_damage.IsDead();
                        player_attributed |= actor->m_damage.GetLastDamager() == p->m_hObject;
                        if (actor->last_corpse)
                            corpse = actor->last_corpse;
                        if (actor->runtime_failed)
                            std::cerr << name << ": " << actor->runtime_error << " " << actor->death_error
                                      << " state=" << actor->m_nState << " last=" << actor->m_nLastState
                                      << " phase=" << actor->Metacmd << " command=" << actor->m_nCurMetacmd
                                      << " placement=" << body_placement_name(actor->corpse_placement.result)
                                      << " corrections=" << actor->corpse_placement.corrections
                                      << " distance=" << actor->corpse_placement.distance
                                      << " anchor=" << actor->corpse_placement.anchor.x << ','
                                      << actor->corpse_placement.anchor.y << ',' << actor->corpse_placement.anchor.z
                                      << " requested=" << actor->corpse_placement.requested.x << ','
                                      << actor->corpse_placement.requested.y << ','
                                      << actor->corpse_placement.requested.z
                                      << " candidate=" << actor->corpse_placement.candidate.x << ','
                                      << actor->corpse_placement.candidate.y << ','
                                      << actor->corpse_placement.candidate.z
                                      << " polygon=" << actor->corpse_placement.polygon << '\n';
                        CHECK(!actor->runtime_failed);
                    }
                }
                CHECK(damaged && dead && player_attributed && !g.objects.get(target) && corpse);
                CHECK(!alternate || dodges > 0);
                const auto* body = g.objects.get(corpse);
                const auto* dead_body = body ? dynamic_cast<const CCorpse*>(body->game.get()) : nullptr;
                CHECK(dead_body && placement_succeeded(dead_body->placement));
                CHECK(!clearance || encounter || dead_body->placement.source_sweep);
                CHECK(w->shots > 0 && w->m_bLastFireAlt == alternate && w->sounds_played == w->shots &&
                      p->m_InventoryMgr.GetAmmoCount(1) < ammo && g.client_shell.weapons.front().fire_keys > 0);
                CHECK(event(button(alternate ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT, false, id)) && pump());
                std::cout << "Mouse " << (alternate ? "secondary" : "primary") << " killed authored " << name << ": HP "
                          << initial_hp << " -> 0, shots=" << w->shots - shots_before
                          << ", ammo=" << p->m_InventoryMgr.GetAmmoCount(1)
                          << ", independent corpse verified, native=" << gpu << '\n';
                std::cout << "Dodge updates=" << dodges << ", corpse candidates=" << dead_body->placement.candidates
                          << ", zero-margin search=" << dead_body->placement.touching_fit
                          << ", swept source clearance=" << dead_body->placement.source_sweep << '\n';
                CHECK(
                    event(motion(-g.client_shell.rotation.yaw / .0025f, -g.client_shell.rotation.pitch / .0025f, id)) &&
                    event(key(SDL_SCANCODE_W, true, id)));
                for (int frame = 0; frame < 240 && g.objects.get(p->m_hObject)->state.m_Pos.z < beyond; ++frame)
                {
                    if (frame % 60 == 0 || frame % 60 == 1)
                        CHECK(event(key(SDL_SCANCODE_E, frame % 60 == 0, id)));
                    CHECK(pump() && !p->m_damage.IsDead());
                }
                CHECK(event(key(SDL_SCANCODE_W, false, id)) && event(key(SDL_SCANCODE_E, false, id)) && pump());
                const auto end_position = g.objects.get(p->m_hObject)->state.m_Pos;
                std::cout << "Post-kill position=" << end_position.x << ',' << end_position.y << ',' << end_position.z
                          << " camera=" << bool(g.client_shell.view.object) << " yaw=" << g.client_shell.rotation.yaw
                          << '\n';
                CHECK(g.objects.get(p->m_hObject)->state.m_Pos.z >= beyond);
                body = g.objects.get(corpse);
                CHECK(body);
                CollisionHit overlap;
                CHECK(g.collision.sweep(body->state.m_Pos, body->state.m_Pos, body->dimensions, overlap) ==
                      CollisionQuery::Miss);
                std::cout << "Walked past killed Cultist to z=" << g.objects.get(p->m_hObject)->state.m_Pos.z << '\n';
                previous_target = target;
                previous_player = g.objects.get(p->m_hObject)->id;
                previous_corpse = body->id;
            }
            controls.player_changed();
            if (gpu)
                CHECK(controls.sync_mouse(runtime.window, false, false, error) &&
                      !SDL_GetWindowRelativeMouseMode(runtime.window));
            CHECK(!controls.sample_fire(false).primary && !controls.sample_fire(false).alternate);
            CHECK(g.leave_player() && g.unload_world() && g.objects.snapshot().empty() && !g.server.string_count() &&
                  !g.server.message_count() && !g.server.client_count());
        }
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
    if (mode == "entry")
        return entry() ? 0 : 1;
    if (argc == 3 && (mode == "combat" || mode == "gpu_combat"))
        return combat(argv[2], mode == "gpu_combat") ? 0 : 1;
    if (argc == 3 && (mode == "clearance" || mode == "gpu_clearance"))
        return combat(argv[2], mode == "gpu_clearance", true) ? 0 : 1;
    if (argc == 3 && (mode == "private" || mode == "gpu"))
        return private_input(argv[2], mode == "gpu") ? 0 : 1;
    return 2;
}
