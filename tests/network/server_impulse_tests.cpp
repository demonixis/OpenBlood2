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
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool near(DVector a, DVector b)
    {
        return std::abs(a.x - b.x) < .001f && std::abs(a.y - b.y) < .001f && std::abs(a.z - b.z) < .001f;
    }
    bool run(const char* data)
    {
        Config config;
        config.data_dir = path_from_utf8(data);
        std::string error;
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined joined, other;
        CHECK(session.join({}, joined, error) && session.join({}, other, error));
        auto& game = const_cast<GameSession&>(session.game());
        auto* object = game.objects.get(joined.object);
        auto* player = dynamic_cast<CPlayerObj*>(object->game.get());
        CHECK(player && player->m_damage.replicate_velocity);
        auto drain = [&]() -> bool
        {
            for (auto id : {joined.player, other.player})
            {
                std::vector<PrivateEvent> events;
                CHECK(session.take_private_events(id, events, error));
            }
            return true;
        };
        auto tick = [&]() -> bool
        {
            CHECK(session.tick(error) && drain());
            return true;
        };
        CHECK(tick() && tick());
        MovementSetup setup;
        CHECK(session.movement_setup(joined.player, setup, error));
        // One-substep report from the server body, as the client integrates it.
        auto make = [&](std::uint64_t sequence, DVector velocity, std::uint64_t impulse)
        {
            const float seconds = 1.f / 120;
            MovementReport value{setup.owner, sequence, {setup.code, object->state.m_Pos, velocity, {}, true, 0},
                                 {},          0,        object->state.m_Pos,
                                 {},          impulse};
            value.steps.push_back({vector_scale(velocity, seconds), velocity, {}, seconds, true, 0});
            value.move.position = vector_add(value.origin, value.steps[0].offset);
            return value;
        };
        auto report = make(1, {1, 0, 0}, 0);
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued && tick());
        CHECK(near(object->velocity, {1, 0, 0}));
        // Original damage accumulation and once-consumed door push share one delta.
        player->m_damage.ApplyDamagePhysics(1, {1, 0, 0});
        player->m_damage.ApplyDamagePhysics(2, {1, 0, 0});
        player->door_push_pending = true;
        player->door_push_velocity = {0, 40, 0};
        CHECK(near(object->velocity, {31, 0, 0}));
        std::vector<MovementImpulse> impulses;
        CHECK(session.movement_impulses(joined.player, 0, impulses, error) && impulses.size() == 1);
        CHECK(impulses[0].sequence == 1 && near(impulses[0].velocity, {30, 40, 0}));
        CHECK(near(object->velocity, {31, 40, 0}) && !player->door_push_pending);
        CHECK(!player->m_damage.added_velocity_pending);
        CHECK(session.movement_impulses(joined.player, 1, impulses, error) && impulses.empty());
        // A report integrated before the delta: each substep adds the missing
        // velocity and its displacement, before collision.
        const auto before = object->state.m_Pos;
        report = make(2, {}, 0);
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued && tick());
        std::optional<MovementAck> ack;
        CHECK(session.movement_ack(joined.player, ack) && ack && ack->sequence == 2 && ack->impulse_sequence == 1);
        CHECK(near(ack->accepted.velocity, {30, 40, 0}));
        CHECK(near(object->state.m_Pos, vector_add(before, vector_scale({30, 40, 0}, 1.f / 120))));
        // Not yet acknowledged by a report: still retained for the peer.
        CHECK(session.movement_impulses(joined.player, 0, impulses, error) && impulses.size() == 1);
        // A client that has consumed the impulse carries the new velocity itself.
        report = make(3, {8, 20, 0}, 1); // Original drag/gravity may already have changed it.
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued && tick());
        CHECK(near(object->velocity, {8, 20, 0}));
        CHECK(session.movement_impulses(joined.player, 0, impulses, error) && impulses.empty());
        auto invalid = make(4, {}, 2);
        CHECK(session.queue_movement(joined.player, invalid) == ServerSession::MoveResult::Invalid);
        invalid.impulse_sequence = 0;
        CHECK(session.queue_movement(joined.player, invalid) == ServerSession::MoveResult::Invalid);
        // A dead body keeps the server's own motion: reports are received but
        // their substeps are not replayed.
        player->m_damage.m_bDead = DTRUE;
        player->m_damage.m_fHitPoints = 0;
        player->m_damage.reaction_damage_type = DamageType::FreeFall;
        player->DoDeath();
        CHECK(player->m_bDead && player->death_messages == 1);
        const auto dead_position = object->state.m_Pos;
        report = make(4, {0, 600, 0}, 1);
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued && tick());
        CHECK(near(object->state.m_Pos, dead_position));
        // A new force/life retires unsent original deltas and every old ACK.
        player->m_damage.ApplyDamagePhysics(1, {1, 0, 0});
        CHECK(player->TeleportClientToServerPos());
        const auto old_owner = setup.owner;
        CHECK(session.movement_setup(joined.player, setup, error));
        CHECK(setup.owner.force_revision > old_owner.force_revision);
        CHECK(!player->m_damage.added_velocity_pending && near(object->velocity, {}));
        CHECK(session.movement_impulses(joined.player, 0, impulses, error) && impulses.empty());
        report = make(5, {}, 0);
        report.owner = old_owner;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Ignored);
        report = make(6, {}, 0);
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued && tick());
        // Bound an unacknowledging peer without stopping other players/the server.
        for (unsigned i = 0; i < movement_impulse_limit; ++i)
        {
            player->m_damage.ApplyDamagePhysics(.01f, {1, 0, 0});
            CHECK(session.movement_impulses(joined.player, i, impulses, error) && impulses.size() == 1);
        }
        player->m_damage.ApplyDamagePhysics(.01f, {1, 0, 0});
        CHECK(!session.movement_impulses(joined.player, movement_impulse_limit, impulses, error));
        CHECK(tick() && session.player_count() == 2);
        CHECK(!session.movement_setup(joined.player, setup, error));
        CHECK(session.movement_setup(other.player, setup, error));
        CHECK(session.leave(joined.player) && session.tick(error));
        std::cout << "Server impulses: damage+door, replayed missing deltas, acknowledged reports, dead bodies, force "
                     "retirement and "
                     "isolated capacity passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
