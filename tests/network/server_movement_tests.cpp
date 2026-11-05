#include "game/game_settings.h"
#include "server/session.h"
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << "\n";                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool near(DVector a, DVector b)
    {
        return std::abs(a.x - b.x) < .001f && std::abs(a.y - b.y) < .001f && std::abs(a.z - b.z) < .001f;
    }
    // Horizontal motion may climb the spawn's small steps (the solo stair rules).
    bool flat(DVector a, DVector b)
    {
        return std::abs(a.x - b.x) < .001f && std::abs(a.z - b.z) < .001f && std::abs(a.y - b.y) < 2;
    }
    MovementReport make(const MovementSetup& setup, std::uint64_t sequence, DVector origin,
                        const std::vector<DVector>& offsets, DVector velocity = {12, 0, 0})
    {
        MovementReport report;
        report.owner = setup.owner;
        report.sequence = sequence;
        report.origin = origin;
        auto end = origin;
        for (const auto offset : offsets)
        {
            report.steps.push_back({offset, velocity, {}, 1.0f / 120, true, 0});
            end = vector_add(end, offset);
        }
        report.move = {setup.code, end, velocity, {}, true, 0};
        return report;
    }
    bool run(const char* path)
    {
        ServerSession session;
        Config config;
        config.data_dir = path;
        std::string error;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined joined;
        CHECK(session.join({}, joined, error));
        CHECK(session.tick(error)); // Original first-update initialization publishes another force position.
        MovementSetup setup;
        CHECK(session.movement_setup(joined.player, setup, error));
        const auto* object = session.game().objects.get(joined.object);
        CHECK(object && object->client_movement);
        const auto initial = object->state.m_Pos;
        auto report = make(setup, 1, initial, {{.1f, 0, 0}, {.1f, 0, 0}});
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        CHECK(near(object->state.m_Pos, initial));
        CHECK(session.tick(error));
        // Each client substep is replayed through the solo submission.
        CHECK(flat(object->state.m_Pos, vector_add(initial, {.2f, 0, 0})) && session.movement_steps == 2);
        std::optional<MovementAck> ack;
        CHECK(session.movement_ack(joined.player, ack) && ack && ack->sequence == 1);
        CHECK(near(ack->accepted.position, object->state.m_Pos));
        auto pose = object->state.m_Pos;
        for (unsigned i = 0; i < 10; ++i)
            CHECK(session.tick(error));
        CHECK(near(object->state.m_Pos, pose)); // Never extrapolated between reports.
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Ignored);
        auto malformed = report;
        malformed.owner.player++;
        CHECK(session.queue_movement(joined.player, malformed) == ServerSession::MoveResult::Invalid);
        malformed = report;
        malformed.owner.world++;
        CHECK(session.queue_movement(joined.player, malformed) == ServerSession::MoveResult::Ignored);
        malformed = report;
        malformed.owner.object.generation++;
        CHECK(session.queue_movement(joined.player, malformed) == ServerSession::MoveResult::Ignored);
        // Reports queued in one tick all apply, in order (no coalescing).
        report = make(setup, 2, pose, {{.25f, 0, 0}});
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        report = make(setup, 3, report.move.position, {{.25f, 0, 0}});
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        CHECK(session.tick(error) && flat(object->state.m_Pos, vector_add(pose, {.5f, 0, 0})));
        CHECK(session.movement_reports == 3 && session.movement_steps == 4 && session.movement_catchups == 0);
        CHECK(session.movement_ack(joined.player, ack) && ack->sequence == 3);
        // A lost report: one checked catch-up sweep to the next report's origin.
        pose = object->state.m_Pos;
        report = make(setup, 5, vector_add(pose, {1, 0, 0}), {{.25f, 0, 0}});
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error));
        CHECK(session.movement_catchups == 1 && flat(object->state.m_Pos, vector_add(pose, {1.25f, 0, 0})));
        // The speed leash refuses a teleport-sized step; the ACK corrects the client.
        pose = object->state.m_Pos;
        report = make(setup, 6, pose, {{200, 0, 0}});
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error));
        CHECK(session.movement_leashed == 1 && near(object->state.m_Pos, pose));
        CHECK(session.movement_ack(joined.player, ack) && ack->sequence == 6 && near(ack->accepted.position, pose));
        // Movement time never exceeds real time: a burst of reports beyond the
        // half-second budget is cut, and overflowing the queue drops the oldest.
        const auto steps = session.movement_steps;
        std::vector<DVector> still(24, DVector{});
        for (std::uint64_t i = 0; i < ServerSession::movement_queue_limit + 1; ++i)
            CHECK(session.queue_movement(joined.player, make(setup, 7 + i, pose, still, {})) ==
                  ServerSession::MoveResult::Queued);
        CHECK(session.movement_dropped == 1 && session.tick(error));
        CHECK(session.movement_steps - steps <= 63 && session.movement_steps - steps >= 50);
        CHECK(session.movement_leashed > 1 && session.movement_ack(joined.player, ack) && ack->sequence == 15);
        // Reports stop: the body is held, not extrapolated.
        report = make(setup, 16, pose, {{0, 0, 0}});
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error) && !near(object->velocity, {}));
        for (unsigned i = 0; i < 35; ++i)
            CHECK(session.tick(error));
        CHECK(session.movement_holds > 0 && near(object->velocity, {}) && near(object->state.m_Pos, pose));
        // Inject original force messages; their movement setup subsumes the
        // legacy payload without growing the private event queue. A full byte wrap must never admit the previous
        // revision.
        auto* player = dynamic_cast<CPlayerObj*>(object->game.get());
        CHECK(player);
        auto& adapter = const_cast<ServerAdapter&>(session.game().server);
        std::vector<Net::PrivateEvent> events;
        for (unsigned i = 0; i < 256; ++i)
        {
            CHECK(session.take_private_events(joined.player, events, error));
            CHECK(player->TeleportClientToServerPos());
        }
        CHECK(player->m_byClientMoveCode == setup.code);
        CHECK(session.movement_setup(joined.player, setup, error));
        CHECK(setup.owner.force_revision > report.owner.force_revision);
        report.sequence = 17;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Ignored);
        CHECK(session.movement_ack(joined.player, ack) && !ack);
        const auto reports = session.movement_reports;
        report = make(setup, 18, object->state.m_Pos, {{.25f, 0, 0}});
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        CHECK(player->TeleportClientToServerPos());
        CHECK(session.tick(error));
        CHECK(session.movement_ack(joined.player, ack) && !ack); // Teleport invalidates a queued report too.
        CHECK(session.movement_reports == reports);
        CHECK(session.movement_setup(joined.player, setup, error));
        // Steps toward the nearest wall stop at it (collision, not the client's word).
        SegmentHit wall;
        DVector direction;
        float distance = 1e9f;
        const auto from = object->state.m_Pos;
        for (const DVector axis : {DVector{1, 0, 0}, DVector{-1, 0, 0}, DVector{0, 0, 1}, DVector{0, 0, -1}})
        {
            SegmentHit hit;
            const auto to = vector_add(from, vector_scale(axis, 4096));
            if (adapter.IntersectSegment(from, to, object->handle, hit) == SegmentResult::Hit && !hit.object &&
                std::abs(vector_dot(hit.normal, axis)) > .99f &&
                vector_dot(vector_subtract(hit.point, from), axis) > 1 &&
                vector_dot(vector_subtract(hit.point, from), axis) < distance)
            {
                wall = hit;
                direction = axis;
                distance = vector_dot(vector_subtract(hit.point, from), axis);
            }
        }
        CHECK(distance < 1e9f);
        std::uint64_t sequence = 19;
        auto origin = from;
        for (float covered = 0; covered < distance + 64;)
        {
            std::vector<DVector> offsets;
            for (unsigned i = 0; i < 24 && covered < distance + 64; ++i, covered += 60)
                offsets.push_back(vector_scale(direction, 60));
            report = make(setup, sequence++, origin, offsets, vector_scale(direction, 7200));
            origin = report.move.position;
            CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
            for (unsigned i = 0; i < 15; ++i)
                CHECK(session.tick(error));
        }
        CHECK(session.movement_ack(joined.player, ack) && ack && ack->sequence == sequence - 1);
        CHECK(vector_dot(vector_subtract(object->state.m_Pos, wall.point), direction) < .01f);
        CHECK(!near(object->state.m_Pos, report.move.position)); // Never the historical teleport bypass.
        // Replayed substeps keep the client's pre-collision velocity, so a remote
        // landing reaches the original stopping-contact falling damage.
        for (unsigned i = 0; i < 15; ++i)
            CHECK(session.tick(error));
        const auto floor = object->state.m_Pos;
        report = make(setup, sequence++, floor, std::vector<DVector>(4, DVector{0, 40, 0}), {0, 300, 0});
        for (auto& step : report.steps)
            step.on_ground = false;
        report.move.on_ground = false;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        for (unsigned i = 0; i < 3; ++i)
            CHECK(session.tick(error));
        const auto top = object->state.m_Pos;
        CHECK(top.y > floor.y + 20);
        const auto health = player->m_damage.m_fHitPoints;
        const auto falls = player->fall_damage_requests;
        // Steps only until the floor: later client substeps start from the
        // landed (clipped) velocity.
        const auto fall = static_cast<std::size_t>(std::ceil((top.y - floor.y) / (2000.f / 120)));
        report = make(setup, sequence++, top, std::vector<DVector>(fall, DVector{0, -2000.f / 120, 0}), {0, -2000, 0});
        for (auto& step : report.steps)
            step.on_ground = false;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error) && session.tick(error));
        CHECK(std::abs(object->state.m_Pos.y - floor.y) < 2);
        CHECK(player->fall_damage_requests == falls + 1 && player->m_damage.m_fHitPoints < health &&
              !player->m_damage.IsDead());
        CHECK(session.movement_setup(joined.player, setup, error));
        const auto previous_force = setup.owner.force_revision;
        auto teleport = from;
        const auto native_revision = player->movement_revision;
        CHECK(adapter.TeleportObject(object->handle, &teleport) == DE_OK);
        CHECK(player->movement_revision == native_revision); // Engine discontinuity without a player force message.
        report.sequence = sequence;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Ignored);
        CHECK(session.movement_setup(joined.player, setup, error) && setup.owner.force_revision > previous_force);
        CHECK(session.movement_ack(joined.player, ack) && !ack);
        // One player's movement failure removes only that player.
        Joined other;
        CHECK(session.join({}, other, error) && session.tick(error));
        MovementSetup other_setup;
        CHECK(session.movement_setup(other.player, other_setup, error));
        const auto* other_object = session.game().objects.get(other.object);
        CHECK(other_object);
        CHECK(session.movement_setup(joined.player, setup, error));
        const auto client = player->GetClient();
        CHECK(session.queue_movement(joined.player, make(setup, ++sequence, object->state.m_Pos, {{.25f, 0, 0}})) ==
              ServerSession::MoveResult::Queued);
        CHECK(session.queue_movement(other.player, make(other_setup, 1, other_object->state.m_Pos, {{.25f, 0, 0}})) ==
              ServerSession::MoveResult::Queued);
        // The original position handler refuses a client no longer bound to its object.
        CHECK(adapter.SetClientUserData(client, nullptr) == DE_OK);
        session.take_warnings();
        CHECK(session.tick(error) && session.movement_failures == 1);
        const auto warnings = session.take_warnings();
        CHECK(warnings.size() == 1 && warnings[0].find("Remote player " + std::to_string(joined.player)) == 0);
        CHECK(!session.movement_setup(joined.player, setup, error));
        CHECK(session.queue_movement(joined.player, make(setup, ++sequence, object->state.m_Pos, {})) ==
              ServerSession::MoveResult::Ignored);
        CHECK(session.movement_ack(other.player, ack) && ack && ack->sequence == 1);
        CHECK(adapter.SetClientUserData(client, object->handle) == DE_OK);
        CHECK(session.leave(joined.player) && !session.game().objects.get(joined.object));
        CHECK(session.tick(error) && session.leave(other.player) && session.tick(error));
        std::cout << "Remote movement: substep replay, order, catch-up, leash, budget, hold, falling damage, 256-code "
                     "wrap and "
                     "isolation passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
