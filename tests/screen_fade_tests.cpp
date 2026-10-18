#include "app/sprite_scene.h"
#include "camera_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/ExitTrigger.h"
#include "player_fixture.h"
#include <algorithm>
#include <cmath>
#include <limits>
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
    bool values()
    {
        CScreenFade fade;
        CHECK(fade.Scale() == 1 && fade.Update(0));
        fade.Begin(false);
        for (int i = 0; i < 25; ++i)
            CHECK(fade.Update(.1));
        CHECK(fade.value == 5 && fade.fade_out);
        CHECK(fade.Update(.05) && fade.value == 0 && fade.fade_out);
        CHECK(fade.Update(.1) && fade.Scale() == 0 && fade.fade_out);
        fade.Begin(true);
        for (int i = 0; i < 25; ++i)
            CHECK(fade.Update(.1));
        CHECK(fade.Update(.05) && fade.value == 255 && fade.fade_in); // Strict >, not >=.
        CHECK(fade.Update(0) && fade.fade_in);
        CHECK(fade.Update(.01) && !fade.fade_in && fade.Scale() == 1);
        fade.Begin(false);
        CHECK(fade.Update(.1) && fade.value == 245);
        fade.Begin(false);
        CHECK(fade.value == 255); // Retrigger starts again, never reverses current value.
        fade.Begin(true);
        CHECK(fade.value == 0);
        CHECK(!fade.Update(-1) && !fade.Update(60.01) && !fade.Update(std::numeric_limits<double>::quiet_NaN()));
        CHECK(fade.value == 0 && fade.fade_in && fade.messages == 5);
        return true;
    }
    bool send(GameSession& g, HOBJECT sender, const char* command)
    {
        auto text = g.server.CreateString(command);
        auto msg = g.server.StartMessageToObject(g.server.HandleToObject(sender), g.client_shell.player, 1000);
        const bool ok = msg && g.server.WriteToMessageHString(msg, text) == DE_OK && g.server.EndMessage(msg) == DE_OK;
        g.server.FreeString(text);
        return ok;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        const auto strings = g.server.string_count();
        CHECK(send(g, p->m_hObject, "  FaDeOuT ignored suffix") && p->fade_messages == 1);
        CHECK(g.client_shell.fade.messages == 0 && g.pump_player_messages(error));
        CHECK(g.client_shell.fade.fade_out && g.step(.1) && g.client_shell.fade.value == 245);
        CHECK(send(g, p->m_hObject, "fadeout") && g.pump_player_messages(error) && g.client_shell.fade.value == 255);
        CHECK(send(g, p->m_hObject, "fadein") && g.pump_player_messages(error) && g.client_shell.fade.value == 0);
        CHECK(g.step(.1) && g.client_shell.fade.value == 10);
        CHECK(send(g, p->m_hObject, "fadeout\t") && p->ignored_triggers == 1 && p->player_trigger_error.empty());
        CHECK(send(g, p->m_hObject, "Music LEVEL 3") && p->unsupported_triggers == 0 && p->music_messages == 1);
        CHECK(g.pump_player_messages(error) && g.client_shell.music.logical_level == 3 &&
              g.client_shell.music.failures == 2 && !g.client_shell.music.error.empty()); // No bank in fixture.
        CHECK(send(g, p->m_hObject, "   ") && p->unsupported_triggers == 0 && p->player_trigger_error.empty());
        CHECK(g.client_shell.fade.messages == 3 && g.server.string_count() == strings);
        p->m_bDead = true; // Source trigger dispatch is not under its pickup/dead guard.
        CHECK(send(g, p->m_hObject, "fadeout") && g.pump_player_messages(error) && g.client_shell.fade.messages == 4);
        p->m_bDead = false;
        CHECK(send(g, p->m_hObject, "fadein")); // Queued event must not survive client/world lifetime.
        const auto client = g.local_client;
        CHECK(g.leave_player() && g.client_shell.fade.messages == 0 && g.client_shell.fade.Scale() == 1);
        CHECK(g.server.SendToClient(client, ScreenFade{false}) == DE_INVALIDPARAMS);
        CHECK(g.enter_player(error) && g.client_shell.fade.Scale() == 1 && !g.client_shell.fade.messages);
        CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.client_count() && !g.server.string_count());
        return true;
    }
    bool private_exit(const std::filesystem::path& root)
    {
        std::string error;
        Config config;
        config.data_dir = root;
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize() && game->resources.mount_game(config, error));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            auto& g = *game;
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error) &&
                  test_camera::complete_subway_intro(g));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p);
            HOBJECT relay = nullptr, exit = nullptr;
            for (auto id : g.objects.snapshot())
            {
                const auto* r = g.objects.get(id);
                if (std::string_view(r->state.m_Name) == "Trigger2")
                    relay = r->handle;
                if (dynamic_cast<ExitTrigger*>(r->game.get()))
                    exit = r->handle;
            }
            CHECK(relay && exit);
            auto* t = dynamic_cast<Trigger*>(g.server.HandleToObject(relay));
            auto* x = dynamic_cast<ExitTrigger*>(g.server.HandleToObject(exit));
            CHECK(t && x && !t->activations && t->touch_activate && t->slots[0].delay == 7 && t->slots[9].delay == 12);
            // Diagnostic relocation to the final carriage, then normal MoveMgr/USE/contact.
            // No trigger rearming, message injection, NPC editing, health/ammo grants, or invulnerability.
            DVector position{-80, 170, 2000};
            CHECK(g.server.TeleportObject(p->m_hObject, &position) == DE_OK && p->TeleportClientToServerPos());
            CHECK(g.pump_player_messages(error));
            g.client_shell.rotation = {0, 0};
            g.client_shell.movement_input.forward = true;
            for (int tick = 0; tick < 300 && !t->activations; ++tick)
            {
                g.client_shell.use_down = tick % 60 == 0;
                CHECK(g.step(1. / 60) && !p->m_damage.IsDead());
            }
            std::cout << "Relay reached at " << g.objects.get(p->m_hObject)->state.m_Pos.z
                      << " activations=" << t->activations << '\n';
            CHECK(t->activations == 1 && t->last_sender == p->m_hObject && !x->attempts);
            g.client_shell.movement_input = {};
            g.client_shell.use_down = false;
            const auto began = t->delay_start;
            bool faded = false, black = false;
            std::size_t peak_trail = 0, peak_total = 0;
            std::uint64_t combat_hits = 0;
            bool large_scene_prepared = false;
            for (int tick = 0; tick < 1000 && !g.pending_level_change; ++tick)
            {
                // The relocation leaves the carriage uncleared. Fight visible authored
                // Cultists through normal aiming/FIRE_KEY, rather than granting immunity.
                HOBJECT target = nullptr;
                float closest = 1e20f, target_health = 0;
                DVector aim{};
                for (auto identity : g.objects.snapshot())
                {
                    const auto* record = g.objects.get(identity);
                    const auto* ai = record ? dynamic_cast<const AI_Mgr*>(record->game.get()) : nullptr;
                    if (!ai || ai->npc_type != AI_Mgr::NPCType::Cultist || ai->m_damage.IsDead())
                        continue;
                    DVector head;
                    DRotation rotation;
                    CHECK(g.server.GetModelNodeTransform(record->handle, "head", &head, &rotation));
                    SegmentHit hit;
                    const auto delta = vector_subtract(head, g.client_shell.view.position);
                    const auto distance = vector_dot(delta, delta);
                    if (distance < closest &&
                        g.server.IntersectSegment(g.client_shell.view.position, head, p->m_hObject, hit, false,
                                                  SegmentObjects::SolidOrRayHit, SegmentFilter::IgnoreVolumes,
                                                  SegmentStart::SkipInterior) == SegmentResult::Hit &&
                        hit.object == record->handle)
                    {
                        target = record->handle;
                        closest = distance;
                        aim = delta;
                        target_health = ai->m_damage.m_fHitPoints;
                    }
                }
                g.client_shell.weapon_input = {target != nullptr, false, false};
                if (target)
                    g.client_shell.rotation = {-std::atan2(aim.y, std::hypot(aim.x, aim.z)), std::atan2(aim.x, aim.z)};
                const bool stepped = g.step(1. / 60);
                if (target)
                {
                    const auto* record = g.objects.get(target);
                    const auto* ai = record ? dynamic_cast<const AI_Mgr*>(record->game.get()) : nullptr;
                    combat_hits += !ai || ai->m_damage.m_fHitPoints < target_health;
                }
                if (!stepped || p->m_damage.IsDead())
                {
                    std::cerr << "Exit update failure after " << g.server.GetTime() - began << "s, stepped=" << stepped
                              << " HP=" << p->m_damage.m_fHitPoints << "\n";
                    std::cerr << "FX errors: " << g.server.explosion_fragments.update_error << " | "
                              << g.server.explosion_flames.update_error << " | "
                              << g.server.object_particles.update_error << " | " << g.server.blood_spurts.update_error
                              << " | " << g.server.sparks.update_error << "\n";
                    for (const auto& line : g.objects.diagnostics())
                        std::cerr << "Objects: " << line << "\n";
                    for (const auto& line : g.trace)
                        std::cerr << line << "\n";
                    for (auto id : g.objects.snapshot())
                        if (auto* ai = dynamic_cast<AI_Mgr*>(g.objects.get(id)->game.get()); ai && ai->runtime_failed)
                            std::cerr << "AI " << g.objects.get(id)->state.m_Name << ": " << ai->runtime_error << "\n";
                }
                CHECK(stepped && !p->m_damage.IsDead());
                peak_total = std::max(peak_total, g.server.explosion_fragments.trail_particles());
                for (const auto& trail : g.server.explosion_fragments.trails)
                    peak_trail = std::max(peak_trail, trail.particles.size());
                if (!large_scene_prepared && g.server.explosion_fragments.trail_particles() > 8192)
                {
                    SpriteView large_scene;
                    CHECK(update_sprite_scene(g, large_scene, error));
                    std::size_t prepared = 0;
                    for (const auto& entry : large_scene.entries)
                        if (entry.trail_system)
                        {
                            CHECK(entry.error.empty());
                            ++prepared;
                        }
                    CHECK(prepared == g.server.explosion_fragments.trail_particles());
                    large_scene_prepared = true;
                }
                const auto elapsed = g.server.GetTime() - began;
                if (elapsed < 7)
                    CHECK(!g.client_shell.fade.messages);
                faded |= g.client_shell.fade.messages == 1 && g.client_shell.fade.Scale() < 1;
                black |= g.client_shell.fade.Scale() == 0;
                if (elapsed < 14)
                    CHECK(!g.pending_level_change);
            }
            std::cout << "Trail peaks: " << peak_trail << " per system, " << peak_total << " total\n";
            SpriteView scene;
            CHECK(update_sprite_scene(g, scene, error));
            CHECK(g.server.explosion_fragments.trail_failed == 0 && peak_trail > 2048 && peak_total <= 16384);
            CHECK(large_scene_prepared && combat_hits > 0 && g.client_shell.weapons.front().fire_keys > 0);
            g.client_shell.weapon_input = {};
            CHECK(faded && black && g.pending_level_change && x->deliveries == 1 && p->fade_messages == 1);
            CHECK(t->slots[0].sent == 1 && t->slots[9].sent == 1 && g.client_shell.fade.fade_out);
            for (const auto& slot : t->slots)
                CHECK(slot.sent == 1);
            for (const auto& row : service_coverage(g.server.service_calls))
            {
                CHECK(row.support != ServiceSupport::Unclassified &&
                      (row.support != ServiceSupport::Unavailable || row.calls == 0));
                std::cout << "EXIT_TRACE\t" << cycle << '\t' << row.name << '\t' << service_support_name(row.support)
                          << '\t' << row.calls << '\n';
            }
            std::cout << "Authored exit cycle " << cycle << ": fade at 7s, held black, original exit at "
                      << g.server.GetTime() - began << "s; unsupported player commands=" << p->unsupported_triggers
                      << '\n';
            CHECK(g.server.unavailable_history.empty());
            for (auto id : g.objects.snapshot())
                if (auto* ai = dynamic_cast<AI_Mgr*>(g.objects.get(id)->game.get()); ai && ai->runtime_failed)
                {
                    std::cerr << ai->runtime_error << '\n';
                    return false;
                }
            const auto health = p->m_damage.m_fHitPoints, ammo = p->m_InventoryMgr.GetAmmoCount(1);
            CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Committed);
            CHECK(game->world_path == "worlds/02_cityhub.dat" && game->restoration.restored == 1);
            CHECK(game->client_shell.fade.messages == 0 && game->client_shell.fade.Scale() == 1);
            p = game->server_shell.Player(game->local_client);
            CHECK(p && p->m_damage.m_fHitPoints == health && p->m_InventoryMgr.GetAmmoCount(1) == ammo);
            for (int tick = 0; tick < 60; ++tick)
                CHECK(game->step(1. / 60));
            CHECK(game->unload_world() && game->objects.snapshot().empty() && !game->server.string_count() &&
                  !game->server.message_count() && !game->server.client_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string_view mode = argv[1];
    return (mode == "values"                 ? values()
            : argc == 3 && mode == "session" ? session(argv[2])
            : argc == 3 && mode == "private" ? private_exit(argv[2])
                                             : false)
               ? 0
               : 1;
}
