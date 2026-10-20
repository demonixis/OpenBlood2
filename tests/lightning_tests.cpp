#include "app/sprite_scene.h"
#include "brush_fixture.h"
#include "game/original/ClientLightningSFX.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool formulas(const char* root)
    {
        GameSession g;
        std::string error;
        Config config;
        config.data_dir = root;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        std::minstd_rand rng(1);
        for (DBYTE shape = 0; shape <= 4; ++shape)
            for (DBYTE form = 0; form <= 4; ++form)
                for (DBYTE type = 0; type <= 2; ++type)
                {
                    CLightningBoltFX fx;
                    CHECK(fx.CreateObject({{0, 0, 0}, {0, 0, 950}, shape, form, type}, g.resources, 0, rng, error));
                    const unsigned count = type == 2 ? 9 : 5;
                    CHECK(fx.segments.size() == count && fx.lights.size() == 3);
                    CHECK(fx.segments.front().from.z == 0 && fx.segments.back().to.z == 950);
                    CHECK(fx.lights[1].settings.position.z == 475 && fx.lights[2].settings.position.z == 950);
                    CHECK(fx.lights[0].radius == 200);
                    CHECK(fx.segments.front().radius == (form == 2 || form == 3 ? 300 : 3000));
                    for (std::size_t i = 0; i < fx.segments.size(); ++i)
                    {
                        const auto& s = fx.segments[i];
                        if (i)
                            CHECK(vector_dot(vector_subtract(s.from, fx.segments[i - 1].to),
                                             vector_subtract(s.from, fx.segments[i - 1].to)) == 0);
                        if (!s.particles.empty())
                            CHECK(s.particles.front().position.z == s.from.z);
                        for (const auto& p : s.particles)
                            CHECK(physics_vector(p.position) && p.velocity.x == 0 && p.color.z == 1);
                    }
                    CHECK(fx.Update(.1) && fx.alpha == .75f);
                    CHECK(fx.Update(.2) && std::abs(fx.alpha - .25f) < .001f);
                    CHECK(std::abs(fx.lights[0].radius - 160) < .001f);
                    CHECK(fx.Update(.25) && fx.segments.empty() && fx.lights.size() == 3);
                    CHECK(!fx.Update(.5) && fx.error.empty());
                }
        CLightningBoltFX empty;
        CHECK(empty.CreateObject({{}, {0, 0, .001f}, 2, 1, 2}, g.resources, 0, rng, error));
        for (const auto& segment : empty.segments)
            CHECK(segment.particles.empty() && segment.increment == 0);
        CLightningBoltFX ten;
        CHECK(ten.CreateObject({{}, {0, 0, 1000}, 2, 1, 2}, g.resources, 0, rng, error) &&
              ten.segments.front().radius == 6000);
        CHECK(!ten.Update(-1) && !ten.error.empty());
        CHECK(!empty.CreateObject({{}, {}, 2, 1, 2}, g.resources, 0, rng, error));
        Resources missing;
        CHECK(!empty.CreateObject({{}, {0, 0, 1000}, 2, 1, 2}, missing, 0, rng, error));
        CHECK(g.server.lightning.publish({{}, {0, 0, 1000}, 2, 1, 2}, "bolt", g.resources, 0, error) && error.empty());
        const auto id = g.server.lightning.active[0].id;
        SpriteView scene;
        CHECK(update_sprite_scene(g, scene, error) && scene.entries.size() == g.server.lightning.live_particles());
        for (const auto& entry : scene.entries)
            CHECK(entry.lightning_bolt == id && entry.error.empty());
        CHECK(g.server.lightning.advance(.5) && g.server.lightning.active.empty());
        CHECK(g.unload_world() && g.server.lightning.reports.empty());
        CHECK(g.server.lightning.publish({{}, {0, 0, 100}, 2, 1, 2}, "reload", g.resources, 0, error) && error.empty());
        CHECK(g.server.lightning.active.front().id > id);
        return true;
    }
    bool emitter(const char* root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        auto wall = test_brush::box("wall", {-100, -100, 100}, {100, 100, 110});
        for (auto& surface : wall.surfaces)
            surface.flags = 1;
        CHECK(g.collision.build(wall, error));
        g.server.collision_ready = true;
        auto sender = g.objects.create(g.objects.find_class("BaseClass"), {});
        auto target = g.objects.create(g.objects.find_class("CBaseCharacter"), {});
        auto* victim = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(target));
        CHECK(victim && victim->m_damage.Init(target));
        victim->m_damage.SetHitPoints(1000);
        DVector at{0, 0, 98}, dims{1, 1, 1};
        CHECK(g.server.TeleportObject(target, &at) == DE_OK);
        g.server.SetObjectDims(target, &dims);
        auto h = g.objects.create(
            g.objects.find_class("CClientLightningSFX"), {}, Creation::World,
            {{"Name", std::string("lightning")}, {"Sound", std::string("Sounds\\Weapons\\c4\\explosion_1.wav")}});
        CHECK(h);
        auto* light = dynamic_cast<CClientLightningSFX*>(g.server.HandleToObject(h));
        // Cross-owner damage is accepted only during the original Fire radius-delivery scope.
        auto forged = g.server.StartMessageToObject(light, target, damage_message);
        CHECK(forged && g.server.WriteDamage(forged, {{0, 0, 1}, {}, 1, DamageType::Normal, sender, true}) != DE_OK);
        g.server.EndHMessageWrite(forged);
        std::cout << "Authored thunder resource present: " << bool(g.resources.find("Sounds\\Thunder.wav")) << '\n';
        auto trigger = [&](const char* text)
        {
            auto str = g.server.CreateString(text);
            auto message = g.server.StartMessageToObject(g.server.HandleToObject(sender), h, 1000);
            g.server.WriteToMessageHString(message, str);
            g.server.EndMessage(message);
            g.server.FreeString(str);
        };
        trigger("fire");
        CHECK(light->fires == 0);
        trigger("FIREsuffix");
        if (!light->error.empty() || !light->sound_error.empty())
            std::cerr << light->error << ' ' << light->sound_error << '\n';
        CHECK(light->fires == 1 && light->event && light->error.empty() && light->voice && light->sound_error.empty());
        CHECK(std::abs(light->bolt.destination.z - 100) < .001f && light->targets.delivered == 1);
        CHECK(victim->m_damage.last_damage.attacker == sender && victim->m_damage.last_damage.amount == 500);
        CHECK(victim->m_damage.m_fHitPoints == 500);
        light->Fire(nullptr); // A trigger without an owner still delivers original unowned area damage.
        CHECK(light->error.empty() && light->targets.delivered == 1 && victim->m_damage.IsDead() &&
              !victim->m_damage.last_damage.attacker);
        CHECK(!light->AllowsDamageAttribution(nullptr));
        light->sound = "Sounds\\Thunder.wav";
        light->Fire(nullptr);
        CHECK(!light->sound_error.empty());
        WorldValidation report;
        CHECK(!check_world_initialization(g, report, error) &&
              error.starts_with("Lightning")); // No global missing-sound waiver.
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && g.server.lightning.active.size() == 3);
        CHECK(g.server.lightning.advance(.3) && g.server.lightning.live_particles() == 0);
        CHECK(g.server.lightning.active[0].lights.size() == 3);
        CHECK(g.server.lightning.advance(.5) && g.server.lightning.active.empty());
        CHECK(g.unload_world() && !g.server.message_count() && !g.server.string_count());
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        unsigned total = 0, absent = 0;
        for (const auto* world : {"20_powerstation", "21_laboratory", "24b_arena", "25_ancientone"})
        {
            CHECK(g.request_world(world, error));
            for (auto id : g.objects.snapshot())
                if (const auto* r = g.objects.get(id))
                    if (auto* emitter = dynamic_cast<CClientLightningSFX*>(r->game.get()))
                    {
                        emitter->Fire(nullptr);
                        ++total;
                        CHECK(emitter->event && emitter->error.empty());
                        if (!emitter->sound_error.empty())
                        {
                            CHECK(emitter->sound == "Sounds\\Thunder.wav" && !g.resources.find(emitter->sound));
                            ++absent;
                        }
                        else
                            CHECK(emitter->voice);
                        std::cout << world << " " << r->state.m_Name << " effect=" << emitter->event
                                  << " sound=" << emitter->sound << " error=" << emitter->sound_error << '\n';
                    }
            CHECK(g.server.lightning.started > 0 && !g.server.lightning.failed);
            WorldValidation report;
            if (!check_world_initialization(g, report, error))
                CHECK(error.starts_with("World initialization rejected:"));
            for (const auto& row : report.entries)
                if (row.class_name == "CClientLightningSFX" &&
                    row.detail.find("missing exact authored sound") != std::string::npos)
                    CHECK(row.state == EntryState::Created);
            if (std::string_view(world) == "20_powerstation")
            {
                // A runtime lookalike with the same world/name/path is not the original authored row.
                CClientLightningSFX* original = nullptr;
                for (auto id : g.objects.snapshot())
                    if (auto* r = g.objects.get(id))
                        if (auto* light = dynamic_cast<CClientLightningSFX*>(r->game.get()))
                            original = light;
                CHECK(original);
                DRotation rotation;
                CHECK(g.server.GetObjectRotation(original->m_hObject, &rotation) == DE_OK);
                auto fake = g.objects.create(
                    g.objects.find_class("CClientLightningSFX"), {}, Creation::String,
                    {{"Name", std::string("KABOOM")}, {"Pos", original->bolt.source}, {"Rotation", rotation}});
                CHECK(fake);
                auto* lookalike = dynamic_cast<CClientLightningSFX*>(g.server.HandleToObject(fake));
                lookalike->Fire(nullptr);
                CHECK(lookalike->error.empty() && !lookalike->sound_error.empty());
                CHECK(!check_world_initialization(g, report, error) && error.starts_with("Lightning KABOOM"));
                CHECK(g.objects.remove(fake) && g.objects.finish_frame());
            }
            CHECK(g.unload_world() && g.server.lightning.active.empty());
        }
        CHECK(total == 10 && absent == 6);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    return (std::string_view(argv[1]) == "formulas"  ? formulas(argv[2])
            : std::string_view(argv[1]) == "catalog" ? catalog(argv[2])
                                                     : emitter(argv[2]))
               ? 0
               : 1;
}
