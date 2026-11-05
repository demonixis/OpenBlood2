#include "../brush_fixture.h"
#include "client/effect_scene.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    std::string error;
    ClientEffects client;
    CHECK(client.begin(3));
    ClientEffectsFrame frame;
    frame.world = 3;
    frame.sequence = 1;
    frame.time = 1;
    EffectParticles particles;
    particles.texture = "missing.dtx";
    particles.half_extent = .5f;
    particles.particles.push_back({{1, 2, 3}, {}, 0, {1, .5f, 0}});
    frame.billboards.push_back({{EffectKind::Sparks, 1}, particles});
    EffectSprite sprite;
    sprite.pose.position = {3, 4, 5};
    sprite.clip_polygon = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    frame.billboards.push_back({{EffectKind::BloodSplat, 1}, sprite});
    frame.lights.push_back({{{1, 2, 3}, {.5f, .5f, 1}, 100}, 32});
    frame.fog = {true, 10.5f, 200.5f, {255, 128, 0}};
    auto shape = std::make_shared<ContainerShape>();
    CHECK(shape->build(test_brush::box("volume", {-10, -10, -10}, {10, 10, 10}), {}, error));
    frame.volumes.push_back({{2, 1}, shape, {true, -20.9f, 100.9f, {64, 128, 255}}, true});
    auto copy = frame;
    CHECK(client.receive(std::move(copy), error));
    frame.billboards.clear();
    frame.lights.clear();
    CHECK(client.frame().billboards.size() == 2 && client.frame().lights.size() == 1);
    auto fog = gather_scene_effects(client, {}, {});
    CHECK(fog.uniform.fog_eye_near[3] == -20 && fog.uniform.fog_forward_far[3] == 100 &&
          fog.uniform.fog_color[0] == 64.f / 255);
    CHECK(gather_scene_effects(client, {30, 0, 0}, {}).uniform.fog_color[0] == 1);
    const auto accepted = client.frame();
    for (unsigned bad = 0; bad < 9; ++bad)
    {
        copy = accepted;
        copy.sequence = 2;
        if (bad == 0)
            copy.world = 4;
        if (bad == 1)
            copy.sequence = 1;
        if (bad == 2)
            copy.time = .5;
        if (bad == 3)
            copy.billboards.push_back(copy.billboards.front());
        if (bad == 4)
            copy.billboards.resize(ClientEffects::record_limit + 1);
        if (bad == 5)
            std::get<EffectParticles>(copy.billboards[0].value).particles.resize(ClientEffects::particle_limit + 1);
        if (bad == 6)
            std::get<EffectSprite>(copy.billboards[1].value).pose.position.x = NAN;
        if (bad == 7)
            copy.volumes[0].owner.generation = 0;
        if (bad == 8)
            copy.tracers.push_back({{EffectKind::Tracer, 0}, {}, {}, {}});
        CHECK(!client.receive(std::move(copy), error));
        CHECK(client.frame().sequence == 1 && client.frame().billboards.size() == 2);
    }
    {
        // The local solo publisher drops a malformed cosmetic item and keeps the rest.
        ClientEffects lenient;
        CHECK(lenient.begin(5));
        copy = accepted;
        copy.world = 5;
        std::get<EffectSprite>(copy.billboards[1].value).pose.position.x = NAN;
        CHECK(lenient.receive(std::move(copy), error, true) && lenient.frame().billboards.size() == 1 &&
              lenient.dropped_items == 1 && !lenient.drop_error.empty());
        // Stale frames still refuse in that mode.
        copy = accepted;
        copy.world = 5;
        CHECK(!lenient.receive(std::move(copy), error, true));
    }
    ClientEffects second;
    CHECK(second.begin(3));
    copy = accepted;
    CHECK(second.receive(std::move(copy), error));
    copy = accepted;
    copy.sequence = 2;
    copy.billboards.clear();
    copy.lights.clear();
    copy.volumes.clear();
    CHECK(client.receive(std::move(copy), error) && client.frame().billboards.empty());
    CHECK(second.frame().billboards.size() == 2);
    // Core-only consumer can build inspection entries without an original object service.
    Resources resources;
    SpriteView scene;
    CHECK(scene.begin_frame({}, {}, error) && append_effect_sprites(second, scene, {}, true, resources, error));
    CHECK(scene.entries.size() == 2 && scene.entries[0].spark_system == 1 && scene.entries[1].blood_splat == 1);
    CHECK(client.begin(4));
    copy = accepted;
    copy.sequence = 100;
    CHECK(!client.receive(std::move(copy), error) && client.frame().billboards.empty());
    return 0;
}
