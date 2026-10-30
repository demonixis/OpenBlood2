#include "../brush_fixture.h"
#include "client/flashlight.h"
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
    ClientWorld world;
    ClientCollisionWorld scene;
    CHECK(world.begin(7) && scene.begin(7));
    ClientWorldFrame frame;
    frame.world = 7;
    frame.sequence = 1;
    ClientObject owner, source;
    owner.id = {1, 1};
    owner.pose.position = {0, 100, 0};
    owner.flags = FLAG_SOLID;
    source.id = {2, 1};
    source.pose = owner.pose;
    source.pose.rotation = rotation_from_euler({1.57079632679f, 0, 0});
    source.user_flags = 1;
    frame.objects = {owner, source};
    auto publish = [&]
    {
        auto copy = frame;
        return world.apply(std::move(copy), error);
    };
    CHECK(publish());
    auto primary = std::make_shared<CollisionWorld>();
    auto floor = test_brush::box("floor", {-500, -10, -500}, {500, 0, 500});
    for (auto& surface : floor.surfaces)
        surface.flags = 1;
    CHECK(primary->build(floor, error));
    ClientCollisionFrame collision;
    collision.world = 7;
    collision.sequence = 1;
    collision.primary = primary;
    ClientCollider body;
    body.id = owner.id;
    body.flags = FLAG_SOLID;
    body.position = owner.pose.position;
    body.dimensions = {20, 20, 20};
    collision.objects = {body};
    auto geometry = [&]
    {
        auto copy = collision;
        return scene.apply(std::move(copy), error);
    };
    CHECK(geometry());
    CFlashlightFX fx;
    auto owner_id = world.reference(owner.id), source_id = world.reference(source.id);
    auto update = [&] { return fx.Update(world, &scene, source_id, owner_id); };
    CHECK(update() && fx.visible && !fx.hit.object && std::abs(fx.distance - 100) < .01f);
    CHECK(std::abs(fx.light.radius - 40) < .01f && std::abs(fx.light.color.x - .95f) < .001f);
    CHECK(std::abs(fx.shade_position.y - 100) < .01f && !fx.shade_available);
    const auto updates = fx.updates;
    CHECK(fx.Current(world).radius == fx.light.radius && fx.updates == updates);
    ClientCollider obstacle;
    obstacle.id = {3, 1};
    obstacle.flags = FLAG_SOLID;
    obstacle.position = {0, 50, 0};
    obstacle.dimensions = {5, 5, 5};
    collision.objects.push_back(obstacle);
    collision.sequence++;
    CHECK(geometry() && update() && fx.hit.object == obstacle.id && std::abs(fx.distance - 45) < .01f);
    collision.objects.pop_back();
    collision.sequence++;
    CHECK(geometry());
    // Current-frame hide is immediate; querying presentation does not run another ray.
    frame.objects[1].user_flags = 0;
    frame.sequence++;
    CHECK(publish() && fx.Current(world).radius == 0 && update() && !fx.visible);
    frame.objects[1].user_flags = 1;
    frame.objects[1].pose.position = {0, 2, 0};
    frame.sequence++;
    CHECK(publish() && update() && fx.distance == 10 && fx.light.radius == 4);
    frame.objects[1].pose.position = {0, 2000, 0};
    frame.sequence++;
    CHECK(publish() && update() && std::abs(fx.light.color.x - 132.f / 255) < .0001f);
    frame.objects[1].pose.rotation = rotation_from_euler({-1.57079632679f, 0, 0});
    frame.sequence++;
    CHECK(publish() && update() && !fx.visible && fx.result == SegmentResult::Miss && !fx.hit.object);
    frame.objects[1].pose.rotation = source.pose.rotation;
    frame.sequence++;
    CHECK(publish());
    CHECK(!fx.Update(world, nullptr, source_id, owner_id) && !fx.visible && fx.result == SegmentResult::Unavailable);
    CHECK(update() && fx.visible);
    frame.objects[1].id.generation++;
    frame.sequence++;
    CHECK(publish() && fx.Current(world).radius == 0 && update() && !fx.visible);
    source_id = world.reference(frame.objects[1].id);
    CHECK(update() && fx.visible);
    frame.objects.erase(frame.objects.begin());
    frame.sequence++;
    CHECK(publish() && fx.Current(world).radius == 0 && update() && !fx.visible);
    // Even matching slot/generation values in a new map cannot revive old references.
    CHECK(world.begin(8) && scene.begin(8));
    frame.world = 8;
    frame.sequence = 1;
    frame.objects = {owner, source};
    collision.world = 8;
    collision.sequence = 1;
    CHECK(publish() && geometry() && update() && !fx.visible);
    source_id = world.reference(source.id);
    owner_id = world.reference(owner.id);
    CHECK(update() && fx.visible);
    ClientCollisionWorld wrong_world;
    CHECK(wrong_world.begin(9));
    CHECK(!fx.Update(world, &wrong_world, source_id, owner_id) && !fx.visible &&
          fx.result == SegmentResult::Unavailable);
    return 0;
}
