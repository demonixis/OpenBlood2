#include "net/admission.h"
#include "client/network_audio.h"
#include "client/network_effects.h"
#include "client/network_world.h"
#include "net/particle_stream_codec.h"
#include <iostream>
#include <random>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
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
    WorldSnapshot sample()
    {
        WorldSnapshot s;
        s.world = 7;
        s.sequence = 1;
        s.time = 100;
        ObjectState a;
        a.id = {0, 1};
        a.user_flags = 1;
        a.particle_stream.emplace();
        auto& p = *a.particle_stream;
        p.on = true;
        p.count = 4;
        p.delay = .02f;
        p.ramp_time = .1f;
        p.min_life = p.max_life = .15f;
        p.ramp_flags = 15;
        s.objects.push_back(a);
        a.id = {1, 1};
        a.particle_stream->system = true;
        a.particle_stream->particles_per_second = 300; // Original floor(rate * elapsed), with no fractional carry.
        a.particle_stream->min_velocity = -5;
        s.objects.push_back(a);
        return s;
    }
    bool codec()
    {
        std::string error;
        auto s = sample();
        std::vector<std::uint8_t> bytes, again;
        CHECK(encode_world_snapshot(s, bytes) && bytes[4] == std::uint8_t(OpenBlood2::Net::protocol_version));
        WorldSnapshot decoded;
        decoded.sequence = 999;
        for (std::size_t n = 0; n < bytes.size(); ++n)
            CHECK(!decode_world_snapshot(std::span(bytes).first(n), decoded) && decoded.sequence == 999);
        CHECK(decode_world_snapshot(bytes, decoded));
        CHECK(same_particle_stream(*decoded.objects[0].particle_stream, *s.objects[0].particle_stream));
        CHECK(encode_world_snapshot(decoded, again) && bytes == again);
        std::minstd_rand rng(22);
        for (unsigned i = 0; i < 10000; ++i)
        {
            auto changed = bytes;
            changed[rng() % changed.size()] = std::uint8_t(rng());
            if (decode_world_snapshot(changed, decoded))
                CHECK(encode_world_snapshot(decoded, again) && again == changed);
        }
        auto extra = bytes;
        extra.push_back(0);
        CHECK(!decode_world_snapshot(extra, decoded));
        auto invalid = s;
        invalid.objects[0].particle_stream->count = 65536;
        CHECK(!encode_world_snapshot(invalid, again));
        invalid = s;
        invalid.objects[0].particle_stream->sounds[0] = std::string(1001, 'x');
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].particle_stream->ramp_flags = 16;
        CHECK(!valid_snapshot(invalid));
        invalid = s;
        invalid.objects[0].particle_stream->alpha = std::numeric_limits<float>::infinity();
        CHECK(!valid_snapshot(invalid));
        // Four strings and all original settings, including the unused sound tail.
        auto p = *s.objects[0].particle_stream;
        p.texture = std::string(1000, 't');
        p.sounds.fill(std::string(1000, 's'));
        Writer writer(8192);
        write_particle_stream(writer, p);
        Reader reader(writer.bytes());
        ParticleStreamSettings q;
        CHECK(read_particle_stream(reader, q) && reader.done() && same_particle_stream(p, q));
        auto malformed = writer.bytes();
        malformed[0] = 2;
        Reader bad(malformed);
        CHECK(!read_particle_stream(bad, q));
        auto crowded = s;
        crowded.objects.clear();
        for (unsigned i = 0; i < 129; ++i)
        {
            auto o = s.objects[0];
            o.id.slot = i;
            crowded.objects.push_back(o);
        }
        CHECK(!valid_snapshot(crowded));
        crowded.objects.pop_back();
        CHECK(valid_snapshot(crowded));
        auto after = s;
        after.sequence = 2;
        after.objects[0].user_flags = 0;
        WorldPatch patch;
        bool changed = false;
        CHECK(make_world_patch(s, after, patch, changed) && changed && patch.values.objects.size() == 1);
        CHECK(encode_world_patch(patch, bytes) && decode_world_patch(bytes, patch));
        CHECK(patch.values.objects[0].particle_stream &&
              same_particle_stream(*s.objects[0].particle_stream, *patch.values.objects[0].particle_stream));
        std::cout << "Stream codec: 10k mutations, every prefix, canonical booleans, 128-emitter cap, original "
                     "settings/sound tail and membership patches passed\n";
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
        CHECK(receiver.begin(7));
        auto s = sample();
        CHECK(receiver.receive(s, geometry, models, sprites, error));
        NetworkEffects effects;
        NetworkAudio audio;
        CHECK(effects.begin({7, 1, {0, 1}}) && audio.begin({7, 1, {0, 1}}));
        auto tick = [&]
        {
            return effects.advance(1. / 60, receiver.world(), receiver.collision(), models, sprites, resources, audio,
                                   error);
        };
        auto publish = [&](WorldSnapshot next)
        {
            next.sequence = s.sequence + 1;
            next.time = s.time + .05;
            WorldPatch p;
            bool changed;
            if (!make_world_patch(s, next, p, changed) || !receiver.receive(p, geometry, models, sprites, error))
                return false;
            s = std::move(next);
            return true;
        };
        CHECK(tick() && effects.runtime().particle_streams.started == 2);
        const auto first = effects.runtime().particle_streams.active[0].effect.id;
        for (unsigned i = 0; i < 60; ++i)
            CHECK(tick());
        CHECK(effects.runtime().particle_streams.particle_count() > 0 && !effects.scene().frame().billboards.empty());
        CHECK(effects.runtime().particle_streams.active[1].effect.emitted > 0 && !effects.failed);
        auto next = s;
        next.objects[0].pose.position = {100, 20, 30};
        next.objects[0].user_flags = 0;
        next.objects[1].user_flags = 0;
        CHECK(publish(next));
        for (unsigned i = 0; i < 60; ++i)
            CHECK(tick());
        CHECK(effects.runtime().particle_streams.started == 2 &&
              effects.runtime().particle_streams.active[0].effect.id == first);
        CHECK(effects.runtime().particle_streams.active[0].effect.position.x == 100 &&
              effects.runtime().particle_streams.active[0].effect.state == 0);
        CHECK(effects.runtime().particle_streams.active[0].effect.particles.empty() &&
              !effects.runtime().particle_streams.active[1].effect.system_visible);
        NetworkEffects late;
        CHECK(late.begin({7, 2, {0, 1}}));
        CHECK(late.advance(1. / 60, receiver.world(), receiver.collision(), models, sprites, resources, audio, error));
        CHECK(late.runtime().particle_streams.started == 2 && !late.runtime().particle_streams.particle_count());
        CHECK(late.runtime().particle_streams.active[0].effect.state ==
              0); // Stale original On=true cannot flash on arrival.
        next = s;
        next.objects[0].user_flags = 1;
        next.objects[1].user_flags = 1;
        CHECK(publish(next));
        for (unsigned i = 0; i < 60; ++i)
            CHECK(tick());
        CHECK(effects.runtime().particle_streams.particle_count() > 0);
        CHECK(effects.runtime().particle_streams.started == 2);
        auto illegal = s;
        illegal.sequence++;
        illegal.time += .05;
        illegal.objects[0].particle_stream->count = 3;
        WorldPatch patch;
        bool changed;
        CHECK(make_world_patch(s, illegal, patch, changed));
        const auto sequence = receiver.world().frame().sequence;
        CHECK(!receiver.receive(patch, geometry, models, sprites, error) &&
              receiver.world().frame().sequence == sequence);
        next = s;
        next.objects.clear();
        CHECK(publish(next) && tick());
        CHECK(effects.runtime().particle_streams.active.empty() && effects.runtime().particle_streams.expired == 2);
        next = s;
        auto reused = sample().objects[0];
        reused.id.generation = 2;
        next.objects.push_back(reused);
        CHECK(publish(next) && tick() && effects.runtime().particle_streams.started == 3);
        CHECK(effects.runtime().particle_streams.active[0].effect.id > first);
        next = s;
        next.objects.clear();
        CHECK(publish(next) && tick());
        reused.id.generation = 3;
        reused.particle_stream->texture = "missing-network-stream.dtx";
        next = s;
        next.objects.push_back(reused);
        CHECK(publish(next) && tick() && effects.failed == 1 && !effects.last_error.empty());
        for (unsigned i = 0; i < 30; ++i)
            CHECK(tick());
        CHECK(effects.failed == 1 && effects.runtime().particle_streams.active.empty());
        CHECK(effects.begin({8, 3, {0, 1}}) && effects.runtime().particle_streams.active.empty());
        std::cout << "Stream runtime: independent original stream/system emission, toggles/ramp, pose, off late join, "
                     "no restart, immutable recipe, generation reuse, missing texture once and epoch cleanup passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && std::string_view(argv[1]) == "codec"     ? !codec()
           : argc == 3 && std::string_view(argv[1]) == "runtime" ? !runtime(argv[2])
                                                                 : 1;
}
