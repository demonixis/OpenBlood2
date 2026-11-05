#include "../brush_fixture.h"
#include "client/replica_movement.h"
#include "game/original/MoveMgr.h"
#include <cmath>
#include <iostream>
#include <limits>
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
    struct Fixture
    {
        ClientCollisionWorld collision;
        ReplicaMovement movement{collision};
        CMoveMgr mgr{movement};
        ClientObjectId owner{7, {0, 1}};
        bool setup(ContainerCode code = CC_NOTHING)
        {
            auto geometry = std::make_shared<CollisionWorld>();
            auto floor = test_brush::box("floor", {-3000, -20, -3000}, {3000, 0, 3000});
            for (auto& surface : floor.surfaces)
                surface.flags = 1;
            std::string error;
            CHECK(geometry->build(floor, error));
            CHECK(collision.begin(owner.world));
            ClientCollisionFrame frame;
            frame.world = owner.world;
            frame.sequence = 1;
            frame.primary = geometry;
            ClientCollider player;
            player.id = owner.object;
            player.type = OT_MODEL;
            player.position = {0, 41.001f, 0};
            player.dimensions = {16, 41, 16};
            player.flags = FLAG_SOLID | FLAG_STAIRSTEP;
            frame.objects.push_back(player);
            if (code != CC_NOTHING)
            {
                ClientCollider volume;
                volume.id = {1, 1};
                volume.type = OT_CONTAINER;
                volume.container = volume.volume_brush = true;
                auto shape = std::make_shared<ContainerShape>();
                CHECK(shape->build(test_brush::box("volume", {-2000, 0, -2000}, {2000, 1000, 2000}), {}, error));
                volume.shape = shape;
                volume.position = shape->origin;
                volume.dimensions = {2000, 500, 2000};
                volume.environment =
                    ContainerEnvironment{code, code == CC_CONVEYOR ? DVector{100, 0, 0} : DVector{}, -200, 0, false};
                frame.objects.push_back(volume);
            }
            CHECK(collision.apply(std::move(frame), error));
            CHECK(movement.bind(owner, 300, 44, 0, 34, false));
            CHECK(mgr.OnPhysicsUpdate({{}, {}, player.dimensions, {0, -1000, 0}, 280, 600}));
            CHECK(mgr.OnServerForcePos({player.position, 44}));
            return true;
        }
    };
    bool walking()
    {
        Fixture a, b;
        CHECK(a.setup() && b.setup());
        MovementInput input;
        input.forward = true;
        std::optional<ReplicaMoveReport> report;
        unsigned reports = 0;
        std::size_t steps = 0;
        float seconds = 0;
        DVector origin = a.mgr.body.position;
        for (int i = 0; i < 60; ++i)
        {
            CHECK(a.mgr.Update(false, input, 0, 1. / 60, true));
            CHECK(a.movement.report(1. / 60, report));
            if (report)
            {
                ++reports;
                CHECK(report->owner == a.owner && report->force_revision == 300 && report->sequence == reports);
                CHECK(report->move.move_code == 44 && report->move.position.z == a.mgr.body.position.z);
                CHECK(report->controls.movement.forward);
                // Reports chain: each starts where the previous prediction ended,
                // and its unclipped horizontal offsets reach the reported position.
                CHECK(report->origin.z == origin.z && !report->steps.empty());
                float z = report->origin.z;
                for (const auto& step : report->steps)
                {
                    z += step.offset.z;
                    seconds += step.seconds;
                    CHECK(step.on_ground && step.velocity.z > 0);
                }
                CHECK(std::abs(z - report->move.position.z) < 1e-3f);
                steps += report->steps.size();
                origin = report->move.position;
            }
        }
        CHECK(a.movement.pending_steps() == 120 - steps);
        for (int i = 0; i < 30; ++i)
            CHECK(b.mgr.Update(false, input, 0, 1. / 30, true) && b.movement.report(1. / 30, report));
        CHECK(a.mgr.substeps == 120 && b.mgr.substeps == 120 && reports >= 29 && reports <= 31);
        CHECK(steps + a.movement.pending_steps() == 120 && std::abs(seconds - steps / 120.f) < 1e-4f);
        CHECK(a.mgr.body.position.z > 250 && a.mgr.body.position.z < 280);
        CHECK(a.mgr.body.velocity.z == b.mgr.body.velocity.z && a.mgr.body.position.z == b.mgr.body.position.z);
        CHECK(a.collision.get(a.owner.object)->position.z == 0); // Prediction never edits authoritative poses.
        CHECK(a.movement.state.acknowledgements == 0 && a.movement.state.predictions == 120);
        auto older = a.collision.frame();
        ++older.sequence;
        older.objects[0].position.z = 20;
        std::string error;
        CHECK(a.collision.apply(std::move(older), error));
        const auto predicted = a.mgr.body.position;
        CHECK(a.movement.synchronize() && a.mgr.body.position.z == predicted.z);
        input.jump = true;
        CHECK(a.mgr.Update(false, input, 0, 1. / 60, true) && a.movement.report(1. / 60, report));
        CHECK(a.mgr.jumps == 1 && a.mgr.body.velocity.y > 500);
        input = {};
        bool airborne = false;
        for (int i = 0; i < 100; ++i)
        {
            CHECK(a.mgr.Update(false, input, 0, 1. / 60, true) && a.movement.report(1. / 60, report));
            if (report)
                for (const auto& step : report->steps)
                    airborne = airborne || !step.on_ground;
        }
        CHECK(airborne && a.mgr.landings == 1 && a.mgr.m_bOnGround && std::abs(a.mgr.body.position.y - 41.001f) < .01f);
        CHECK(a.mgr.OnServerForcePos({{0, 41.001f, 0}, 44}));
        CHECK(a.mgr.body.position.z == 0 && a.mgr.body.velocity.y == 0 && !a.movement.pending_steps());
        CHECK(a.mgr.Update(false, {}, 0, 2, false));
        CHECK(a.mgr.discarded_time == 1.9);
        CHECK(a.movement.report(2, report) && report && !report->controls.movement.forward);
        CHECK(report->steps.size() == 12 && report->origin.z == 0);
        CHECK(a.movement.report(0, report) && !report); // No burst after a long frame.
        return true;
    }
    bool environment()
    {
        for (auto code : {CC_WATER, CC_LADDER, CC_CONVEYOR})
        {
            Fixture f;
            CHECK(f.setup(code));
            MovementInput input;
            input.forward = input.run = true;
            std::optional<ReplicaMoveReport> report;
            for (int i = 0; i < 60; ++i)
                CHECK(f.mgr.Update(false, input, 0, 1. / 60, true, false, -.4f) && f.movement.report(1. / 60, report));
            CHECK(f.mgr.containers.count == 1 && f.mgr.containers.head.code == code);
            CHECK(f.mgr.body.position.z > 100);
            if (code == CC_WATER)
                CHECK(f.mgr.m_bBodyInLiquid && !f.mgr.m_bUseGravity);
            if (code == CC_LADDER)
                CHECK(f.mgr.m_bBodyOnLadder && !f.mgr.m_bUseGravity && f.mgr.body.position.y > 100);
            if (code == CC_CONVEYOR)
                CHECK(f.mgr.m_bBodyOnConveyor && f.mgr.m_bUseGravity && f.mgr.body.position.x > 0);
        }
        return true;
    }
    bool rejection_and_collision()
    {
        Fixture f;
        CHECK(f.setup());
        auto frame = f.collision.frame();
        ++frame.sequence;
        ClientCollider wall;
        wall.id = {1, 1};
        wall.position = {0, 100, 80};
        wall.dimensions = {100, 100, 1};
        wall.flags = FLAG_SOLID;
        frame.objects.push_back(wall);
        std::string error;
        CHECK(f.collision.apply(std::move(frame), error));
        MovementInput input;
        input.forward = true;
        std::optional<ReplicaMoveReport> report;
        std::uint64_t sequence = 0;
        for (int i = 0; i < 120; ++i)
        {
            CHECK(f.mgr.Update(false, input, 0, 1. / 60, true) && f.movement.report(1. / 60, report));
            if (report)
            {
                CHECK(report->sequence == ++sequence);
                // Reported velocity is CMoveMgr's, before the wall clipped it:
                // the server needs it for stopping contacts and falling damage.
                if (i == 119)
                    CHECK(report->move.velocity.z > 0 && f.mgr.body.velocity.z == 0);
            }
        }
        CHECK(f.mgr.body.position.z > 62.9f && f.mgr.body.position.z < 63.01f && f.mgr.body.velocity.z == 0);
        const auto before = f.mgr.body.position;
        auto move = f.mgr.WritePositionInfo({0, 41.001f, 100}, {0, 0, 100});
        move.move_code = 43;
        CHECK(!f.movement.advance(move, {0, 0, 1}, 1.f / 120) && f.mgr.body.position.z == before.z);
        move.move_code = 44;
        CHECK(!f.movement.advance(move, {0, 0, 1}, 0) && f.mgr.body.position.z == before.z);
        CHECK(!f.movement.advance(move, {0, 0, std::numeric_limits<float>::quiet_NaN()}, 1.f / 120) &&
              f.mgr.body.position.z == before.z);
        move.position.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(!f.movement.advance(move, {0, 0, 1}, 1.f / 120) && f.mgr.body.position.z == before.z);
        CHECK(!f.mgr.OnServerForcePos({{0, 41, 0}, 43}) && f.mgr.body.position.z == before.z);
        CHECK(!f.movement.bind(f.owner, 301, 45, 0, 34, false));
        CHECK(!f.movement.report(-1, report));
        CHECK(f.movement.report(1, report) && report && report->sequence == sequence + 1);
        frame = f.collision.frame();
        ++frame.sequence;
        frame.objects.erase(frame.objects.begin());
        CHECK(f.collision.apply(std::move(frame), error));
        CHECK(!f.mgr.ready() && !f.mgr.Update(false, {}, 0, 1. / 60, true) && !f.movement.report(.2, report));
        f.mgr.Term();
        CHECK(!f.movement.state.owner.world);
        return true;
    }
    // Two players spawned or pushed into each other: the predicted body leaves
    // the player it starts inside, and still stops at a separate one.
    bool overlapping_players()
    {
        Fixture f;
        CHECK(f.setup());
        auto frame = f.collision.frame();
        ++frame.sequence;
        ClientCollider other;
        other.id = {2, 1};
        other.type = OT_MODEL;
        other.position = {0, 41.001f, 10};
        other.dimensions = {16, 41, 16};
        other.flags = FLAG_SOLID;
        frame.objects.push_back(other);
        other.id = {3, 1};
        other.position.z = 200;
        frame.objects.push_back(other);
        std::string error;
        CHECK(f.collision.apply(std::move(frame), error));
        MovementInput input;
        input.forward = true;
        std::optional<ReplicaMoveReport> report;
        for (int i = 0; i < 120; ++i)
            CHECK(f.mgr.Update(false, input, 0, 1. / 60, true) && f.movement.report(1. / 60, report));
        CHECK(f.movement.state.prediction_failures == 0);
        CHECK(f.mgr.body.position.z > 167.9f && f.mgr.body.position.z < 168.01f);
        return true;
    }
} // namespace
int main()
{
    return walking() && environment() && rejection_and_collision() && overlapping_players() ? 0 : 1;
}
