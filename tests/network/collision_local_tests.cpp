#include "../brush_fixture.h"
#include "../player_fixture.h"
#include "engine/world_model.h"
#include <iostream>
#include <random>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool same(DVector a, DVector b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    bool near(DVector a, DVector b)
    {
        return vector_dot(vector_subtract(a, b), vector_subtract(a, b)) < 1e-8f;
    }
    bool enter(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error) && g.step(1. / 60));
        return true;
    }
    HOBJECT box(GameSession& g, DVector p, DVector d)
    {
        ObjectCreateStruct state;
        state.m_Pos = p;
        state.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
        return h && g.server.SetObjectDims(h, &d) == DE_OK ? h : nullptr;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        ClientCollisionWorld detached;
        ClientObjectId owner;
        ClientMotionState initial, predicted;
        {
            GameSession g;
            CHECK(enter(g, root));
            auto h = box(g, {32, 100, 120}, {100, 100, 1});
            CHECK(h);
            CHECK(g.local_movement.publish_collision());
            owner = g.client_session.movement.owner;
            initial = g.client_session.movement.body;
            detached = g.client_session.collision;
            const auto* record = g.objects.get(h);
            const auto id = record->id;
            CHECK(detached.frame().primary.get() != &g.collision);
            const auto copies = g.local_movement.primary_copies;
            CHECK(g.local_movement.publish_collision() && copies == g.local_movement.primary_copies);
            const auto work = g.collision.work();
            const DVector destination{initial.position.x, initial.position.y, 200};
            CHECK(detached.predict(owner, initial, destination, {0, 0, 100}, {}, predicted));
            CHECK(predicted.position.z < 120 && predicted.position.z > initial.position.z);
            CHECK(g.collision.work().triangle_tests == work.triangle_tests &&
                  g.collision.work().node_tests == work.node_tests);
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
            CHECK(g.local_movement.publish_collision() && !g.client_session.collision.get(id));
            CHECK(detached.get(id));
            ClientMotionState clear;
            CHECK(g.client_session.collision.predict(owner, initial, destination, {0, 0, 100}, {}, clear));
            CHECK(clear.position.z == 200);
            // Same address and world, different geometry: revision invalidates the copy.
            const auto revision = g.collision.revision();
            auto floor = test_brush::box("new floor", {-1000, -20, -1000}, {1000, -10, 1000});
            for (auto& s : floor.surfaces)
                s.flags = 1;
            std::string error;
            CHECK(g.collision.build(floor, error));
            CHECK(g.collision.revision() != revision && g.local_movement.publish_collision());
            CHECK(g.local_movement.primary_copies == copies + 1);
            CHECK(detached.frame().primary->revision() == revision);
            CHECK(g.unload_world());
            CHECK(!g.client_session.collision.frame().primary && g.client_session.collision.frame().objects.empty());
        }
        // Server globals and original objects have been destroyed; the replica still collides.
        CHECK(detached.predict(owner, initial, {initial.position.x, initial.position.y, 200}, {0, 0, 100}, {},
                               predicted));
        CHECK(predicted.position.z < 120 && predicted.position.z > initial.position.z);
        return true;
    }
    bool geometry(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        const auto start = g.client_session.movement.body.position;
        auto mesh = test_brush::box("obstacle", {start.x - 30, 0, start.z + 45}, {start.x + 30, 100, start.z + 55});
        mesh.translation = {start.x, 50, start.z + 50};
        g.world.models.push_back(mesh);
        auto h = g.objects.create(g.objects.find_class("CDestructableBrush"), {}, Creation::World,
                                  {{"Name", std::string("obstacle")}});
        CHECK(h);
        auto* record = g.objects.get(h);
        CHECK(record->world_model && g.local_movement.publish_collision());
        const auto id = record->id;
        const auto* copy = g.client_session.collision.get(id);
        CHECK(copy && copy->shape.get() != record->world_model.get());
        const auto frozen = g.client_session.collision;
        const auto copies = g.local_movement.shape_copies;
        CHECK(g.local_movement.publish_collision() && g.local_movement.shape_copies == copies);
        for (unsigned turn = 0; turn < 4; ++turn)
        {
            auto shape = std::make_shared<ContainerShape>(*record->world_model_base);
            CHECK(shape->transform(rotation_from_euler({0, float(turn) * .5f, 0}), mesh.translation));
            record->world_model = shape;
            record->state.m_Pos = vector_add(mesh.translation, {float(turn) * 8, 0, 0});
            CHECK(g.local_movement.publish_collision());
            CHECK(g.local_movement.shape_copies == copies + turn + 1);
            for (float x : {-40.f, -10.f, 0.f, 10.f, 40.f})
            {
                const DVector from{start.x + x, 50, start.z}, to{start.x + x, 50, start.z + 100}, half{1, 1, 1};
                CollisionHit actual;
                ClientCollisionHit replica;
                const auto a = sweep_object(*record, from, to, half, actual);
                const auto b =
                    g.client_session.collision.sweep(g.client_session.movement.owner, from, to, half, replica);
                CHECK(a == b);
                if (a == CollisionQuery::Hit)
                    CHECK(replica.object == id && replica.fraction == actual.fraction &&
                          same(replica.normal, actual.normal));
            }
        }
        CHECK(frozen.get(id)->shape.get() != record->world_model.get());
        return true;
    }
    bool private_worlds(const std::filesystem::path& root)
    {
        for (const auto* world : {"01_SubwayA", "02_CityHub"})
        {
            GameSession game;
            CHECK(game.initialize());
            Config config;
            config.data_dir = root;
            std::string error;
            CHECK(game.resources.mount_game(config, error));
            CHECK(game.request_world(world, error) && game.enter_player(error));
            for (unsigned i = 0; i < 900; ++i)
            {
                // Preserve the intro, then exercise ordinary command/stance changes.
                auto& input = game.client_shell.movement_input;
                input = {};
                input.forward = i >= 780 && i < 840;
                input.crouch = i >= 810 && i < 850;
                input.jump = i >= 860 && i < 865;
                CHECK(game.step(1. / 60));
                CHECK(game.local_movement.collision_error.empty());
            }
            CHECK(game.client_session.collision.frame().sequence > 900);
            CHECK(game.local_movement.primary_copies == 1);
            CHECK(game.client_session.movement.predictions > 900);
            CHECK(game.client_session.movement.prediction_failures == 0);
            CHECK(game.client_session.movement.predictions == game.client_session.movement.acknowledgements);
        }
        return true;
    }
    bool parity(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        CHECK(box(g, {32, 8, 110}, {80, 8, 20}));
        CHECK(box(g, {-50, 70, 60}, {5, 70, 150}));
        CHECK(box(g, {120, 70, 60}, {5, 70, 150}));
        auto* player = g.objects.get(g.client_shell.player);
        const auto start = player->state.m_Pos;
        std::mt19937 rng(5514);
        for (unsigned i = 0; i < 400; ++i)
        {
            player->state.m_Pos = start;
            player->velocity = {};
            player->acceleration = {};
            CHECK(g.local_movement.synchronize() && g.local_movement.publish_collision());
            const auto state = g.client_session.movement.body;
            const DVector delta{float(int(rng() % 241) - 120), 0, float(int(rng() % 241) - 120)};
            auto move = g.client_shell.move_mgr.WritePositionInfo(vector_add(start, delta), delta);
            ClientMotionState predicted;
            const bool prediction = g.client_session.collision.predict(
                g.client_session.movement.owner, state, move.position, move.velocity, move.acceleration, predicted);
            const bool accepted = g.server_shell.OnMessage(g.local_client, move);
            CHECK(prediction == accepted);
            if (accepted)
            {
                CHECK(near(predicted.position, player->state.m_Pos) && near(predicted.velocity, player->velocity));
                MovementState authoritative;
                CHECK(g.server.GetMovementState(player->handle, authoritative));
                CHECK(predicted.standing == authoritative.standing &&
                      same(predicted.ground_normal, authoritative.ground_normal));
                CHECK(predicted.stair_steps == authoritative.stair_steps &&
                      predicted.overlap_contacts == authoritative.overlap_contacts);
            }
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    const auto root = std::filesystem::path(argv[2]);
    return (name == "lifetime"   ? lifetime(root)
            : name == "geometry" ? geometry(root)
            : name == "private"  ? private_worlds(root)
                                 : name == "parity" && parity(root))
               ? 0
               : 1;
}
