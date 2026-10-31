#include "client/network_audio.h"
#include "client/network_effects.h"
#include "client/network_world.h"
#include "net/object_particle_codec.h"
#include <iostream>
#include <random>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x ": " << error << '\n';                                 \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    constexpr unsigned recipes[]{1,  2,  3,  4,  6,  8,  9,  12, 13, 14, 15, 16, 17,
                                 18, 19, 21, 24, 27, 31, 32, 33, 36, 37, 38, 39};
    WorldSnapshot sample()
    {
        WorldSnapshot s;
        s.world = 7;
        s.sequence = 1;
        s.time = 10;
        ObjectState a;
        a.id = {0, 1};
        a.object_particles = {{2, 9, 1, 1, {}, "smoke"}, {4, 9, 19, 1, {}, "shield"}};
        s.objects.push_back(a);
        a.id = {1, 1};
        a.object_particles = {
            {1, 9.5, 12, 1, {}, "voodoo"}, {3, 9.5, 37, 1, {1, 2, 3}, "ringA"}, {5, 9.5, 38, 1, {1, 2, 3}, "ringB"}};
        s.objects.push_back(a);
        return s;
    }
    bool codec()
    {
        std::string error;
        std::minstd_rand rng(23);
        for (auto recipe : recipes)
        {
            CObjectParticleFX fx;
            CHECK(fx.Init(recipe, 1, rng, error));
            CHECK(fx.texture == object_particle_texture(recipe) && fx.duration == object_particle_duration(recipe));
            auto s = sample();
            s.objects.resize(1);
            s.objects[0].object_particles = {{UINT64_C(0x0807060504030201), 10, recipe, 1, {}, "source"}};
            std::vector<std::uint8_t> bytes, again;
            CHECK(encode_world_snapshot(s, bytes) && bytes[4] == 23);
            WorldSnapshot out;
            out.sequence = 999;
            for (std::size_t n = 0; n < bytes.size(); ++n)
                CHECK(!decode_world_snapshot(std::span(bytes).first(n), out) && out.sequence == 999);
            CHECK(decode_world_snapshot(bytes, out) && encode_world_snapshot(out, again) && again == bytes);
            for (unsigned i = 0; i < 500; ++i)
            {
                auto m = bytes;
                m[rng() % m.size()] = std::uint8_t(rng());
                if (decode_world_snapshot(m, out))
                    CHECK(encode_world_snapshot(out, again) && again == m);
            }
        }
        auto s = sample();
        std::vector<std::uint8_t> bytes;
        CHECK(encode_world_snapshot(s, bytes));
        auto invalid = s;
        invalid.objects[1].object_particles[0].id = 2;
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles[0].created = 10.1;
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles[0].type = 34;
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles[0].lifetime_scale = .5;
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles[0].offset = {1, 0, 0};
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles[0].source = std::string(101, 'x');
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles[0].source = "bad\nname";
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles[0].offset.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles[0].id = 0;
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].object_particles.clear();
        invalid.objects[1].object_particles.clear();
        for (unsigned i = 0; i < 129; ++i)
            invalid.objects[i % 2].object_particles.push_back({i + 1, 0, 24, 1, {}, "bounded"});
        CHECK(!valid_snapshot(invalid));
        invalid.objects[0].object_particles.pop_back();
        CHECK(valid_snapshot(invalid));
        auto p = ObjectParticleRecipe{1, 0, 3, .25f, {}, "scaled"};
        CHECK(valid_object_particle_recipe(p));
        p.lifetime_scale = 2;
        CHECK(valid_object_particle_recipe(p));
        p.lifetime_scale = 2.01f;
        CHECK(!valid_object_particle_recipe(p));
        p = {1, 0, 37, 1, {1e7f, -1e7f, 0}, "offset"};
        CHECK(valid_object_particle_recipe(p));
        p.offset.x = 1.1e7f;
        CHECK(!valid_object_particle_recipe(p));
        Writer w(8192);
        write_object_particles(w, {{UINT64_C(0x0807060504030201), 0, 24, 1, {}, ""}});
        CHECK(w.good() && std::equal(w.bytes().begin(), w.bytes().begin() + 9,
                                     std::vector<std::uint8_t>{1, 1, 2, 3, 4, 5, 6, 7, 8}.begin()));
        auto bad = w.bytes();
        bad[0] = 129;
        Reader r(bad);
        std::vector<ObjectParticleRecipe> decoded;
        CHECK(!read_object_particles(r, decoded));
        std::cout << "ObjectFX codec: 25 original recipe metadata comparisons, 12.5k mutations/every prefix, little "
                     "endian ids, global bounds/duplicates and source/scale/offset/timestamps passed\n";
        return true;
    }
    bool membership()
    {
        std::string error;
        Resources resources;
        ModelStore models(resources);
        SpriteStore sprites(resources);
        World geometry;
        NetworkWorldReceiver receiver;
        auto s = sample();
        CHECK(receiver.begin(7) && receiver.receive(s, geometry, models, sprites, error));
        auto accept = [&](WorldSnapshot next)
        {
            next.sequence = s.sequence + 1;
            next.time = s.time + .1;
            WorldPatch patch;
            bool changed;
            if (!make_world_patch(s, next, patch, changed) ||
                !receiver.receive(patch, geometry, models, sprites, error))
                return false;
            s = std::move(next);
            return true;
        };
        auto reject = [&](WorldSnapshot next)
        {
            next.sequence = s.sequence + 1;
            next.time = s.time + .1;
            WorldPatch patch;
            bool changed;
            auto sequence = receiver.world().frame().sequence;
            return make_world_patch(s, next, patch, changed) &&
                   !receiver.receive(patch, geometry, models, sprites, error) &&
                   receiver.world().frame().sequence == sequence;
        };
        auto next = s;
        next.objects[0].object_particles[0].source = "mutated";
        CHECK(reject(next));
        next = s;
        auto moved = next.objects[0].object_particles.back();
        next.objects[0].object_particles.pop_back();
        next.objects[1].object_particles.insert(next.objects[1].object_particles.end() - 1, moved);
        CHECK(reject(next));
        next = s;
        next.objects[0].object_particles.erase(next.objects[0].object_particles.begin());
        CHECK(accept(next));
        next = s;
        next.objects[0].object_particles.insert(next.objects[0].object_particles.begin(), {2, 9, 1, 1, {}, "smoke"});
        CHECK(reject(next));
        next = s;
        next.objects[0].object_particles.push_back({6, 10, 24, 1, {}, "new"});
        CHECK(accept(next));
        next = s;
        next.objects[0].id.generation = 2;
        next.objects[0].object_particles = {{4, 9, 19, 1, {}, "shield"}};
        CHECK(reject(next));
        next = s;
        next.objects[0].id.generation = 2;
        next.objects[0].object_particles = {{7, 10, 24, 1, {}, "new-generation"}};
        CHECK(accept(next));
        // Delayed presentation time may precede a newly admitted descriptor's creation.
        ClientWorld delayed;
        CHECK(delayed.begin(7));
        CHECK(receiver.sample(0, {}, delayed, error));
        CHECK(delayed.frame().time == 0 && delayed.get(ObjectId{0, 2})->object_particles[0].created == 10);
        std::cout << "ObjectFX membership: immutable identity, owner migration/retirement/replay refused, new ids and "
                     "owner generations accepted, delayed presentation clock allowed\n";
        return true;
    }
    bool runtime(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        Resources resources;
        CHECK(resources.mount_game(config, error));
        ModelStore models(resources);
        SpriteStore sprites(resources);
        World geometry;
        NetworkWorldReceiver receiver;
        auto s = sample();
        CHECK(receiver.begin(7) && receiver.receive(s, geometry, models, sprites, error));
        NetworkEffects effects;
        NetworkAudio audio;
        CHECK(effects.begin({7, 1, {0, 1}}) && audio.begin({7, 1, {0, 1}}));
        auto tick = [&](double step = 1. / 60)
        {
            return effects.advance(step, receiver.world(), receiver.collision(), models, sprites, resources, audio,
                                   error);
        };
        auto publish = [&](WorldSnapshot next)
        {
            next.sequence = s.sequence + 1;
            next.time = s.time + .1;
            WorldPatch patch;
            bool changed;
            if (!make_world_patch(s, next, patch, changed) ||
                !receiver.receive(patch, geometry, models, sprites, error))
                return false;
            s = std::move(next);
            return true;
        };
        CHECK(tick() && effects.runtime().object_particles.started == 5 && !effects.failed);
        const auto& fx = effects.runtime().object_particles;
        CHECK(fx.active.size() == 5); // Publication ids are intentionally interleaved across owner slots.
        CHECK(tick() && fx.particle_count() > 0);
        auto next = s;
        next.objects[0].pose.position.x = 5;
        next.objects[0].flags = 1;
        CHECK(publish(next) && tick());
        CHECK(fx.started == 5 && !effects.failed);
        // Removing expired metadata does not prematurely delete an already-running shield.
        next = s;
        next.objects[0].object_particles.clear();
        next.objects[1].object_particles.clear();
        CHECK(publish(next) && tick());
        CHECK(std::any_of(fx.active.begin(), fx.active.end(), [](const auto& p) { return p.effect.recipe == 19; }));
        next = s;
        next.objects[0].object_particles = {{6, 0, 12, 1, {}, "already expired"},
                                            {7, s.time, 24, 1, {}, "new persistent"}};
        CHECK(publish(next) && tick() && fx.started == 6); // No history replay for the old finite burst.
        for (unsigned i = 0; i < 360; ++i)
            CHECK(tick());
        CHECK(!effects.failed && fx.active.size() == 1 && fx.active[0].effect.recipe == 24);
        CHECK(fx.particle_count() > 0 && !effects.scene().frame().billboards.empty());
        next = s;
        next.objects.clear();
        CHECK(publish(next) && tick() && fx.active.empty());
        next = s;
        ObjectState owner;
        owner.id = {0, 2};
        owner.object_particles = {{8, s.time, 1, 1, {}, "new trail"}};
        next.objects.push_back(owner);
        CHECK(publish(next) && tick() && fx.started == 7);
        for (unsigned i = 0; i < 5; ++i)
            CHECK(tick());
        CHECK(fx.active[0].effect.duration == 0 && fx.active[0].effect.stop_time >= 0);
        next = s;
        next.objects[0].pose.position.x = 10;
        next.objects[0].flags = 1;
        CHECK(publish(next) && tick());
        CHECK(fx.active[0].effect.moving && !fx.active[0].effect.duration);
        CHECK(effects.begin({8, 2, {0, 1}}) && effects.runtime().object_particles.active.empty());
        std::cout << "Independent ObjectFX: simultaneous owner recipes, authored finite age/no replay, metadata "
                     "retirement, live untimed tail, once-only starts, removal/new generation, multiplayer resume and "
                     "epoch cleanup passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "codec")
        return codec() ? 0 : 1;
    if (argc == 2 && std::string_view(argv[1]) == "membership")
        return membership() ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "runtime")
        return runtime(argv[2]) ? 0 : 1;
    return 1;
}
