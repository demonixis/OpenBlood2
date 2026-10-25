#include "brush_fixture.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/Grenade.h"
#include "game/server_adapter.h"
#include <cmath>
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
    bool setup(GameSession& g, const char* root)
    {
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        CHECK(g.collision.build(test_brush::box("floor", {-10000, -20, -10000}, {10000, 0, 10000}), error));
        g.server.collision_ready = true;
        return true;
    }
    bool recipes(const char* root)
    {
        struct Row
        {
            unsigned type, models, sprites, rings, flames, lights;
        };
        for (auto row : {Row{12, 0, 0, 0, 1, 2},
                         {18, 2, 0, 1, 0, 1},
                         {19, 1, 0, 1, 0, 1},
                         {23, 2, 0, 1, 0, 1},
                         {24, 1, 0, 1, 0, 1},
                         {25, 1, 0, 0, 0, 1},
                         {28, 1, 0, 0, 1, 1},
                         {29, 0, 1, 1, 0, 1},
                         {30, 0, 1, 0, 0, 0},
                         {31, 0, 2, 1, 0, 0}})
        {
            GameSession g;
            CHECK(setup(g, root));
            auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
            CHECK(owner);
            const auto report = PublishCampaignExplosion(owner, {0, 500, 0}, {0, 1, 0}, row.type);
            if (!report.model_error.empty() || !report.sprite_error.empty() || !report.ring_error.empty() ||
                !report.flame_error.empty() || !report.fragment_error.empty() || !report.light_error.empty())
                std::cerr << row.type << ": " << report.model_error << report.sprite_error << report.ring_error
                          << report.flame_error << report.fragment_error << report.light_error << '\n';
            CHECK(report.model_error.empty() && report.sprite_error.empty() && report.ring_error.empty() &&
                  report.flame_error.empty() && report.fragment_error.empty() && report.light_error.empty());
            CHECK(g.server.explosion_models.active.size() == row.models);
            CHECK(g.server.explosion_sprites.active.size() == row.sprites);
            CHECK(g.server.explosion_rings.active.size() == row.rings);
            CHECK(g.server.explosion_flames.active.size() == row.flames);
            CHECK(g.server.explosion_lights.active.size() == row.lights);
            if (row.type == 18 || row.type == 23)
                CHECK(g.server.explosion_models.active[1].settings.rotations.z == -.05f);
            if (row.type == 29 || row.type == 30)
            {
                CHECK(g.server.explosion_fragments.active.size() >= (row.type == 29 ? 5 : 3));
                CHECK(g.server.explosion_fragments.active.size() <= (row.type == 29 ? 8 : 5));
                CHECK(g.server.explosion_fragments.trails.empty());
                for (const auto& f : g.server.explosion_fragments.active)
                    CHECK(f.settings.duration == 5 && f.settings.rotate_max.x == .5f && f.trail_error.empty());
            }
            CHECK(g.server.explosion_models.advance(.3) && g.server.explosion_rings.advance(.3) &&
                  g.server.explosion_flames.advance(.3));
            CHECK(g.unload_world());
        }
        return true;
    }
    bool particles(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (unsigned recipe : {4u, 9u, 27u, 31u, 37u, 38u, 39u})
        {
            CObjectParticleFX fx;
            std::string error;
            std::minstd_rand random(7);
            NodePose pose;
            CHECK(fx.Init(recipe, 1, random, error) && fx.CreateObject(g.resources, pose, 10, error));
            CHECK(fx.Update(10, &pose, random) && fx.particles.empty());
            pose.position.x = 100;
            CHECK(fx.Update(10.101, &pose, random) && !fx.particles.empty());
            CHECK(fx.alpha == 1); // Source authored alpha remains unused before its ramp.
            if (recipe == 9)
                for (const auto& p : fx.particles)
                    CHECK(vector_dot(p.position, p.velocity) < 0);
            if (recipe == 37 || recipe == 38)
            {
                CHECK(fx.particles.size() == (recipe == 37 ? 24 : 32));
                for (const auto& p : fx.particles)
                    CHECK(p.velocity.z == 450 && std::abs(p.position.z) < .001f);
            }
            CHECK(!fx.Update(10.2, nullptr, random) && fx.error.empty());
        }
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(owner);
        for (auto recipe : {4u, 27u, 31u})
        {
            std::string error;
            CHECK(g.server.PublishObjectParticles(owner, recipe, error, recipe == 31 ? .25f : .5f) && error.empty());
            CHECK(g.server.object_particles.active.back().effect.max_life < .4f);
        }
        return g.unload_world();
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    return (std::string(argv[1]) == "recipes" ? recipes(argv[2]) : particles(argv[2])) ? 0 : 1;
}
