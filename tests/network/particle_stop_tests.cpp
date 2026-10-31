#include "client/effect_queries.h"
#include "game/explosion_fragments.h"
#include "game/object_particles.h"
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                    \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        Config config;
        config.data_dir = path_from_utf8(data);
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        for (unsigned recipe : {1u, 2u, 3u, 4u, 6u, 8u, 27u, 31u, 32u, 33u, 34u})
        {
            CObjectParticleFX fx;
            std::minstd_rand rng(1);
            NodePose pose;
            CHECK(fx.Init(recipe, 1, rng, error) && fx.CreateObject(resources, pose, 100, error));
            CHECK(fx.Update(100, &pose, rng, 2048, true) && fx.updates == 1 && fx.particles.empty());
            pose.position.x = 1;
            CHECK(fx.Update(100.125, &pose, rng, 2048, true));
            CHECK(fx.Update(100.25, &pose, rng, 2048, true) && !fx.moving && !fx.duration && fx.stop_time == 100.25);
            pose.position.x = 2;
            CHECK(fx.Update(101.125, &pose, rng, 2048, true) && fx.moving && fx.stop_time < 0 && !fx.duration);
            CHECK(fx.Update(101.25, &pose, rng, 2048, true) && !fx.moving && fx.stop_time == 101.25);
            CHECK(fx.Update(102.25, &pose, rng, 2048, true) && !fx.duration); // Exact grace boundary does not expire.
            CHECK(fx.Update(102.375, &pose, rng, 2048, true));
            const auto duration = 2.375f + fx.ramp_down + fx.max_life;
            CHECK(fx.duration == duration && !fx.moving);
            pose.position.x = 3;
            CHECK(fx.Update(102.5, &pose, rng, 2048, true) && !fx.moving && fx.duration == duration);
            CHECK(fx.Update(100 + double(duration), &pose, rng, 2048, true));
            CHECK(!fx.Update(100 + double(duration) + .125, &pose, rng, 2048, true) && fx.error.empty());
            // Solo stays terminal on its very first stationary observation.
            CHECK(fx.Init(recipe, 1, rng, error) && fx.CreateObject(resources, pose, 100, error));
            CHECK(fx.Update(100, &pose, rng) && fx.Update(100.25, &pose, rng) && fx.duration > 0 && !fx.moving);
            const auto solo_duration = fx.duration;
            pose.position.x = 4;
            CHECK(fx.Update(100.375, &pose, rng) && !fx.moving && fx.duration == solo_duration && fx.stop_time < 0);
            CHECK(!fx.Update(100.5, nullptr, rng, 2048, true));
        }
        // Fixed-duration spells bypass the movement branch in either game mode.
        for (unsigned recipe : {9u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 21u, 36u, 37u, 38u, 39u})
        {
            CObjectParticleFX fx;
            std::minstd_rand rng(1);
            NodePose pose;
            CHECK(fx.Init(recipe, 1, rng, error) && fx.CreateObject(resources, pose, 0, error));
            const auto duration = fx.duration;
            CHECK(duration > 0);
            CHECK(fx.Update(0, &pose, rng, 2048, true) && fx.Update(.125, &pose, rng, 2048, true));
            CHECK(fx.duration == duration && fx.stop_time < 0);
            CHECK(!fx.Update(duration + .125, nullptr, rng, 2048, true));
        }
        ClientWorld world;
        CHECK(world.begin(7));
        ClientWorldFrame frame;
        frame.world = 7;
        frame.sequence = 1;
        ClientObject owner;
        owner.id = {0, 1};
        frame.objects.push_back(owner);
        CHECK(world.apply(std::move(frame), error));
        ObjectParticles owned;
        CHECK(owned.publish({world.reference(owner.id), {}, 0, "network owner"}, 1, &resources, 0, error));
        CHECK(owned.advance(0, world, true) && owned.advance(.25, world, true));
        CHECK(owned.active[0].effect.stop_time == .25 && !owned.active[0].effect.duration);
        // Fragment child trails use the same network-only branch. A resting fixture
        // requires no geometry query; unavailable queries would expose accidental movement.
        ModelStore models(resources);
        ExplosionFragments fragments;
        CHECK(fragments.publish(3, {}, {0, 1, 0}, "resting fragment fixture", models, resources, 0, error));
        CHECK(!fragments.active.empty() && !fragments.trails.empty());
        for (auto& fragment : fragments.active)
        {
            fragment.velocity = {};
            fragment.last_normal = {0, 1, 0};
        }
        ClientEffectQueries queries(nullptr);
        CHECK(fragments.advance(0, queries, true) && fragments.advance(.25, queries, true));
        for (const auto& trail : fragments.trails)
            CHECK(trail.stop_time == .25 && !trail.duration);
        CHECK(fragments.advance(1.25, queries, true));
        for (const auto& trail : fragments.trails)
            CHECK(!trail.duration);
        CHECK(fragments.advance(1.375, queries, true));
        for (const auto& trail : fragments.trails)
            CHECK(trail.duration > 0 && !trail.moving);
        std::cout << "Original multiplayer stop/resume: 11 trail recipes, strict 1-second grace/terminal tail, 14 "
                     "finite spells, unchanged solo, owner and fragment pool forwarding passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
