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
        MovementReport report;
        report.owner = setup.owner;
        report.sequence = 1;
        report.move = {setup.code, initial, {12, 0, 0}, {0, 1234, 0}, true, 0};
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        CHECK(near(object->state.m_Pos, initial));
        CHECK(session.tick(error));
        CHECK(near(object->state.m_Pos, initial)); // No velocity*dt on the report tick.
        std::optional<MovementAck> ack;
        CHECK(session.movement_ack(joined.player, ack) && ack && ack->sequence == 1);
        CHECK(near(ack->accepted.position, initial) && near(ack->accepted.velocity, {12, 0, 0}));
        CHECK(session.tick(error));
        CHECK(near(object->state.m_Pos, vector_add(initial, {.2f, 0, 0})));
        CHECK(near(object->velocity, {12, 0, 0})); // No gravity/acceleration reintegration.
        CHECK(session.movement_steps == 2);
        CHECK(session.movement_ack(joined.player, ack) && near(ack->accepted.position, initial));
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
        report.sequence = 2;
        report.move.position = object->state.m_Pos;
        report.move.velocity = {};
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        report.sequence = 3;
        report.move.position.x += .5f;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        CHECK(session.tick(error) && near(object->state.m_Pos, report.move.position));
        CHECK(session.movement_reports == 2 && session.movement_coalesced == 1);
        CHECK(session.movement_ack(joined.player, ack) && ack->sequence == 3);
        report.sequence = 4;
        report.move.velocity = {12, 0, 0};
        report.move.position = object->state.m_Pos;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error));
        for (unsigned i = 0; i < 35; ++i)
            CHECK(session.tick(error));
        CHECK(session.movement_holds > 0 && near(object->velocity, {}));
        // Inject original force messages and drain their local queue in this
        // fixture only. A full byte wrap must never admit the previous revision.
        auto* player = dynamic_cast<CPlayerObj*>(object->game.get());
        CHECK(player);
        auto& adapter = const_cast<ServerAdapter&>(session.game().server);
        std::vector<PlayerEvent> events;
        for (unsigned i = 0; i < 256; ++i)
        {
            CHECK(adapter.take_client_events(player->GetClient(), events));
            CHECK(player->TeleportClientToServerPos());
        }
        CHECK(player->m_byClientMoveCode == setup.code);
        CHECK(session.movement_setup(joined.player, setup, error));
        CHECK(setup.owner.force_revision > report.owner.force_revision);
        report.sequence = 5;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Ignored);
        CHECK(session.movement_ack(joined.player, ack) && !ack);
        report.owner = setup.owner;
        report.move.position = object->state.m_Pos;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued);
        CHECK(player->TeleportClientToServerPos());
        CHECK(session.tick(error));
        CHECK(session.movement_ack(joined.player, ack) && !ack); // Teleport invalidates a queued report too.
        CHECK(session.movement_reports == 3);
        CHECK(session.movement_setup(joined.player, setup, error));
        SegmentHit wall;
        DVector direction;
        bool found_wall = false;
        const auto from = object->state.m_Pos;
        for (const DVector axis :
             {DVector{1, 0, 0}, DVector{-1, 0, 0}, DVector{0, 0, 1}, DVector{0, 0, -1}, DVector{0, 1, 0}})
        {
            const auto to = vector_add(from, vector_scale(axis, 4096));
            if (adapter.IntersectSegment(from, to, object->handle, wall) == SegmentResult::Hit && !wall.object &&
                std::abs(vector_dot(wall.normal, axis)) > .99f &&
                vector_dot(vector_subtract(wall.point, from), axis) > 1)
            {
                direction = axis;
                found_wall = true;
                break;
            }
        }
        CHECK(found_wall);
        report.owner = setup.owner;
        report.sequence = 6;
        report.move.move_code = setup.code;
        report.move.position = vector_add(wall.point, vector_scale(direction, 64));
        report.move.velocity = report.move.acceleration = {};
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error));
        CHECK(session.movement_ack(joined.player, ack) && ack && ack->sequence == 6);
        CHECK(vector_dot(vector_subtract(object->state.m_Pos, wall.point), direction) < .01f);
        CHECK(!near(object->state.m_Pos,
                    report.move.position)); // Collision correction, never the historical teleport bypass.
        const auto previous_force = setup.owner.force_revision;
        auto teleport = from;
        const auto native_revision = player->movement_revision;
        CHECK(adapter.TeleportObject(object->handle, &teleport) == DE_OK);
        CHECK(player->movement_revision == native_revision); // Engine discontinuity without a player force message.
        report.sequence = 7;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Ignored);
        CHECK(session.movement_setup(joined.player, setup, error) && setup.owner.force_revision > previous_force);
        CHECK(session.movement_ack(joined.player, ack) && !ack);
        CHECK(session.leave(joined.player) && !session.game().objects.get(joined.object));
        CHECK(session.tick(error));
        std::cout
            << "Remote movement: checked reports, coalescing, two velocity substeps, hold and 256-code wrap passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
