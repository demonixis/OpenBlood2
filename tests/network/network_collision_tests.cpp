#include "../brush_fixture.h"
#include "client/network_world.h"
#include <iostream>
#include <random>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run()
    {
        std::string error;
        Resources resources;
        ModelStore models(resources);
        SpriteStore sprites(resources);
        World geometry;
        geometry.models.push_back(test_brush::box("primary", {-100, -10, -100}, {100, 0, 100}));
        for (auto& surface : geometry.models.front().surfaces)
            surface.flags |= 1; // DAT SURF_SOLID.
        geometry.models.push_back(test_brush::box("volume", {-10, -5, -10}, {10, 20, 10}));
        geometry.models.back().translation = {30, 0, 0}; // Vertices already use world coordinates.
        geometry.models.push_back(test_brush::box("wall", {40, 0, -10}, {45, 20, 10}));
        geometry.models.back().translation = {40, 0, 0};
        WorldSnapshot initial;
        initial.world = 5;
        initial.sequence = 1;
        initial.player = {0, 1};
        initial.player_alive = true;
        ObjectState player;
        player.id = {0, 1};
        player.flags = FLAG_SOLID;
        player.pose.position = {0, 2.001f, 0}; // Existing collision solver's contact separation.
        player.collision.enabled = true;
        player.collision.motion.dimensions = {1, 2, 1};
        initial.objects.push_back(player);
        for (unsigned i = 1; i <= 6; ++i)
        {
            ObjectState volume;
            volume.id = {i, 1};
            volume.type = OT_CONTAINER;
            volume.filename = "volume";
            volume.world_model_index = 1;
            volume.pose.position = {30, 0, 0};
            volume.collision.enabled = true;
            volume.collision.container = true;
            volume.collision.volume_brush = true;
            volume.collision.motion.dimensions = {10, 12.5f, 10};
            volume.collision.motion.shape_origin = {30, 0, 0};
            volume.collision.environment = {i == 1 ? CC_WATER : CC_LADDER, {float(i), 0, 0}, -200, .25f, i == 1};
            initial.objects.push_back(volume);
        }
        ObjectState wall;
        wall.id = {7, 1};
        wall.type = OT_WORLDMODEL;
        wall.filename = "wall";
        wall.world_model_index = 2;
        wall.world_model_bound = true;
        wall.flags = FLAG_SOLID;
        wall.pose.position = {40, 0, 0};
        wall.collision.enabled = true;
        wall.collision.world_model = true;
        wall.collision.motion.dimensions = {2.5f, 10, 10};
        wall.collision.motion.shape_origin = {40, 0, 0};
        initial.objects.push_back(wall);
        ObjectState ignored;
        ignored.id = {8, 1};
        ignored.flags = FLAG_SOLID;
        ignored.pose.position = {20, 15, 0};
        ignored.collision.enabled = true;
        ignored.collision.ignored_by = player.id;
        ignored.collision.motion.dimensions = {2, 2, 2};
        initial.objects.push_back(ignored);
        std::vector<std::uint8_t> encoded;
        CHECK(encode_world_snapshot(initial, encoded));
        auto malformed = initial;
        malformed.objects[7].collision.world_model = false;
        std::vector<std::uint8_t> untouched{42};
        CHECK(!encode_world_snapshot(malformed, untouched) && untouched == std::vector<std::uint8_t>{42});
        malformed = initial;
        malformed.objects[1].collision.container = false;
        malformed.objects[1].collision.volume_brush = false;
        malformed.objects[1].collision.environment.reset();
        CHECK(!encode_world_snapshot(malformed, untouched));
        WorldSnapshot decoded;
        CHECK(decode_world_snapshot(encoded, decoded));
        for (std::size_t i = 0; i < encoded.size(); ++i)
        {
            decoded.sequence = 999;
            CHECK(!decode_world_snapshot(std::span(encoded).first(i), decoded) && decoded.sequence == 999);
        }
        std::mt19937 random(9145);
        for (unsigned i = 0; i < 10000; ++i)
        {
            auto bytes = encoded;
            bytes[random() % bytes.size()] ^= std::uint8_t(1u << (random() % 8));
            WorldSnapshot mutated;
            if (decode_world_snapshot(bytes, mutated))
            {
                std::vector<std::uint8_t> canonical;
                CHECK(encode_world_snapshot(mutated, canonical) && canonical == bytes);
            }
        }
        CHECK(decode_world_snapshot(encoded, decoded));
        NetworkWorldReceiver receiver;
        CHECK(receiver.begin(5) && receiver.receive(decoded, geometry, models, sprites, error));
        CHECK(receiver.collision().frame().sequence == receiver.world().frame().sequence);
        CHECK(receiver.collision().frame().primary->triangles() == 12);
        const auto& collision = receiver.collision();
        ClientMotionState body;
        body.position = player.pose.position;
        body.dimensions = player.collision.motion.dimensions;
        ClientContainerState environment;
        CHECK(collision.environment({5, player.id}, body, body.position, environment));
        CHECK(environment.count == 4 && environment.total == 6 && environment.head.hidden &&
              environment.head.code == CC_NOTHING);
        CHECK(environment.body[0].code == CC_WATER && environment.body[3].current.x == 4);
        CHECK(collision.support({5, player.id}, body) && body.standing);
        ClientSegmentHit hit;
        CHECK(collision.intersect({0, 10, 0}, {100, 10, 0}, {5, player.id}, hit) == SegmentResult::Hit &&
              hit.object == wall.id);
        CHECK(hit.point.x == 40);
        ClientCollisionHit sweep;
        CHECK(collision.sweep({5, player.id}, {0, 15, 0}, {100, 15, 0}, {}, sweep) == CollisionQuery::Hit &&
              sweep.object == wall.id);
        CHECK(collision.sweep({5, {}}, {0, 15, 0}, {100, 15, 0}, {}, sweep) == CollisionQuery::Hit &&
              sweep.object == ignored.id);
        CHECK(collision.intersect({0, 15, 0}, {100, 15, 0}, {5, player.id}, hit) == SegmentResult::Hit &&
              hit.object == ignored.id);
        const auto old_shape = collision.get(wall.id)->shape;
        auto after = initial;
        after.sequence = 2;
        after.time = .05;
        after.objects[1].collision.environment = {CC_CONVEYOR, {120, 0, 0}, 0, 0, false};
        WorldPatch patch;
        bool changed = false;
        CHECK(make_world_patch(initial, after, patch, changed) && changed);
        CHECK(receiver.receive(patch, geometry, models, sprites, error));
        CHECK(collision.environment({5, player.id}, body, body.position, environment));
        CHECK(environment.head.code == CC_CONVEYOR && environment.body[0].current.x == 120);
        CHECK(collision.get(wall.id)->shape == old_shape);
        auto broken = after;
        broken.sequence = 3;
        broken.objects[1].world_model_index = 2000;
        CHECK(make_world_patch(after, broken, patch, changed));
        const auto accepted = receiver.world().frame().sequence;
        CHECK(!receiver.receive(patch, geometry, models, sprites, error));
        CHECK(receiver.world().frame().sequence == accepted && collision.frame().sequence == accepted);
        WorldState state{5, 2, 3, .1, {motion_values(after.objects[0]), motion_values(after.objects[7])}};
        state.objects[0].pose.position.y = 1.001f;
        state.objects[0].collision.dimensions.y = 1;
        state.objects[1].pose.position.x = 50; // Translation keeps the authored shape, no double offset.
        auto invalid = state;
        invalid.objects[1].collision.shape_origin.x += 1;
        CHECK(receiver.receive(invalid, error) == NetworkWorldReceiver::StateResult::Refused);
        CHECK(receiver.world().frame().sequence == accepted && collision.frame().sequence == accepted);
        CHECK(receiver.world().get(player.id)->pose.position.y == 2.001f &&
              collision.get(player.id)->dimensions.y == 2);
        CHECK(receiver.receive(state, error) == NetworkWorldReceiver::StateResult::Applied);
        CHECK(collision.get(wall.id)->shape == old_shape);
        CHECK(collision.intersect({0, 10, 0}, {100, 10, 0}, {5, player.id}, hit) == SegmentResult::Hit &&
              hit.point.x == 50);
        CHECK(collision.get(player.id)->dimensions.y == 1);
        auto rotated = snapshot_values(receiver.world().frame());
        rotated.sequence = 4;
        rotated.time = .15;
        rotated.objects[7].pose.rotation = rotation_from_euler({0, .4f, 0});
        rotated.objects[7].collision.shape_transformed = true;
        rotated.objects[7].collision.motion.shape_origin = {50, 0, 0};
        CHECK(make_world_patch(after, rotated, patch, changed) && changed);
        CHECK(receiver.receive(patch, geometry, models, sprites, error));
        ContainerShape reference;
        CHECK(reference.build(geometry.models[2], {}, error, true));
        CHECK(reference.transform(rotated.objects[7].pose.rotation, {50, 0, 0}));
        const auto shape = collision.get(wall.id)->shape;
        CHECK(shape != old_shape && shape->triangles.size() == reference.triangles.size());
        for (std::size_t i = 0; i < shape->triangles.size(); ++i)
            for (unsigned j = 0; j < 3; ++j)
            {
                const auto a = shape->triangles[i][j], b = reference.triangles[i][j];
                CHECK(a.x == b.x && a.y == b.y && a.z == b.z);
            }
        const auto retained = collision;
        geometry = {}; // State updates and retained collision own their geometry.
        state.membership = 4;
        state.sequence = 5;
        state.time = .2;
        state.objects = {motion_values(rotated.objects[7])};
        state.objects[0].pose.position.x += 5;
        CHECK(receiver.receive(state, error) == NetworkWorldReceiver::StateResult::Applied);
        CHECK(receiver.begin(6) && !receiver.collision().get(wall.id));
        CHECK(retained.intersect({0, 10, 0}, {100, 10, 0}, {5, player.id}, hit) == SegmentResult::Hit &&
              hit.object == wall.id);
        std::cout << "Independent DAT collision, 10000 physics codec mutations, volumes/stance/movers, atomic binding "
                     "and retained lifetime\n";
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
