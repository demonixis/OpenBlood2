#include "../brush_fixture.h"
#include "../model_fixture.h"
#include "client/effect_queries.h"
#include "client/original_effects.h"
#include "client/session.h"
#include "game/original/ExplosionFragFX.h"
#include <cmath>
#include <iostream>
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
    ClientSession client{std::make_unique<OriginalEffects>()};
    auto* owned = static_cast<OriginalEffects*>(client.effect_runtime());
    CHECK(owned);
    owned->sparks.started = 7;
    client.reset_local_world();
    CHECK(client.effect_runtime() == owned && owned->sparks.started == 0);
    ClientCollisionWorld scene;
    CHECK(scene.begin(3));
    ClientCollisionFrame frame;
    frame.world = 3;
    frame.sequence = 1;
    auto primary = std::make_shared<CollisionWorld>();
    auto floor = test_brush::box("floor", {-500, -10, -500}, {500, 0, 500});
    for (auto& surface : floor.surfaces)
        surface.flags = 1;
    CHECK(primary->build(floor, error));
    frame.primary = primary;
    ClientCollider obstacle;
    obstacle.id = {1, 1};
    obstacle.position = {0, 50, 10};
    obstacle.dimensions = {5, 5, 1};
    obstacle.flags = FLAG_RAYHIT;
    frame.objects.push_back(obstacle);
    auto volume = std::make_shared<ContainerShape>();
    CHECK(volume->build(test_brush::box("water", {-10, 40, -10}, {10, 60, 10}), {}, error));
    ClientCollider water;
    water.id = {2, 1};
    water.container = true;
    water.shape = volume;
    water.environment = ContainerEnvironment{CC_WATER};
    frame.objects.push_back(water);
    CHECK(scene.apply(std::move(frame), error));
    primary.reset();
    volume.reset();
    ClientEffectQueries query(&scene), unavailable(nullptr);
    ClientSegmentHit hit;
    CHECK(query.effect_segment({0, 50, 0}, {0, 50, 20}, hit) == SegmentResult::Miss);
    CHECK(query.effect_segment({0, 50, 0}, {0, 50, 20}, hit, SegmentObjects::SolidOrRayHit) == SegmentResult::Hit);
    CHECK(hit.object == obstacle.id && hit.point.z == 9);
    CHECK(query.effect_world_segment({0, 50, 0}, {0, 50, 20}, hit) == SegmentResult::Miss);
    CHECK(query.effect_world_ray({0, 5, 0}, {0, -1, 0}, hit) == SegmentResult::Hit && !hit.object);
    ContainerCode code = CC_NOTHING;
    CHECK(query.effect_container({0, 50, 0}, code) && code == CC_WATER);
    CHECK(query.effect_container({0, 50, 0}, code, true) && code == CC_NOTHING);
    CHECK(unavailable.effect_segment({0, 5, 0}, {0, -5, 0}, hit) == SegmentResult::Unavailable);
    CHECK(unavailable.effect_world_ray({}, {}, hit) == SegmentResult::Invalid);
    CHECK(!unavailable.effect_container({}, code));
    // Original fragment update runs with an owned decoded model and client geometry,
    // without creating a ServerAdapter, original registry, or any server callback.
    auto model = std::make_shared<Model>();
    CHECK(read_model(test_model::abc().b, *model, error));
    CExplosionFragFX fragment;
    ExplosionFragSettings settings;
    settings.position = {0, 5, 0};
    settings.velocity = 4;
    settings.gravity = 1;
    settings.duration = 10;
    settings.fade_time = 2;
    CHECK(fragment.Init(settings, error));
    fragment.model = model;
    fragment.velocity = {0, -4, 0};
    CHECK(fragment.Update(0, query));
    CHECK(fragment.impacts == 1 && fragment.position.y == 1 && fragment.velocity.y == 1.25f);
    CHECK(fragment.Update(.01, query) && fragment.position.y == 1.25f && fragment.velocity.y == .25f);
    CHECK(fragment.Update(.02, query) && !fragment.moving);
    CHECK(fragment.Update(9, query) && fragment.alpha == .5f);
    CHECK(!fragment.Update(10, query) && fragment.error.empty());
    CHECK(scene.begin(4));
    CHECK(query.effect_world_segment({0, 5, 0}, {0, -5, 0}, hit) == SegmentResult::Unavailable);
    CHECK(!query.effect_container({}, code));
    return 0;
}
