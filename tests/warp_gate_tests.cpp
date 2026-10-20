#include "app/sprite_scene.h"
#include "game/original/ClientWarpGateSFX.h"
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
    bool close(float a, float b)
    {
        return std::abs(a - b) < .0001f;
    }
    void trigger(GameSession& g, HOBJECT h, const char* text)
    {
        auto string = g.server.CreateString(text);
        auto message = g.server.StartMessageToObject(nullptr, h, 1000);
        g.server.WriteToMessageHString(message, string);
        g.server.EndMessage(message);
        g.server.FreeString(string);
    }
    bool formulas(const char* root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        WarpGateSettings settings;
        settings.sprite.resource = "Sprites\\SingleMus.spr";
        settings.ramp_up = 1;
        settings.ramp_down = 3;
        settings.sprite.ramp_up = 3;
        settings.sprite.ramp_down = 3;
        CHECK(valid_warp_gate(settings, error));
        NodePose pose;
        CWarpGateSpriteFX sprite;
        CHECK(sprite.CreateObject(settings, g.sprites, pose, 0));
        CHECK(sprite.Update(0, pose, false) && sprite.scale == 0);
        CHECK(sprite.Update(1, pose, true) && close(sprite.scale, .1f) && sprite.alpha == 0);
        CHECK(sprite.Update(1.5, pose, true) && close(sprite.scale, .55f) && close(sprite.alpha, .375f));
        CHECK(sprite.Update(2.1, pose, true) && sprite.scale == 1 && sprite.alpha == .75f);
        CHECK(sprite.Update(3, pose, false) && sprite.scale == 1 && sprite.alpha == .75f);
        CHECK(sprite.Update(5, pose, false) && close(sprite.scale, -.8f) && sprite.alpha == 0);
        CHECK(sprite.Update(6.1, pose, false) && !sprite.started && close(sprite.scale, -.8f));
        CHECK(sprite.Update(6.2, pose, false) && sprite.scale == 0);
        settings.ramp_up = settings.ramp_down = 0;
        CHECK(sprite.CreateObject(settings, g.sprites, pose, 0) && sprite.Update(0, pose, true) && sprite.scale == 1);
        CHECK(sprite.Update(.1, pose, false) && std::isfinite(sprite.alpha));
        CHECK(!sprite.Update(-1, pose, false) && !sprite.error.empty());
        WarpParticleSettings ps;
        ps.resource = "Sprites\\flaregunflare.spr";
        ps.count = 3;
        ps.ramp_up = ps.ramp_down = 1;
        ps.delay = .25f;
        ps.gravity = 0;
        ps.min_life = ps.max_life = 10;
        ps.min_color = {50, 20, 30};
        ps.max_color = {150, 120, 80};
        ps.offset = {1, 2, 3};
        std::minstd_rand rng(1);
        CWarpGateParticleFX p;
        CHECK(p.CreateObject(ps, 1, 2, g.sprites, g.resources, pose, 0) && p.sprite && p.alpha == 1);
        CHECK(p.Update(0, pose, true, rng, 100) && p.alpha == 0 && p.particles.empty());
        CHECK(p.Update(.25, pose, true, rng, 100) && p.emitted == 3 && close(p.alpha, .125f));
        CHECK(p.pose.position.x == 1 && p.pose.position.y == 2 && p.pose.position.z == 3);
        for (const auto& part : p.particles)
            CHECK(close(part.color.y, part.color.x) && close(part.color.z, part.color.x * .5f));
        CHECK(p.Update(.75, pose, true, rng, 100) && p.emitted == 6 && close(p.alpha, .375f));
        CHECK(p.Update(1.1, pose, true, rng, 100) && p.emitted == 9 && close(p.alpha, .375f)); // No snap.
        CHECK(p.Update(2, pose, false, rng, 100) && p.alpha == .5f && p.emitted == 9 && close(float(p.last_add), 1.1f));
        CHECK(p.Update(3, pose, false, rng, 100) && p.alpha == 0 && p.emitted == 9);
        CHECK(p.Update(4.1, pose, false, rng, 100) && !p.started && p.emitted == 12); // Ending update emits.
        CHECK(p.Update(5, pose, true, rng, 100) && p.emitted == 12);
        CHECK(p.Update(7, pose, true, rng, 100) && p.emitted == 12); // Prior last_add survives reset.
        CHECK(p.Update(7.5, pose, true, rng, 100) && p.emitted == 15);
        CHECK(!p.Update(8, pose, true, rng, 15) && !p.error.empty());
        ps.count = 0;
        ps.resource = "SpriteTextures\\";
        CHECK(p.CreateObject(ps, 1, 1, g.sprites, g.resources, pose, 0));
        CHECK(p.Update(0, pose, true, rng, 0) && p.Update(1, pose, true, rng, 0) && p.particles.empty());
        ps.count = 1;
        CHECK(!p.CreateObject(ps, 1, 1, g.sprites, g.resources, pose, 0) && !p.error.empty());
        settings.particles[0].count = 513;
        CHECK(!valid_warp_gate(settings, error));
        return true;
    }
    bool controller(const char* root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        auto cls = g.objects.find_class("CClientWarpGateSFX");
        CHECK(cls);
        auto h = g.objects.create(cls, {}, Creation::World, {{"SpriteFile", std::string("Sprites\\SingleMus.spr")}});
        CHECK(h);
        auto* controller = dynamic_cast<CClientWarpGateSFX*>(g.server.HandleToObject(h));
        CHECK(controller && !controller->on);
        CHECK(g.server.warp_gates.active.empty());
        CHECK(g.objects.advance(.5) && g.server.warp_gates.active.empty());
        trigger(g, h, "on");
        CHECK(!controller->on);
        trigger(g, h, "ONsuffix");
        CHECK(controller->on && !(g.server.GetObjectUserFlags(h) & 1u));
        CHECK(g.objects.advance(.51) && controller->event && controller->error.empty());
        CHECK(g.server.GetObjectUserFlags(h) & 1u);
        CHECK(g.server.warp_gates.advance(1.01, g.server));
        const auto id = controller->event;
        CHECK(g.server.warp_gates.advance(1.51, g.server) && g.server.warp_gates.particle_count() == 20);
        SpriteView scene;
        CHECK(update_sprite_scene(g, scene, error));
        CHECK(scene.entries.size() == 21);
        for (const auto& entry : scene.entries)
            CHECK(entry.warp_gate == id && entry.error.empty());
        trigger(g, h, "OFFsuffix");
        CHECK(!controller->on && (g.server.GetObjectUserFlags(h) & 1u));
        CHECK(g.objects.advance(.51) && !(g.server.GetObjectUserFlags(h) & 1u));
        trigger(g, h, "TOGGLEsuffix");
        CHECK(controller->on);
        CHECK(!g.server.string_count() && !g.server.message_count());
        // Two live owners exercise erase-before-budget counting, including an earlier dead slot.
        auto second =
            g.objects.create(cls, {}, Creation::World, {{"SpriteFile", std::string("Sprites\\SingleMus.spr")}});
        CHECK(second && g.objects.advance(1.1));
        CHECK(g.server.warp_gates.active.size() == 2);
        CHECK(g.objects.remove(h) && g.objects.finish_frame() &&
              g.server.warp_gates.advance(g.objects.time(), g.server));
        CHECK(g.server.warp_gates.active.size() == 1 && g.server.warp_gates.reports[0].ended);
        CHECK(g.unload_world() && g.server.warp_gates.active.empty() && g.server.warp_gates.reports.empty());
        auto bad =
            g.objects.create(cls, {}, Creation::World, {{"SpriteFile", std::string("Sprites\\absent-warp-test.spr")}});
        CHECK(bad && g.objects.advance(1.1));
        auto* missing = dynamic_cast<CClientWarpGateSFX*>(g.server.HandleToObject(bad));
        CHECK(missing && missing->event > id && !missing->error.empty());
        CHECK(g.server.warp_gates.active.size() == 1 && !g.server.warp_gates.active[0].sprite_ready &&
              g.server.warp_gates.active[0].particle_ready[0]);
        WorldValidation validation;
        CHECK(!check_world_initialization(g, validation, error) && error.starts_with("Warp"));
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        unsigned total = 0, animated = 0, unused = 0;
        for (const auto* world : {"09_abandoned_buildings", "24b_arena"})
        {
            CHECK(g.request_world(world, error));
            std::vector<HOBJECT> owners;
            for (auto id : g.objects.snapshot())
                if (const auto* r = g.objects.get(id))
                    if (auto* c = dynamic_cast<CClientWarpGateSFX*>(r->game.get()))
                    {
                        owners.push_back(c->m_hObject);
                        trigger(g, c->m_hObject, "ON");
                        ++total;
                    }
            CHECK(owners.size() == (std::string_view(world) == "24b_arena" ? 6 : 1));
            CHECK(g.objects.advance(1.01));
            CHECK(g.server.warp_gates.active.size() == owners.size());
            for (const auto& gate : g.server.warp_gates.active)
            {
                CHECK(gate.sprite_ready && gate.particle_ready[0] && gate.particle_ready[1]);
                for (const auto& p : gate.particles)
                {
                    animated += bool(p.sprite);
                    unused += p.settings.count == 0;
                }
            }
            const double start = g.objects.time();
            for (unsigned step = 0; step < 90; ++step)
            {
                const bool updated = g.server.warp_gates.advance(start + step / 60.0, g.server);
                error = g.server.warp_gates.update_error;
                CHECK(updated);
            }
            SpriteView scene;
            CHECK(update_sprite_scene(g, scene, error));
            std::size_t displayed = 0;
            for (const auto& e : scene.entries)
                if (e.warp_gate)
                {
                    ++displayed;
                    CHECK(e.error.empty());
                }
            CHECK(displayed > owners.size() && !g.server.warp_gates.failed);
            std::cout << world << " gates=" << owners.size() << " particles=" << g.server.warp_gates.particle_count()
                      << " prepared=" << displayed << '\n';
            CHECK(g.unload_world() && g.server.warp_gates.active.empty());
        }
        CHECK(total == 7 && animated == 3 && unused == 1);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    return (std::string_view(argv[1]) == "formulas"  ? formulas(argv[2])
            : std::string_view(argv[1]) == "catalog" ? catalog(argv[2])
                                                     : controller(argv[2]))
               ? 0
               : 1;
}
