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
        std::minstd_rand random(21);
        for (unsigned kind = 5; kind < 10; ++kind)
        {
            auto event = effect_fixture::sample(kind);
            std::vector<std::uint8_t> bytes, again;
            CHECK(encode_effect(event, bytes) && bytes[4] == 21 && bytes[6] == kind + 1);
            EffectEvent decoded;
            decoded.sequence = 999;
            for (std::size_t n = 0; n < bytes.size(); ++n)
                CHECK(decode_effect(std::span(bytes).first(n), decoded) != DecodeEffect::Ok && decoded.sequence == 999);
            CHECK(decode_effect(bytes, decoded) == DecodeEffect::Ok && encode_effect(decoded, again) && again == bytes);
            auto extra = bytes;
            extra.push_back(0);
            CHECK(decode_effect(extra, decoded) == DecodeEffect::Malformed);
            for (unsigned n = 0; n < 5000; ++n)
            {
                auto mutated = bytes;
                for (unsigned i = 0; i < 3; ++i)
                    mutated[random() % mutated.size()] = std::uint8_t(random());
                if (decode_effect(mutated, decoded) == DecodeEffect::Ok)
                    CHECK(encode_effect(decoded, again) && again == mutated);
            }
        }
        auto event = effect_fixture::sample(5);
        std::vector<std::uint8_t> bytes;
        for (unsigned component = 0; component < 6; ++component)
            for (unsigned type = 0; type < 32; ++type)
            {
                event.recipe = ExplosionRecipe{ExplosionComponent(component), type, {}, {0, 1, 0}};
                EffectEvent decoded;
                CHECK(encode_effect(event, bytes) && decode_effect(bytes, decoded) == DecodeEffect::Ok);
                const auto& s = std::get<ExplosionRecipe>(decoded.recipe);
                CHECK(unsigned(s.component) == component && s.type == type);
            }
        auto rejects = [&](InstantEffectRecipe recipe)
        {
            auto e = event;
            e.recipe = std::move(recipe);
            std::vector<std::uint8_t> unchanged{42};
            return !encode_effect(e, unchanged) && unchanged == std::vector<std::uint8_t>{42};
        };
        CHECK(rejects(ExplosionRecipe{ExplosionComponent(6), 0, {}, {0, 1, 0}}));
        CHECK(rejects(ExplosionRecipe{ExplosionComponent::Models, 32, {}, {0, 1, 0}}));
        CHECK(rejects(ExplosionRecipe{ExplosionComponent::Models, 0, {}, {}}));
        auto flesh = std::get<FleshRecipe>(effect_fixture::sample(6).recipe);
        flesh.count = 256;
        CHECK(rejects(flesh));
        flesh.count = 64;
        flesh.dimensions.y = -1;
        CHECK(rejects(flesh));
        auto debris = std::get<DebrisRecipe>(effect_fixture::sample(7).recipe);
        debris.flags = 0x10003;
        CHECK(rejects(debris));
        debris.flags = 6;
        CHECK(rejects(debris));
        debris.flags = 0x4003;
        CHECK(rejects(debris));
        debris.model = "models/gibs/wood/gib1.abc";
        debris.texture = "skins/gibs/wood/gib1.dtx";
        debris.sound = std::string(101, 'x');
        CHECK(rejects(debris));
        debris.sound = "";
        event.recipe = debris;
        event.source = {};
        CHECK(encode_effect(event, bytes));
        event.source = {UINT32_MAX, 1};
        CHECK(!encode_effect(event, bytes));
        CHECK(rejects(LaserRecipe{}));
        CHECK(rejects(LaserRecipe{{-1e7f, 0, 0}, {1e7f, 0, 0}, 1}));
        CHECK(rejects(LightningSettings{{}, {0, 0, 100}, 2, 1, 3}));
        event = effect_fixture::sample(8);
        std::get<LaserRecipe>(event.recipe).type = 255;
        CHECK(encode_effect(event, bytes));
        event = effect_fixture::sample(9);
        auto& bolt = std::get<LightningSettings>(event.recipe);
        bolt.shape = bolt.form = 255;
        CHECK(encode_effect(event, bytes));
        event = effect_fixture::sample(6);
        event.source = {};
        CHECK(!encode_effect(event, bytes));
        NetworkEffects receiver;
        CHECK(receiver.begin({7, 2, {0, 1}}) && !receiver.set_detail(-1) && !receiver.set_detail(3));
        for (unsigned kind = 0; kind < 10; ++kind)
            CHECK(receiver.receive(effect_fixture::sample(kind), error));
        for (const auto n : receiver.received_kinds)
            CHECK(n == 1);
        CHECK(receiver.receive(effect_fixture::sample(9), error) && receiver.received == 10);
        std::cout << "Instant codec: five new kinds, six components/all 32 types, 25k mutations, every prefix, "
                     "bounded custom debris, source identities, fallback bytes and shared ten-kind sequence passed\n";
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
        auto floor = test_brush::box("floor", {-10000, -100, -10000}, {10000, 0, 10000});
        for (auto& surface : floor.surfaces)
            surface.flags = 1;
        CHECK(primary->build(floor, error));
        CollisionHit floor_hit;
        CHECK(primary->sweep({0, 80, 0}, {0, -20, 0}, {}, floor_hit) == CollisionQuery::Hit);
        ClientCollisionWorld collision;
        CHECK(collision.begin(7));
        ClientCollisionFrame frame;
        frame.world = 7;
        frame.sequence = 1;
        frame.primary = primary;
        CHECK(collision.apply(std::move(frame), error));
        primary.reset();
        ClientWorld world;
        NetworkAudio audio;
        NetworkEffects effects;
        CHECK(world.begin(7) && audio.begin({7, 2, {0, 1}}) && effects.begin({7, 2, {0, 1}}));
        CHECK(effects.set_detail(0)); // Local LOW halves only destruction byte counts.
        audio.mixer.inspect_pcm = true;
        std::uint64_t sequence = 0;
        auto send = [&](EffectEvent e)
        {
            e.sequence = ++sequence;
            std::vector<std::uint8_t> bytes;
            EffectEvent decoded;
            return encode_effect(e, bytes) && decode_effect(bytes, decoded) == DecodeEffect::Ok &&
                   effects.receive(decoded, error);
        };
        for (unsigned component = 0; component < 6; ++component)
        {
            auto e = effect_fixture::sample(5);
            auto& s = std::get<ExplosionRecipe>(e.recipe);
            s.component = ExplosionComponent(component);
            if (s.component == ExplosionComponent::Flames)
                s.type = 8;
            CHECK(send(e));
        }
        for (unsigned kind = 6; kind < 10; ++kind)
            CHECK(send(effect_fixture::sample(kind)));
        CHECK(effects.advance(1. / 60, world, collision, models, sprites, resources, audio, error));
        const auto& fx = effects.runtime();
        if (effects.failed)
            error = effects.last_error;
        CHECK(!effects.failed && fx.explosion_models.active.size() == 1 && fx.explosion_sprites.active.size() == 2);
        CHECK(fx.explosion_rings.active.size() == 4 && fx.explosion_flames.active.size() == 1 &&
              !fx.explosion_lights.active.empty() && fx.explosion_fragments.active.size() >= 2);
        CHECK(fx.explosion_fragments.trails.size() == fx.explosion_fragments.active.size());
        CHECK(fx.flesh_gibs.reports[0].requested == 4 && fx.flesh_gibs.active.size() >= 4 &&
              fx.destruction_debris.active.size() == 2 && fx.destruction_debris.reports[0].requested == 4);
        CHECK(fx.blood_spurts.active.size() == fx.flesh_gibs.active.size());
        for (const auto& g : fx.flesh_gibs.active)
            CHECK(g.spurt_event && g.spurt_error.empty());
        for (const auto& blood : fx.blood_spurts.active)
            CHECK(blood.updates == 1); // Exactly one immediate launch update, no ordinary reintegration.
        CHECK(fx.laser_beams.active.size() == 1 && fx.laser_beams.active[0].updates == 1 &&
              fx.lightning.active.size() == 1 && fx.lightning.live_particles() > 0);
        CHECK(!effects.scene().frame().lights.empty() && effects.scene().frame().models.size() >= 8 &&
              !effects.scene().frame().billboards.empty());
        float peak = 0;
        std::uint64_t max_trail_particles = 0;
        for (unsigned tick = 0; tick < 1560; ++tick)
        {
            CHECK(effects.advance(1. / 60, world, collision, models, sprites, resources, audio, error));
            CHECK(audio.advance(1. / 60, world, {0, 50, 0}, {}));
            max_trail_particles =
                std::max(max_trail_particles, std::uint64_t(fx.explosion_fragments.trail_particles()));
            peak = std::max(peak, audio.mixer.peak);
            if (effects.failed)
            {
                std::cerr << "Tick " << tick << " failures=" << effects.failed
                          << " particles=" << fx.explosion_fragments.trail_particles()
                          << " error=" << effects.last_error << '\n';
                for (const auto& trail : fx.explosion_fragments.trails)
                    std::cerr << "Trail " << trail.id << " particles=" << trail.particles.size()
                              << " emitted=" << trail.emitted << " bursts=" << trail.bursts << '\n';
                break;
            }
        }
        if (effects.failed)
            error = effects.last_error;
        CHECK(!effects.failed && max_trail_particles && audio.effects_started && peak > 0);
        std::cout << "After26s pools models/sprites/rings/flames/lights/fragments/trails/laser/bolt="
                  << fx.explosion_models.active.size() << '/' << fx.explosion_sprites.active.size() << '/'
                  << fx.explosion_rings.active.size() << '/' << fx.explosion_flames.active.size() << '/'
                  << fx.explosion_lights.active.size() << '/' << fx.explosion_fragments.active.size() << '/'
                  << fx.explosion_fragments.trails.size() << '/' << fx.laser_beams.active.size() << '/'
                  << fx.lightning.active.size() << '\n';
        CHECK(fx.explosion_models.active.empty() && fx.explosion_sprites.active.empty() &&
              fx.explosion_rings.active.empty() && fx.explosion_flames.active.empty() &&
              fx.explosion_lights.active.empty() && fx.explosion_fragments.active.empty() &&
              fx.explosion_fragments.trails.empty() && fx.laser_beams.active.empty() && fx.lightning.active.empty());
        unsigned gib_impacts = 0, debris_impacts = 0, missing_zero = 0;
        for (const auto& g : fx.flesh_gibs.active)
        {
            CHECK(g.resting && g.error.empty() && g.sound_error.empty() && g.splat_error.empty() && g.splat_event);
            gib_impacts += unsigned(g.impacts);
        }
        for (const auto& g : fx.destruction_debris.active)
        {
            CHECK(g.resting && g.error.empty() && g.sound_error.empty() && !g.splat_event && !g.spurt_event);
            debris_impacts += unsigned(g.impacts);
        }
        for (const auto& r : fx.blood_spurts.reports)
            if (!r.sound_error.empty())
            {
                CHECK(r.sound_resource == "sounds/gibs/flesh/gib_impact0.wav" && !resources.find(r.sound_resource));
                ++missing_zero;
            }
        CHECK(gib_impacts && debris_impacts && !fx.blood_splats.active.empty() && audio.effects_failed == missing_zero);
        // A client resource failure leaves sibling effects and future delivery usable.
        auto bad = effect_fixture::sample(7);
        auto& recipe = std::get<DebrisRecipe>(bad.recipe);
        recipe.flags |= 0x4000;
        recipe.model = "models/m9_absent.abc";
        recipe.texture = "skins/gibs/wood/gib1.dtx";
        CHECK(send(bad) && send(effect_fixture::sample(8)));
        CHECK(effects.advance(1. / 60, world, collision, models, sprites, resources, audio, error));
        CHECK(effects.failed == 1 && !effects.last_error.empty() && fx.laser_beams.active.size() == 1);
        CHECK(effects.begin({8, 3, {0, 2}}) && fx.flesh_gibs.active.empty() && fx.destruction_debris.active.empty() &&
              fx.blood_splats.active.empty() && fx.laser_beams.active.empty() &&
              effects.scene().frame().models.empty());
        std::cout << "Independent instant runtime: six explosion components/trails, flesh children and LOW debris, "
                     "beams/bolts/lights, source absence, expiry/partial failure/world cleanup; gib_impacts="
                  << gib_impacts << ", debris_impacts=" << debris_impacts << ", trail_peak=" << max_trail_particles
                  << ", cosmetic_sounds=" << audio.effects_started << ", known_missing_zero=" << missing_zero
                  << ", PCM_peak=" << peak << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "codec")
        return codec() ? 0 : 1;
    return argc == 3 && std::string_view(argv[1]) == "runtime" && runtime(argv[2]) ? 0 : 1;
}
