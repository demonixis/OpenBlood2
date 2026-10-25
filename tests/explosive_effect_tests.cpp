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
        GameSession g;
        CHECK(setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(owner);
        for (unsigned type : {5u, 9u, 10u, 11u})
        {
            std::string error;
            CHECK(g.server.PublishExplosionModels(owner, type, {0, 500, 0}, {0, 2, 0}, error) && error.empty());
            CHECK(g.server.PublishExplosionSprites(owner, type, {0, 500, 0}, {0, 2, 0}, error) && error.empty());
            CHECK(g.server.PublishExplosionRings(owner, type, {0, 500, 0}, {0, 2, 0}, error) && error.empty());
            CHECK(g.server.PublishExplosionFragments(owner, type, {0, 500, 0}, {0, 2, 0}, error) && error.empty());
            CHECK(g.server.PublishExplosionFlames(owner, type, {0, 500, 0}, {0, 2, 0}, error) && error.empty());
            CHECK(g.server.PublishExplosionLights(owner, type, {0, 500, 0}, {0, 2, 0}, error) && error.empty());
            CHECK(g.server.explosion_models.active.size() == 1 && g.server.explosion_flames.active.empty());
            CHECK(g.server.explosion_sprites.active.size() == (type == 11 ? 1 : 2));
            CHECK(g.server.explosion_lights.active.size() == 2);
            const auto& model = g.server.explosion_models.active.front().settings;
            CHECK(model.position.y == (type == 11 ? 515 : 510));
            CHECK(model.scale1.x == (type == 5 ? 25 : type == 11 ? 10 : 50));
            CHECK(model.scale2.x == (type == 5 ? 250 : type == 11 ? 150 : 400));
            const auto& light = g.server.explosion_lights.active.back().settings;
            CHECK(light.radius2 == 0 && light.duration == 25); // Original duplicate Radius1 write.
            if (type == 9)
            {
                CHECK(g.server.explosion_rings.active.size() == 2);
                CHECK(g.server.explosion_rings.active[0].settings.radius == 4000 &&
                      g.server.explosion_rings.active[1].settings.radius == 5000);
                CHECK(g.server.explosion_rings.advance(.1) && g.server.explosion_rings.active[0].particles.empty());
                CHECK(g.server.explosion_rings.advance(.2) &&
                      g.server.explosion_rings.active[0].particles.size() == 32 &&
                      g.server.explosion_rings.active[1].particles.empty());
                CHECK(g.server.explosion_rings.advance(.31) &&
                      g.server.explosion_rings.active[1].particles.size() == 32);
                const auto& fragments = g.server.explosion_fragments.active;
                CHECK(fragments.size() >= 5 && fragments.size() <= 10 &&
                      g.server.explosion_fragments.trails.size() == fragments.size());
                for (const auto& f : fragments)
                    CHECK(f.settings.scale.x == .75f && f.settings.scale.z == 1 && f.settings.spread >= 40 &&
                          f.settings.spread <= 80 && f.settings.velocity >= 20 && f.settings.velocity <= 40 &&
                          f.trail_error.empty());
            }
            else
            {
                CHECK(g.server.explosion_fragments.active.empty());
                CHECK(g.server.explosion_rings.active.size() == (type == 11 ? 1 : 0));
                if (type == 11)
                    CHECK(g.server.explosion_rings.active.front().settings.count == 12 &&
                          g.server.explosion_rings.active.front().settings.radius == 8000);
            }
            g.server.explosion_models.clear();
            g.server.explosion_sprites.clear();
            g.server.explosion_rings.clear();
            g.server.explosion_fragments.clear();
            g.server.explosion_lights.clear();
        }
        return g.unload_world();
    }
    bool smoke(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        CObjectParticleFX fx;
        std::string error;
        std::minstd_rand rng(1);
        NodePose pose;
        CHECK(fx.Init(2, 1, rng, error) && fx.CreateObject(g.resources, pose, 0, error));
        CHECK(fx.Update(0, &pose, rng));
        pose.position.x = 42.01f;
        CHECK(fx.Update(.051, &pose, rng) && fx.particles.size() == 80); // 10 stream samples, 8 per comet shell.
        CHECK(fx.gravity == 25 && fx.radius == 1200 && fx.requested_alpha == .25f && fx.alpha == 1);
        const auto& first = fx.particles.front();
        const float radial = first.position.x - pose.position.x; // Cached identity right axis.
        CHECK(radial >= 0 && radial <= 12.5f && vector_dot(first.velocity, first.velocity) == 0);
        CHECK(std::abs(first.lifetime - (.35f - .2f * radial / 12.5f)) < .00001f);
        CHECK(fx.Update(.1, &pose, rng) && !fx.moving && std::abs(fx.duration - .45f) < .0001f);
        pose.position.x += 100;
        CHECK(fx.Update(.2, &pose, rng) && fx.emitted == 80 && fx.alpha == 1);
        CHECK(!fx.Update(.5, &pose, rng) && fx.error.empty());
        CHECK(fx.Init(2, 1, rng, error) && fx.CreateObject(g.resources, {}, 0, error) && fx.Update(0, &pose, rng));
        auto before = rng;
        CHECK(!fx.Update(.051, &pose, rng, 1) && !fx.error.empty() && rng == before);
        return g.unload_world();
    }
    bool callers(const char* root)
    {
        for (auto name :
             {"CNapalmAltProjectile", "CHowitzerShell", "CHowitzerAltShell", "CHowitzerAltFrag", "CGrenade"})
        {
            GameSession g;
            CHECK(setup(g, root));
            auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
            CHECK(owner);
            ObjectCreateStruct state;
            state.m_Pos = {0, 500, 0};
            auto* p = dynamic_cast<CProjectile*>(g.server.CreateObject(g.server.GetClass(name), &state));
            CHECK(p && p->Setup({0, 0, 1}, 6, 120, 100, 200, owner));
            if (auto* n = dynamic_cast<CNapalmProjectile*>(p))
                CHECK(n->smoke && n->smoke_error.empty());
            if (auto* f = dynamic_cast<CHowitzerAltFrag*>(p))
                CHECK(f->trail && f->trail_error.empty());
            if (auto* grenade = dynamic_cast<CGrenade*>(p))
                CHECK(grenade->trail && grenade->trail_error.empty());
            p->Explode();
            CHECK(p->error.empty() && !g.server.scene_explosions.empty());
            const auto& e = g.server.scene_explosions.back();
            CHECK(e.model_error.empty() && e.sprite_error.empty() && e.ring_error.empty() && e.fragment_error.empty() &&
                  e.flame_error.empty() && e.light_error.empty());
            unsigned children = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* child = dynamic_cast<CProjectile*>(g.objects.get(id)->game.get());
                if (!child || child == p)
                    continue;
                if (e.type == 5)
                    CHECK(dynamic_cast<CNapalmFireball*>(child) && child->m_fDamage == 40 && child->m_nRadius == 200);
                else if (e.type == 10)
                    CHECK(dynamic_cast<CHowitzerAltFrag*>(child) && child->m_fDamage == 30 && child->m_nRadius == 100 &&
                          child->trail_error.empty());
                ++children;
            }
            CHECK(e.type == 5 ? children == 8 : e.type == 10 ? children >= 5 && children <= 10 : children == 0);
            CHECK(g.unload_world() && g.server.object_particles.active.empty() &&
                  g.server.explosion_fragments.active.empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    return (mode == "recipes" ? recipes(argv[2])
            : mode == "smoke" ? smoke(argv[2])
                              : mode == "callers" && callers(argv[2]))
               ? 0
               : 1;
}
