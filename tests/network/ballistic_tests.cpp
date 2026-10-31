#include "../brush_fixture.h"
#include "client/network_audio.h"
#include "client/network_effects.h"
#include "net/codec.h"
#include <iostream>
#include <random>
using namespace std::string_literals;
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
    EffectEvent sample()
    {
        EffectEvent e{7, 2, 1, {15, 3}, "detached source", {}};
        auto& s = std::get<ShotgunEffectSettings>(e.recipe);
        s.source = {50, 120, 0};
        s.destination = {100, 299, 0};
        s.forward = {1, 0, 0};
        s.normal = {0, -1, 0};
        s.eject = true;
        s.ammo = AMMO_BULLET;
        s.surface = SURFTYPE_METAL;
        s.clip_polygon = {{0, 299, -100}, {200, 299, -100}, {200, 299, 100}, {0, 299, 100}};
        return e;
    }
    bool codec()
    {
        std::string error;
        auto e = sample();
        std::vector<std::uint8_t> bytes, again;
        CHECK(encode_effect(e, bytes));
        const std::vector<std::uint8_t> header{0x42, 0x32, 0x45, 0x46, 21, 0, 1, 7, 0, 0, 0, 0,
                                               0,    0,    0,    2,    0,  0, 0, 0, 0, 0, 0, 1};
        CHECK(std::equal(header.begin(), header.end(), bytes.begin()));
        EffectEvent decoded = sample();
        decoded.sequence = 99;
        for (std::size_t n = 0; n < bytes.size(); ++n)
            CHECK(decode_effect(std::span(bytes).first(n), decoded) != DecodeEffect::Ok && decoded.sequence == 99);
        CHECK(decode_effect(bytes, decoded) == DecodeEffect::Ok && encode_effect(decoded, again) && again == bytes);
        bytes.push_back(0);
        CHECK(decode_effect(bytes, decoded) == DecodeEffect::Malformed);
        bytes.pop_back();
        bytes[4] = 0;
        CHECK(decode_effect(bytes, decoded) == DecodeEffect::Protocol);
        for (unsigned bad = 0; bad < 12; ++bad)
        {
            auto v = e;
            switch (bad)
            {
                case 0:
                    v.world = 0;
                    break;
                case 1:
                    v.player = 0;
                    break;
                case 2:
                    v.sequence = 0;
                    break;
                case 3:
                    v.source.generation = 0;
                    break;
                case 4:
                    v.source.slot = 65536;
                    break;
                case 5:
                    v.name.assign(101, 'a');
                    break;
                case 6:
                    v.name = "a\0b"s;
                    break;
                case 7:
                    std::get<ShotgunEffectSettings>(v.recipe).forward = {};
                    break;
                case 8:
                    std::get<ShotgunEffectSettings>(v.recipe).normal.y = std::nanf("");
                    break;
                case 9:
                    std::get<ShotgunEffectSettings>(v.recipe).color1.x = 256;
                    break;
                case 10:
                    std::get<ShotgunEffectSettings>(v.recipe).ammo = AMMO_FUEL;
                    break;
                case 11:
                    std::get<ShotgunEffectSettings>(v.recipe).clip_polygon.resize(513);
                    break;
            }
            CHECK(!valid_effect(v) && !encode_effect(v, again));
        }
        std::get<ShotgunEffectSettings>(e.recipe).clip_polygon.resize(512, {1, 2, 3});
        e.name.assign(100, 'a');
        CHECK(encode_effect(e, bytes) && bytes.size() < effect_packet_limit);
        for (std::size_t n = 0; n < bytes.size(); ++n)
            CHECK(decode_effect(std::span(bytes).first(n), decoded) != DecodeEffect::Ok);
        CHECK(decode_effect(bytes, decoded) == DecodeEffect::Ok &&
              std::get<ShotgunEffectSettings>(decoded.recipe).clip_polygon.size() == 512);
        auto over = bytes;
        over.resize(effect_packet_limit + 1);
        CHECK(decode_effect(over, decoded) == DecodeEffect::Malformed);
        std::minstd_rand random(7);
        for (unsigned n = 0; n < 10000; ++n)
        {
            auto mutation = bytes;
            mutation.resize(random() % (bytes.size() + 1));
            for (unsigned j = 0; j < 4 && !mutation.empty(); ++j)
                mutation[random() % mutation.size()] = std::uint8_t(random());
            if (decode_effect(mutation, decoded) == DecodeEffect::Ok)
                CHECK(valid_effect(decoded));
        }
        NetworkEffects effects;
        CHECK(effects.begin({7, 2, {0, 1}}));
        e = sample();
        CHECK(effects.receive(e, error) && effects.receive(e, error) && effects.received == 1);
        e.sequence = 3;
        CHECK(!effects.receive(e, error));
        e.sequence = 2;
        e.player = 3;
        CHECK(!effects.receive(e, error));
        e.player = 2;
        for (; e.sequence <= effect_queue_limit; ++e.sequence)
            CHECK(effects.receive(e, error));
        CHECK(!effects.receive(e, error) && effects.received == effect_queue_limit);
        CHECK(effects.begin({8, 3, {0, 2}}) && effects.scene().frame().models.empty() && !effects.receive(e, error));
        std::cout << "Ballistic codec: portable header, full truncation, 512-point/8KiB bounds, 10k mutations; "
                     "duplicate/gap/private/world and 2048-event/2MiB pending limits passed\n";
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
        auto primary = std::make_shared<CollisionWorld>();
        auto floor = test_brush::box("floor", {-4000, -100, -4000}, {4000, 0, 4000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        CHECK(primary->build(floor, error));
        ClientCollisionWorld collision;
        CHECK(collision.begin(7));
        ClientCollisionFrame c;
        c.world = 7;
        c.sequence = 1;
        c.primary = primary;
        CHECK(collision.apply(std::move(c), error));
        primary.reset();
        ClientWorld world;
        CHECK(world.begin(7)); // Source object deliberately absent: effects are detached.
        NetworkEffects effects, other;
        NetworkAudio audio;
        CHECK(effects.begin({7, 2, {0, 1}}) && other.begin({7, 3, {1, 1}}) && audio.begin({7, 2, {0, 1}}));
        audio.mixer.inspect_pcm = true;
        auto e = sample();
        for (unsigned i = 0; i < 3; ++i)
        {
            e.sequence = i + 1;
            std::get<ShotgunEffectSettings>(e.recipe).ammo = DBYTE(i + 1);
            std::get<ShotgunEffectSettings>(e.recipe).eject = i < 2;
            std::get<ShotgunEffectSettings>(e.recipe).left = i == 1;
            std::get<ShotgunEffectSettings>(e.recipe).source.x += 100;
            std::get<ShotgunEffectSettings>(e.recipe).destination.x += 100;
            CHECK(effects.receive(e, error));
        }
        CHECK(effects.advance(1. / 60, world, collision, models, sprites, resources, audio, error));
        const auto& fx = effects.runtime().shotgun_effects;
        CHECK(!effects.failed && effects.received == 3 && fx.casings.size() == 3 && fx.sprites.size() == 3 &&
              !fx.tracers.empty());
        CHECK(fx.reports[0].casings == 1 && fx.reports[1].casings == 2 && fx.reports[2].casings == 0);
        CHECK(effects.scene().frame().models.size() == 3 && effects.scene().frame().billboards.size() >= 5);
        CHECK(other.runtime().shotgun_effects.casings.empty());
        float peak = 0;
        for (unsigned i = 0; i < 1500; ++i)
        {
            CHECK(effects.advance(1. / 60, world, collision, models, sprites, resources, audio, error));
            CHECK(audio.advance(1. / 60, world, {200, 50, 0}, {}));
            peak = std::max(peak, audio.mixer.peak);
        }
        CHECK(!effects.failed && audio.effects_started > 0 && peak > 0 && audio.mixer.voices().empty());
        CHECK(fx.casings.empty() && fx.tracers.empty() && fx.sprites.size() == 3);
        CHECK(effects.scene().frame().models.empty() && effects.scene().frame().billboards.size() == 3);
        CHECK(!effects.advance(-1, world, collision, models, sprites, resources, audio, error));
        CHECK(effects.begin({8, 3, {0, 2}}) && effects.scene().frame().billboards.empty() && fx.sprites.empty());
        std::cout << "Independent retail ballistic runtime: bullet/shell/BMG, detached source, replica queries, "
                     "owned marks/smoke/tracers/models, collision sounds and 25s lifetime/map cleanup; sounds="
                  << audio.effects_started << ", peak=" << peak << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "codec")
        return codec() ? 0 : 1;
    return argc == 3 && std::string_view(argv[1]) == "runtime" && runtime(argv[2]) ? 0 : 1;
}
