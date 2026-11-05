#include "../brush_fixture.h"
#include "client/collision.h"
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run()
    {
        // This executable links engine_core only, without ObjectDLL or ServerAdapter.
        auto geometry = std::make_shared<CollisionWorld>();
        auto floor = test_brush::box("floor", {-500, -20, -500}, {500, 0, 500});
        for (auto& surface : floor.surfaces)
            surface.flags = 1;
        std::string error;
        CHECK(geometry->build(floor, error));
        ClientCollisionWorld client;
        CHECK(client.begin(7));
        ClientCollisionFrame frame;
        frame.world = 7;
        frame.sequence = 1;
        frame.primary = geometry;
        ClientCollider player;
        player.id = {0, 1};
        player.type = OT_MODEL;
        player.position = {0, 41.001f, 0};
        player.dimensions = {16, 41, 16};
        player.flags = FLAG_SOLID | FLAG_STAIRSTEP;
        ClientCollider wall;
        wall.id = {2, 1};
        wall.flags = FLAG_SOLID;
        wall.position = {0, 100, 80};
        wall.dimensions = {100, 100, 1};
        frame.objects = {player, wall};
        CHECK(client.apply(std::move(frame), error));
        ClientMotionState initial, predicted;
        initial.position = player.position;
        initial.dimensions = player.dimensions;
        const ClientObjectId owner{7, player.id};
        CHECK(client.support(owner, initial) && initial.standing);
        CHECK(client.predict(owner, initial, {0, 41.001f, 120}, {0, 0, 120}, {}, predicted));
        CHECK(predicted.position.z > 62.9f && predicted.position.z < 63.01f && predicted.velocity.z == 0);
        CHECK(predicted.standing);
        CHECK(!client.predict({8, player.id}, initial, {0, 41, 120}, {}, {}, predicted));
        CHECK(!client.predict({7, {0, 2}}, initial, {0, 41, 120}, {}, {}, predicted));
        auto reject = [&](ClientCollisionFrame next)
        {
            const auto sequence = client.frame().sequence;
            return !client.apply(std::move(next), error) && client.frame().sequence == sequence && client.get(wall.id);
        };
        frame = client.frame();
        CHECK(reject(frame));
        frame.sequence = 2;
        frame.world = 8;
        CHECK(reject(frame));
        frame.world = 7;
        frame.primary.reset();
        CHECK(reject(frame));
        frame.primary = geometry;
        frame.objects.push_back(wall);
        CHECK(reject(frame));
        frame.objects.pop_back();
        frame.objects[1].dimensions.y = -1;
        CHECK(reject(frame));
        frame.objects[1] = wall;
        frame.objects[1].velocity.x = std::numeric_limits<float>::infinity();
        CHECK(reject(frame));
        frame.objects[1] = wall;
        frame.objects[1].world_model = true;
        CHECK(reject(frame));
        frame.objects.resize(ClientWorld::object_limit + 1);
        CHECK(reject(std::move(frame)));
        frame = client.frame();
        ++frame.sequence;
        frame.objects[1].ignored_by = player.id;
        CHECK(client.apply(std::move(frame), error));
        CHECK(client.predict(owner, initial, {0, 41.001f, 120}, {0, 0, 120}, {}, predicted));
        CHECK(predicted.position.z == 120); // Freshly thrown Eye ignores only its owner.
        frame = client.frame();
        ++frame.sequence;
        auto stair = std::make_shared<ContainerShape>();
        CHECK(stair->build(test_brush::box("step", {-100, 0, 40}, {100, 16, 90}), {}, error));
        frame.objects[1] = wall;
        frame.objects[1].position = stair->origin;
        frame.objects[1].dimensions = {100, 8, 25};
        frame.objects[1].world_model = true;
        frame.objects[1].shape = stair;
        CHECK(client.apply(std::move(frame), error));
        CHECK(client.predict(owner, initial, {0, 41.001f, 60}, {0, 0, 120}, {}, predicted));
        CHECK(predicted.stair_steps == 1 && std::abs(predicted.position.y - 57.001f) < .01f);
        CHECK(predicted.ground_object == wall.id);
        // An unchanged scene under a newer sequence is admitted as is (no revalidation).
        frame = client.frame();
        const auto renewed = ++frame.sequence;
        CHECK(client.apply(std::move(frame), error) && client.frame().sequence == renewed &&
              client.get(wall.id)->shape == stair);
        frame = client.frame();
        ++frame.sequence;
        frame.objects.pop_back();
        CHECK(client.apply(std::move(frame), error));
        CHECK(!client.get(wall.id));
        CHECK(client.predict(owner, initial, {0, 41.001f, 60}, {0, 0, 120}, {}, predicted));
        CHECK(predicted.position.y == initial.position.y && predicted.stair_steps == 0);
        CHECK(!client.begin(7) && client.begin(8));
        CHECK(!client.frame().primary && client.frame().objects.empty());
        CHECK(!client.predict(owner, initial, {0, 41, 120}, {}, {}, predicted));
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
