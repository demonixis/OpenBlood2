#include "../brush_fixture.h"
#include "client/network_input.h"
#include "client/network_movement.h"
#include "game/original/MoveMgr.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <type_traits>
using namespace OpenBlood2;
using Reception = NetworkMovement::Reception;
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
        NetworkMovement client{collision};
        Net::MovementSetup initial;
        std::string error;
        ClientObjectId owner{7, {0, 1}};
        // Predicted bodies at each report: an agreeing server acknowledges the
        // resolved state, while reports carry CMoveMgr's pre-collision velocity.
        std::map<std::uint64_t, ClientMotionState> reported;
        bool step(std::optional<Net::MovementReport>& report, MovementInput input = {})
        {
            if (!client.update(input, 0, -.4f, 1. / 60, true, false, false, report, error))
                return false;
            if (report)
                reported[report->sequence] = client.movement().body;
            return true;
        }
        Net::MovementAck ack(const Net::MovementReport& report) const
        {
            auto accepted = report.move;
            if (const auto found = reported.find(report.sequence); found != reported.end())
            {
                accepted.velocity = found->second.velocity;
                accepted.acceleration = found->second.acceleration;
                accepted.on_ground = found->second.standing && found->second.ground_normal.y >= .5f;
            }
            return {report.owner,
                    report.sequence,
                    client.setup().sequence,
                    accepted,
                    client.movement().body.dimensions,
                    report.impulse_sequence};
        }
        bool box(DVector position, DVector dimensions)
        {
            auto frame = collision.frame();
            ++frame.sequence;
            ClientCollider obstacle;
            obstacle.id = {10, 1};
            obstacle.position = position;
            obstacle.dimensions = dimensions;
            obstacle.flags = FLAG_SOLID;
            frame.objects.push_back(obstacle);
            return collision.apply(std::move(frame), error);
        }
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
            initial = {{7, 1, owner.object, 300},
                       1,
                       44,
                       0,
                       false,
                       true,
                       34,
                       player.position,
                       player.dimensions,
                       {0, -1000, 0},
                       280,
                       600};
            CHECK(client.begin({7, 1, owner.object}));
            CHECK(client.receive(initial, error) == Reception::Applied);
            return true;
        }
    };
    bool same(DVector a, DVector b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    bool equal(const NetworkMovement& a, const NetworkMovement& b)
    {
        const auto& x = a.movement().body;
        const auto& y = b.movement().body;
        return same(x.position, y.position) && same(x.velocity, y.velocity) && same(x.acceleration, y.acceleration) &&
               a.manager().substeps == b.manager().substeps && a.manager().updates == b.manager().updates &&
               a.manager().jumps == b.manager().jumps && a.manager().landings == b.manager().landings;
    }
    bool immediate()
    {
        Fixture f;
        CHECK(f.setup());
        MovementInput input;
        input.forward = true;
        std::optional<Net::MovementReport> report;
        unsigned count = 0;
        for (unsigned i = 0; i < 60; ++i)
        {
            CHECK(f.step(report, input));
            if (report)
            {
                const auto before = f.client.movement().body;
                CHECK(f.client.receive(f.ack(*report), f.error) == Reception::Applied);
                CHECK(same(before.position, f.client.movement().body.position));
                CHECK(same(before.velocity, f.client.movement().body.velocity));
                ++count;
            }
        }
        CHECK(count >= 29 && count <= 31 && f.client.manager().substeps == 120 &&
              f.client.movement().predictions == 120);
        CHECK(f.client.movement().acknowledgements == count && f.client.corrections == count && !f.client.replays);
        CHECK(f.client.movement().body.position.z > 250 && f.collision.get(f.owner.object)->position.z == 0);
        return true;
    }
    bool delayed(ContainerCode code)
    {
        Fixture a, b, c;
        CHECK(a.setup(code) && b.setup(code) && c.setup(code));
        MovementInput input;
        input.forward = true;
        std::optional<Net::MovementReport> ra, rb, rc;
        CHECK(a.step(ra, input) && b.step(rb, input) && c.step(rc, input));
        CHECK(ra && rb && rc);
        auto first = a.ack(*ra);
        first.accepted.position.z -= .2f;
        first.accepted.velocity.x += 30;
        CHECK(b.client.receive(first, b.error) == Reception::Applied);
        unsigned frames = 1;
        do
        {
            CHECK(frames < 10 && a.step(ra, input) && b.step(rb, input) && c.step(rc, input));
            ++frames;
        } while (!ra);
        CHECK(ra->sequence == 2);
        const auto second = a.ack(*ra);
        CHECK(c.client.receive(second, c.error) == Reception::Applied);
        CHECK(a.client.receive(first, a.error) == Reception::Applied);
        CHECK(equal(a.client, b.client));
        CHECK(a.client.replays == frames - 1 && a.client.manager().substeps == frames * 2);
        for (int i = 0; i < 10; ++i)
            CHECK(a.step(ra, input) && c.step(rc, input));
        CHECK(a.client.receive(second, a.error) == Reception::Applied);
        CHECK(equal(a.client, c.client));
        CHECK(a.client.replays == frames - 1 + 10 && a.client.movement().acknowledgements == 2);
        CHECK(a.client.receive(first, a.error) == Reception::Ignored);
        CHECK(a.client.receive(second, a.error) == Reception::Ignored);
        auto invalid = second;
        invalid.sequence = 10000;
        CHECK(a.client.receive(invalid, a.error) == Reception::Refused && equal(a.client, c.client));
        return true;
    }
    bool forces_and_stances()
    {
        Fixture f;
        CHECK(f.setup());
        std::optional<Net::MovementReport> report;
        CHECK(f.step(report) && report);
        const auto old = f.ack(*report);
        auto setup = f.initial;
        ++setup.sequence;
        ++setup.owner.force_revision;
        ++setup.code;
        setup.position.z = 100;
        CHECK(f.client.receive(setup, f.error) == Reception::Applied);
        CHECK(f.client.manager().updates == 1 && f.client.manager().substeps == 2);
        CHECK(same(f.client.movement().body.position, setup.position) && same(f.client.movement().body.velocity, {}));
        CHECK(f.client.receive(old, f.error) == Reception::Ignored);
        CHECK(f.step(report) && report && report->sequence == 2 && report->move.move_code == setup.code);
        ++setup.sequence;
        setup.owner.force_revision += 256; // Byte-code wrap cannot revive old reports.
        CHECK(f.client.receive(setup, f.error) == Reception::Applied);
        CHECK(f.client.receive(old, f.error) == Reception::Ignored);
        auto invalid = setup;
        ++invalid.sequence;
        ++invalid.code;
        CHECK(f.client.receive(invalid, f.error) == Reception::Refused);
        invalid = setup;
        invalid.position.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(f.client.receive(invalid, f.error) == Reception::Refused);
        const auto foot = f.client.movement().body.position.y - 41;
        ++setup.sequence;
        setup.dimensions.y = 20;
        setup.eye_level = 12;
        setup.crouched = true;
        CHECK(f.client.receive(setup, f.error) == Reception::Applied);
        CHECK(std::abs(f.client.movement().body.position.y - 20 - foot) < .0001f);
        CHECK(f.box({0, 60, 100}, {30, 10, 30}));
        ++setup.sequence;
        setup.dimensions.y = 41;
        setup.eye_level = 34;
        setup.crouched = false;
        CHECK(f.client.receive(setup, f.error) == Reception::Applied);
        CHECK(f.client.movement().body.dimensions.y == 20); // Growth would enter the ceiling.
        CHECK(f.step(report) && report);
        auto ack = f.ack(*report);
        ack.accepted.position = {100, 41.001f, 100};
        ack.dimensions.y = 41;
        CHECK(f.client.receive(ack, f.error) == Reception::Applied);
        CHECK(f.client.movement().body.dimensions.y == 41 &&
              same(f.client.movement().body.position, ack.accepted.position));
        return true;
    }
    bool failures_and_ordering()
    {
        Fixture f;
        CHECK(f.setup());
        MovementInput input;
        input.forward = true;
        std::optional<Net::MovementReport> report;
        CHECK(f.step(report, input) && report);
        auto ack = f.ack(*report);
        CHECK(f.step(report, input));
        const auto before = f.client.movement().body;
        const auto steps = f.client.manager().substeps;
        ack.accepted.position.x = 100;
        CHECK(f.box({100, 41, ack.accepted.position.z}, {10, 30, 10}));
        CHECK(f.client.receive(ack, f.error) == Reception::Deferred);
        CHECK(same(before.position, f.client.movement().body.position) && f.client.manager().substeps == steps);
        CHECK(!f.client.corrections && !f.client.movement().acknowledgements);
        auto frame = f.collision.frame();
        ++frame.sequence;
        frame.objects.pop_back();
        CHECK(f.collision.apply(std::move(frame), f.error));
        CHECK(f.client.receive(ack, f.error) == Reception::Applied && f.client.replays == 1);
        // Reliable setup and unreliable ACK may cross. Identical constraints
        // keep useful report history; changing constraints retires it.
        for (int i = 2; i < 10 || !report; ++i)
            CHECK(i < 20 && f.step(report, input));
        ack = f.ack(*report);
        ++ack.setup_sequence;
        CHECK(f.client.receive(ack, f.error) == Reception::Deferred);
        auto setup = f.client.setup();
        ++setup.sequence;
        CHECK(f.client.receive(setup, f.error) == Reception::Applied);
        CHECK(f.step(report, input) && f.client.corrections == 2);
        const auto position = f.client.movement().body.position;
        const auto updates = f.client.manager().updates;
        input.mouse_forward = std::numeric_limits<float>::infinity();
        CHECK(!f.step(report, input) && !report && same(position, f.client.movement().body.position));
        CHECK(f.client.manager().updates == updates);
        // A partially attempted collision step is rolled back and the body held:
        // the next acknowledgement or force corrects a briefly disagreeing
        // replica, while a failure that persists still refuses prediction.
        CHECK(f.box(position, {10, 30, 10}));
        for (unsigned i = 0; i < NetworkMovement::hold_limit; ++i)
            CHECK(f.step(report, {}) && !report && same(position, f.client.movement().body.position));
        CHECK(f.client.prediction_holds == NetworkMovement::hold_limit && f.client.manager().updates == updates);
        CHECK(!f.step(report, {}) && !report && same(position, f.client.movement().body.position));
        CHECK(f.client.manager().updates == updates);
        return true;
    }
    bool immediate_obstruction()
    {
        Fixture f;
        CHECK(f.setup());
        std::optional<Net::MovementReport> report;
        CHECK(f.step(report) && report);
        auto ack = f.ack(*report);
        ack.accepted.position.x = 100;
        CHECK(f.box(ack.accepted.position, {10, 30, 10}));
        CHECK(f.client.receive(ack, f.error) == Reception::Deferred);
        CHECK(f.client.movement().body.position.x == 0 && !f.client.corrections);
        auto frame = f.collision.frame();
        ++frame.sequence;
        frame.objects.pop_back();
        CHECK(f.collision.apply(std::move(frame), f.error));
        CHECK(f.client.receive(ack, f.error) == Reception::Applied && !f.client.replays);
        return true;
    }
    bool render_frame_input()
    {
        Fixture sixty, fast, catchup;
        CHECK(sixty.setup() && fast.setup() && catchup.setup());
        NetworkInput a, b, c;
        const auto frames = [](Fixture& f, NetworkInput& clock, unsigned count, double seconds, float offset)
        {
            for (unsigned frame = 0; frame < count; ++frame)
            {
                MovementInput input;
                input.mouse_move = true;
                input.mouse_forward = offset;
                input.mouse_strafe = offset * .5f;
                MovementInput sample;
                unsigned ticks = 0;
                CHECK(clock.advance(seconds, input, true, ticks, sample));
                for (unsigned tick = 0; tick < ticks; ++tick)
                {
                    std::optional<Net::MovementReport> report;
                    CHECK(f.step(report, sample));
                }
            }
            return true;
        };
        CHECK(frames(sixty, a, 60, 1. / 60, .25f));
        CHECK(frames(fast, b, 240, 1. / 240, .0625f));
        CHECK(frames(catchup, c, 20, 1. / 20, .75f));
        CHECK(equal(sixty.client, fast.client) && equal(sixty.client, catchup.client));
        CHECK(sixty.client.manager().substeps == 120 && sixty.client.movement().predictions == 120);
        CHECK(sixty.client.movement().body.position.z > 1);
        NetworkInput gate;
        MovementInput input, sample;
        input.mouse_move = true;
        input.mouse_forward = 4;
        unsigned ticks = 99;
        CHECK(gate.advance(1. / 240, input, true, ticks, sample) && !ticks);
        CHECK(gate.advance(1. / 240, {}, false, ticks, sample) && !ticks);
        CHECK(gate.advance(1. / 120, {}, true, ticks, sample) && ticks == 1 && !sample.mouse_forward);
        CHECK(gate.advance(1. / 240, input, true, ticks, sample));
        CHECK(gate.advance(1. / 80, {}, true, ticks, sample) && ticks == 1 && !sample.mouse_forward);
        input.mouse_forward = std::numeric_limits<float>::quiet_NaN();
        CHECK(!gate.advance(1, input, true, ticks, sample));
        input.mouse_forward = 60;
        CHECK(gate.advance(1, input, true, ticks, sample) && ticks == 6 && sample.mouse_forward == 10);
        CHECK(gate.discarded_ticks() == 54);
        CHECK(gate.advance(1. / 60, {}, true, ticks, sample) && ticks == 1 && !sample.mouse_forward);
        return true;
    }
    bool impulses()
    {
        Fixture a, b;
        CHECK(a.setup() && b.setup());
        std::optional<Net::MovementReport> ra, rb;
        CHECK(a.step(ra) && b.step(rb) && ra && rb);
        const auto old = a.ack(*ra);
        auto future = old;
        future.impulse_sequence = 1;
        future.accepted.velocity = {100, 300, 0};
        CHECK(a.client.receive(future, a.error) == Reception::Deferred);
        const auto before = a.client.movement().body;
        const auto updates = a.client.manager().updates, substeps = a.client.manager().substeps;
        Net::MovementImpulse impulse{a.client.setup().owner, 1, {100, 300, 0}};
        CHECK(a.client.receive(impulse, a.error) == Reception::Applied);
        CHECK(b.client.receive(impulse, b.error) == Reception::Applied);
        CHECK(same(before.position, a.client.movement().body.position));
        CHECK(same(vector_add(before.velocity, impulse.velocity), a.client.movement().body.velocity));
        CHECK(a.client.manager().updates == updates && a.client.manager().substeps == substeps);
        CHECK(a.client.receive(impulse, a.error) == Reception::Ignored);
        // History is kept: an ACK whose server state predates the impulse applies,
        // and the impulse is re-applied at the frame it was received.
        const auto expected = a.client.movement().body.velocity;
        CHECK(a.client.receive(old, a.error) == Reception::Applied);
        CHECK(b.client.receive(b.ack(*rb), b.error) == Reception::Applied);
        CHECK(same(vector_add(old.accepted.velocity, impulse.velocity), a.client.movement().body.velocity));
        CHECK(same(expected, a.client.movement().body.velocity));
        CHECK(a.client.receive(future, a.error) == Reception::Ignored);
        auto invalid = impulse;
        invalid.sequence = 3;
        CHECK(a.client.receive(invalid, a.error) == Reception::Refused);
        invalid = impulse;
        invalid.velocity.x = std::numeric_limits<float>::infinity();
        CHECK(a.client.receive(invalid, a.error) == Reception::Refused);
        invalid = impulse;
        invalid.owner.object.generation++;
        CHECK(a.client.receive(invalid, a.error) == Reception::Ignored);
        // A second source delta adds once, without moving or advancing the clock.
        impulse.sequence = 2;
        impulse.velocity = {-20, 50, 0};
        CHECK(a.client.receive(impulse, a.error) == Reception::Applied);
        CHECK(b.client.receive(impulse, b.error) == Reception::Applied);
        CHECK(a.client.impulse_sequence() == 2 && equal(a.client, b.client));
        // Reports follow the 1/30 s cadence and acknowledge the latest impulse.
        ra.reset();
        for (unsigned i = 0; i < 9 && !ra; ++i)
            CHECK(a.step(ra) && b.step(rb) && bool(ra) == bool(rb));
        CHECK(ra && ra->sequence == 2 && ra->impulse_sequence == 2);
        CHECK(a.client.movement().body.position.y > before.position.y);
        auto correction = a.ack(*ra);
        correction.accepted.position.z += .1f;
        CHECK(b.client.receive(correction, b.error) == Reception::Applied);
        for (unsigned i = 0; i < 4; ++i)
            CHECK(a.step(ra) && b.step(rb));
        CHECK(a.client.receive(correction, a.error) == Reception::Applied && equal(a.client, b.client));
        // Seven frames of two substeps; replayed frames are not counted again.
        CHECK(a.client.replays == 4 && a.client.manager().substeps == 14 &&
              b.client.manager().substeps == a.client.manager().substeps);
        // Dead/slow-death movement gates neutralize controls, not existing momentum.
        auto setup = a.client.setup();
        ++setup.sequence;
        setup.can_move = false;
        CHECK(a.client.receive(setup, a.error) == Reception::Applied);
        CHECK(b.client.receive(setup, b.error) == Reception::Applied);
        MovementInput held;
        held.forward = held.jump = true;
        CHECK(a.step(ra, held) && b.step(rb) && equal(a.client, b.client));
        // Reliable impulse can arrive before the collider/setup is admitted.
        ++setup.sequence;
        ++setup.owner.force_revision;
        ++setup.code;
        impulse.owner = setup.owner;
        impulse.sequence = 1;
        CHECK(a.client.receive(impulse, a.error) == Reception::Deferred);
        CHECK(a.client.receive(setup, a.error) == Reception::Applied);
        CHECK(a.client.impulse_sequence() == 0 && same(a.client.movement().body.velocity, {}));
        CHECK(a.client.receive(impulse, a.error) == Reception::Applied);
        CHECK(a.client.receive(correction, a.error) == Reception::Ignored);
        --impulse.owner.force_revision;
        CHECK(a.client.receive(impulse, a.error) == Reception::Ignored);
        std::cout
            << "Reliable impulses: additive/deduplicated, receipt ordering, replay, report clocks and lives passed\n";
        return true;
    }
    bool bounded_history()
    {
        Fixture f;
        CHECK(f.setup());
        std::optional<Net::MovementReport> report;
        CHECK(f.step(report) && report);
        const auto old = f.ack(*report);
        for (int i = 1; i < 600; ++i)
            CHECK(f.step(report));
        CHECK(f.client.receive(old, f.error) == Reception::Ignored);
        auto frame = f.collision.frame();
        ++frame.sequence;
        frame.objects.clear();
        CHECK(f.collision.apply(std::move(frame), f.error));
        CHECK(!f.client.ready() && !f.step(report));
        auto setup = f.initial;
        ++setup.sequence;
        CHECK(f.client.receive(setup, f.error) == Reception::Deferred);
        setup.owner.world = 6;
        CHECK(f.client.receive(setup, f.error) == Reception::Ignored);
        return true;
    }
} // namespace
int main()
{
    if (!immediate() || !forces_and_stances() || !failures_and_ordering() || !immediate_obstruction() ||
        !bounded_history() || !render_frame_input() || !impulses())
        return 1;
    for (auto code : {CC_NOTHING, CC_WATER, CC_LADDER, CC_CONVEYOR})
        if (!delayed(code))
            return 1;
    std::cout << "Independent CMoveMgr: immediate/delayed ACKs, four environments, force/stance, rollback and bounds "
                 "passed\n";
}
