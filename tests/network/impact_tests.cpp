#include "../brush_fixture.h"
#include "client/network_audio.h"
#include "client/network_effects.h"
#include "effect_fixture.h"
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
    bool codec()
    {
        std::string error;
        std::minstd_rand random(20);
        for (unsigned kind = 0; kind < 5; ++kind)
        {
            auto event = effect_fixture::sample(kind);
            std::vector<std::uint8_t> bytes, again;
            CHECK(encode_effect(event, bytes));
            CHECK(bytes[0] == 0x42 && bytes[1] == 0x32 && bytes[2] == 0x45 && bytes[3] == 0x46 && bytes[4] == 22 &&
                  bytes[5] == 0 && bytes[6] == kind + 1);
            EffectEvent decoded;
            decoded.sequence = 999;
            for (std::size_t i = 0; i < bytes.size(); ++i)
                CHECK(decode_effect(std::span(bytes).first(i), decoded) != DecodeEffect::Ok && decoded.sequence == 999);
            CHECK(decode_effect(bytes, decoded) == DecodeEffect::Ok && encode_effect(decoded, again) && again == bytes);
            auto extra = bytes;
            extra.push_back(0);
            CHECK(decode_effect(extra, decoded) == DecodeEffect::Malformed);
            for (unsigned n = 0; n < 5000; ++n)
            {
                auto corrupted = bytes;
                for (unsigned j = 0; j < 3; ++j)
                    corrupted[random() % corrupted.size()] = std::uint8_t(random());
                if (decode_effect(corrupted, decoded) == DecodeEffect::Ok)
                    CHECK(valid_effect(decoded) && encode_effect(decoded, again) && again == corrupted);
            }
        }
        auto invalid = [](const InstantEffectRecipe& recipe)
        {
            auto e = effect_fixture::sample(1);
            e.recipe = recipe;
            std::vector<std::uint8_t> unchanged{37};
            return !valid_effect(e) && !encode_effect(e, unchanged) && unchanged == std::vector<std::uint8_t>{37};
        };
        auto sprites = std::get<WeaponSpriteSettings>(effect_fixture::sample(1).recipe);
        auto changed = sprites;
        changed.flash->duration = 0;
        CHECK(invalid(changed));
        changed = sprites;
        changed.flash->fade = 6;
        CHECK(invalid(changed));
        changed = sprites;
        changed.smoke->normal = {1, 0, 0};
        changed.smoke->offset = 1e30f;
        CHECK(invalid(changed));
        changed = sprites;
        changed.smoke->resource = std::string(1001, 'x');
        CHECK(invalid(changed));
        changed = sprites;
        changed.smoke->color.x = 2;
        CHECK(invalid(changed));
        auto spark = std::get<SparkSettings>(effect_fixture::sample(2).recipe);
        spark.count = 256;
        CHECK(invalid(spark));
        spark.count = 0;
        spark.duration = 11;
        CHECK(invalid(spark));
        auto blood = std::get<BloodSpurtRecipe>(effect_fixture::sample(3).recipe);
        blood.settings.fade_time = blood.settings.lifetime;
        CHECK(invalid(blood));
        blood.settings.fade_time = .5f;
        blood.settings.texture = 4;
        CHECK(invalid(blood));
        auto splat = std::get<BloodSplatSettings>(effect_fixture::sample(4).recipe);
        splat.rotation.m_Spin = 2;
        CHECK(invalid(splat));
        splat.rotation = {};
        splat.grow_scale = .2f;
        CHECK(invalid(splat));
        // Optional default resource and authored empty string must remain distinct.
        std::vector<std::uint8_t> absent, empty;
        auto e = effect_fixture::sample(4);
        CHECK(encode_effect(e, absent));
        std::get<BloodSplatSettings>(e.recipe).resource = "";
        CHECK(encode_effect(e, empty) && empty != absent);
        EffectEvent roundtrip;
        CHECK(decode_effect(empty, roundtrip) == DecodeEffect::Ok &&
              std::get<BloodSplatSettings>(roundtrip.recipe).resource == "");
        e.source = {UINT32_MAX, 1};
        CHECK(!valid_effect(e));
        e = effect_fixture::sample(0);
        e.source = {};
        CHECK(!valid_effect(e));
        // Encoded-byte budget must stop large recipes before the item bound.
        NetworkEffects receiver;
        CHECK(receiver.begin({7, 2, {0, 1}}));
        e = effect_fixture::sample(0);
        std::get<ShotgunEffectSettings>(e.recipe).clip_polygon.resize(512, {1, 2, 3});
        CHECK(encode_effect(e, empty));
        const auto capacity = effect_queue_bytes / empty.size();
        CHECK(capacity < effect_queue_limit);
        for (std::size_t i = 0; i < capacity; ++i)
        {
            e.sequence = i + 1;
            CHECK(receiver.receive(e, error));
        }
        e.sequence = capacity + 1;
        CHECK(!receiver.receive(e, error) && receiver.received == capacity);
        CHECK(receiver.begin({8, 3, {0, 2}}));
        e = effect_fixture::sample(3);
        e.world = 8;
        e.player = 3;
        e.sequence = 1;
        CHECK(receiver.receive(e, error));
        // Mixed kinds retain a single sequence; delayed kinds cannot overtake.
        NetworkEffects ordered;
        CHECK(ordered.begin({7, 2, {0, 1}}));
        for (unsigned i = 0; i < 5; ++i)
            CHECK(ordered.receive(effect_fixture::sample(i), error));
        e = effect_fixture::sample(2);
        CHECK(ordered.receive(e, error) && ordered.received == 5);
        e.sequence = 7;
        CHECK(!ordered.receive(e, error));
        for (auto count : std::span(ordered.received_kinds).first(5))
            CHECK(count == 1);
        std::cout << "Impact protocol: all five variants, little-endian tags, full truncation, 25k mutations, optional "
                     "resources, independent item/byte bounds and ordered mixed recipes passed\n";
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
        CHECK(world.begin(7));
        NetworkAudio audio;
        NetworkEffects effects;
        CHECK(audio.begin({7, 2, {0, 1}}) && effects.begin({7, 2, {0, 1}}));
        audio.mixer.inspect_pcm = true;
        for (unsigned i = 0; i < 5; ++i)
            CHECK(effects.receive(effect_fixture::sample(i), error));
        auto deferred = effect_fixture::sample(3);
        deferred.sequence = 6;
        auto& d = std::get<BloodSpurtRecipe>(deferred.recipe);
        d.immediate = false;
        d.settings.position.x = 200;
        CHECK(effects.receive(deferred, error));
        CHECK(effects.advance(1. / 60, world, collision, models, sprites, resources, audio, error));
        const auto& fx = effects.runtime();
        CHECK(!effects.failed && fx.weapon_sprites.flashes.size() == 1 && fx.weapon_sprites.smoke.size() == 2 &&
              fx.sparks.active.size() == 1);
        CHECK(fx.blood_spurts.active.size() == 2 && fx.blood_splats.active.size() == 1 &&
              fx.shotgun_effects.casings.size() == 1);
        const auto& immediate = fx.blood_spurts.active[0];
        const auto& later = fx.blood_spurts.active[1];
        CHECK(immediate.updates == 1 && immediate.emitted == 12 &&
              std::abs(immediate.position.y - (80 - 100.f / 60)) < 1e-5f);
        CHECK(later.updates == 1 && later.position.y == 80 && later.emitted == 12);
        CHECK(fx.weapon_sprites.flashes[0].alpha == 1 && !fx.shotgun_effects.casings[0].visible);
        CHECK(effects.scene().frame().billboards.size() >= 8);
        float peak = 0;
        for (unsigned i = 0; i < 180; ++i)
        {
            CHECK(effects.advance(1. / 60, world, collision, models, sprites, resources, audio, error));
            CHECK(audio.advance(1. / 60, world, {0, 50, 0}, {}));
            peak = std::max(peak, audio.mixer.peak);
        }
        CHECK(!effects.failed && fx.weapon_sprites.flashes.empty() && fx.weapon_sprites.smoke.empty() &&
              fx.sparks.active.empty() && fx.blood_spurts.active.empty());
        CHECK(fx.blood_splats.active.size() >= 3 && audio.effects_started > 0 && peak > 0);
        unsigned impacts = 0, missing_zero = 0;
        for (const auto& r : fx.blood_spurts.reports)
        {
            impacts += unsigned(r.impacts);
            CHECK(r.ended && r.error.empty() && r.splat_error.empty());
            if (!r.sound_error.empty())
            {
                CHECK(r.sound_resource == "sounds/gibs/flesh/gib_impact0.wav" && !resources.find(r.sound_resource));
                ++missing_zero;
            }
        }
        CHECK(impacts == 2);
        // A valid recipe with a missing asset reports locally without aborting the stream.
        auto missing = effect_fixture::sample(1);
        missing.sequence = 7;
        std::get<WeaponSpriteSettings>(missing.recipe).flash->resource = "Sprites/m9_absent.spr";
        CHECK(effects.receive(missing, error) &&
              effects.advance(1. / 60, world, collision, models, sprites, resources, audio, error));
        CHECK(effects.failed == 1 && !effects.last_error.empty() && fx.weapon_sprites.smoke.size() == 1);
        CHECK(effects.begin({8, 3, {0, 2}}) && fx.blood_splats.active.empty() &&
              effects.scene().frame().billboards.empty());
        std::cout
            << "Independent retail impacts: flash/smoke/sparks, immediate/deferred blood exactly one first update, "
               "child splats/audio, source absence, expiration/partial resource failure/world cleanup; impacts="
            << impacts << ", known absent zero=" << missing_zero << ", sounds=" << audio.effects_started
            << ", peak=" << peak << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "codec")
        return codec() ? 0 : 1;
    return argc == 3 && std::string_view(argv[1]) == "runtime" && runtime(argv[2]) ? 0 : 1;
}
