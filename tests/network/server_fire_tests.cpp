#include "client/network_weapons.h"
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
    struct Target : BaseClass
    {
        CDestructable damage;
        Target()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD message, void* data, float value) override
        {
            if (message == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject, nullptr);
                damage.SetHitPoints(10000);
                damage.m_fMaxHitPoints = 10000;
            }
            return BaseClass::EngineMessageFn(message, data, value);
        }
    };
    std::unique_ptr<BaseClass> make_target()
    {
        return std::make_unique<Target>();
    }
    bool run(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined joined;
        JoinProfile profile;
        profile.weapons = {WEAP_BERETTA};
        CHECK(session.join(profile, joined, error) && session.tick(error));
        auto& game = const_cast<GameSession&>(session.game());
        auto* player = dynamic_cast<CPlayerObj*>(game.objects.get(joined.object)->game.get());
        CHECK(player);
        auto& inv = player->m_InventoryMgr;
        inv.SetAmmoCount(AMMO_BULLET, 100);
        auto* gun = inv.CurrentWeapon();
        CHECK(gun);
        NetworkWeapons client;
        CHECK(client.begin(joined));
        MovementSetup setup;
        WeaponLoadout published;
        std::vector<PrivateEvent> events;
        const auto publish = [&]() -> bool
        {
            CHECK(session.movement_setup(joined.player, setup, error));
            CHECK(session.weapon_loadout(joined.player, published, error));
            CHECK(session.take_private_events(joined.player, events, error));
            for (const auto& event : events)
                CHECK(client.receive(event, game.models, game.resources, error) != NetworkWeapons::Reception::Refused);
            return true;
        };
        CHECK(publish());
        using Reception = ServerSession::CommandResult;
        WeaponFrame frame{setup.owner, 0, client.state_sequence(), {-1.4f, 0}, false, false};
        DVector source = game.objects.get(joined.object)->state.m_Pos;
        source.y += player->eye_level;
        DVector target_position;
        bool clear = false;
        for (unsigned direction = 0; direction < 8 && !clear; ++direction)
        {
            frame.aim = {0, float(direction) * .7853981634f};
            target_position = vector_add(source, {std::sin(frame.aim.yaw) * 48, 0, std::cos(frame.aim.yaw) * 48});
            SegmentHit hit;
            clear =
                game.server.IntersectSegment(source, target_position, player->m_hObject, hit) == SegmentResult::Miss;
        }
        CHECK(clear);
        static ClassInfo target_class{"NetworkFireTarget", nullptr, make_target, {}};
        target_class.parent = game.objects.find_class("BaseClass");
        CHECK(game.objects.register_class(target_class));
        ObjectCreateStruct target_state;
        target_state.m_Pos = target_position;
        target_state.m_Flags = FLAG_SOLID;
        const auto target_handle = game.objects.create(&target_class, target_state);
        DVector target_dims{8, 8, 8};
        CHECK(target_handle && game.server.SetObjectDims(target_handle, &target_dims) == DE_OK);
        auto* target = dynamic_cast<Target*>(game.server.HandleToObject(target_handle));
        CHECK(target);
        std::optional<WeaponFrameAck> ack;
        const auto step = [&](bool firing, bool receive = true) -> bool
        {
            if (receive)
                CHECK(publish());
            frame.owner.force_revision = client.state()->force_revision;
            frame.state = client.state_sequence();
            frame.primary = firing;
            ++frame.sequence;
            CHECK(session.queue_weapon_frame(joined.player, frame) == Reception::Queued);
            CHECK(session.queue_weapon_frame(joined.player, frame) == Reception::Ignored);
            CHECK(client.update(frame.owner.force_revision, ServerClock::step, error, {frame.primary, frame.alternate},
                                frame.aim));
            CHECK(session.tick(error));
            CHECK(session.weapon_frame_ack(joined.player, ack) && ack && ack->sequence == frame.sequence);
            return true;
        };
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(false));
        for (unsigned tick = 0; tick < 300; ++tick)
            CHECK(step(true));
        CHECK(ack->fired[0] > 0 && ack->fired[0] == gun->shots && !ack->fired[1] && !ack->blocked);
        CHECK(client.views()[0].shots == gun->shots && gun->CheckAmmo() == 100 - float(gun->shots));
        const auto eye = game.objects.get(joined.object)->state.m_Pos;
        CHECK(gun->last_shot.request.position.x == eye.x && gun->last_shot.request.position.z == eye.z &&
              gun->last_shot.request.position.y == eye.y + player->eye_level);
        const auto first_shots = gun->shots;
        const auto damaged = target->damage.m_fHitPoints;
        CHECK(damaged < 10000 && damaged > 9000 && gun->last_shot.damage_delivered &&
              gun->last_shot.hit.object == target_handle);
        // The issued state still allows fire. Current authority must refuse it,
        // while both animation timelines continue consuming the same inputs.
        player->movement_blocked = true;
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(true, false));
        CHECK(gun->shots == first_shots && ack->blocked > 0 && client.views()[0].shots > gun->shots &&
              target->damage.m_fHitPoints == damaged);
        player->movement_blocked = false;
        const auto blocked = ack->blocked;
        CHECK(player->TeleportClientToServerPos());
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(true, false));
        CHECK(gun->shots == first_shots && ack->blocked > blocked &&
              ack->owner.force_revision > frame.owner.force_revision);
        CHECK(publish());
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(true));
        CHECK(gun->shots > first_shots);
        const auto before_dry = gun->shots;
        inv.SetAmmoCount(AMMO_BULLET, 0);
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(true, false));
        CHECK(gun->shots == before_dry && gun->CheckAmmo() == 0);
        inv.SetAmmoCount(AMMO_BULLET, 100);
        CHECK(publish());
        // Published A -> B -> A cannot reuse a historical A's active fire state.
        const auto old_state = client.state_sequence();
        const auto old_revision = client.state()->view_revision[0];
        CHECK(inv.ChangeWeaponSlot(0) == InventoryResult::Ok);
        CHECK(session.weapon_loadout(joined.player, published, error));
        CHECK(inv.ChangeWeaponSlot(1) == InventoryResult::Ok);
        CHECK(session.weapon_loadout(joined.player, published, error));
        CHECK(published.view_revision[0] == old_revision + 2);
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(true, false));
        CHECK(gun->shots == before_dry && client.state_sequence() == old_state);
        CHECK(publish() && client.views()[0].state == WS_DRAW);
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(false));
        auto invalid = frame;
        invalid.sequence += 2;
        CHECK(session.queue_weapon_frame(joined.player, invalid) == Reception::Invalid);
        --invalid.sequence;
        invalid.state = UINT64_MAX;
        CHECK(session.queue_weapon_frame(joined.player, invalid) == Reception::Invalid);
        invalid.state = frame.state;
        ++invalid.owner.player;
        CHECK(session.queue_weapon_frame(joined.player, invalid) == Reception::Invalid);
        invalid.owner = frame.owner;
        ++invalid.owner.world;
        CHECK(session.queue_weapon_frame(joined.player, invalid) == Reception::Ignored);
        invalid.owner = frame.owner;
        invalid.state = old_state;
        CHECK(session.queue_weapon_frame(joined.player, invalid) == Reception::Invalid);
        // Adding a left Beretta must retain the right hand's timeline. Original
        // dual alternate input becomes primary independently for both hands.
        const auto right_animation = client.views()[0].animation;
        CHECK(inv.ObtainWeapon(WEAP_BERETTA) == InventoryResult::Ok && publish());
        CHECK(client.views()[0].animation.revision == right_animation.revision &&
              client.views()[0].animation.time == right_animation.time && client.views()[1].state == WS_DRAW);
        frame.alternate = true;
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(false));
        CHECK(ack->fired[1] > 0 && inv.CurrentWeapon(true)->shots == ack->fired[1] &&
              !gun->last_shot.request.alternate && !inv.CurrentWeapon(true)->last_shot.request.alternate);
        frame.alternate = false;
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(false));
        // Engage an attack, then confirm death and respawn without any client
        // frame/publication pass between them. The reliable dead state must
        // still cancel FIRE_KEY on both sequencers; no gun/resource recreation.
        CHECK(step(true));
        CHECK(client.views()[0].state == WS_START_FIRING || client.views()[0].state == WS_FIRING);
        const auto before_death = gun->shots;
        const auto death_revision = client.state()->view_revision[0];
        player->m_damage.m_bDead = true;
        player->m_damage.m_fHitPoints = 0;
        player->m_damage.last_damage.attacker = player->m_hObject;
        player->m_damage.reaction_damage_type = DamageType::FreeFall;
        player->HandleDeath();
        CHECK(player->m_bDead && player->Respawn() && player->GoToStartPoint(nullptr, false));
        CHECK(publish() && client.state()->alive && client.state()->view_revision[0] == death_revision &&
              client.views()[0].state == WS_REST);
        CHECK(std::any_of(events.begin(), events.end(),
                          [](const auto& event)
                          {
                              const auto* value = std::get_if<WeaponLoadout>(&event.value);
                              return value && !value->alive;
                          }));
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(step(false));
        CHECK(gun->shots == before_death);
        // Idle accrues at most half a second of credit. A burst processes at most
        // two inputs per world tick (never above 2x the fire rate), and its
        // following selection must stay behind it.
        for (unsigned tick = 0; tick < 120; ++tick)
            CHECK(session.tick(error));
        CHECK(publish());
        frame.state = client.state_sequence();
        frame.primary = frame.alternate = false;
        const auto before = frame.sequence;
        for (unsigned i = 0; i < weapon_frame_queue_limit; ++i)
        {
            ++frame.sequence;
            CHECK(session.queue_weapon_frame(joined.player, frame) == Reception::Queued);
        }
        auto excess = frame;
        ++excess.sequence;
        CHECK(session.queue_weapon_frame(joined.player, excess) == Reception::Invalid);
        CHECK(session.queue_command(joined.player, {frame.owner, 1, {CommandKind::WeaponSlot, 0}}) ==
              Reception::Queued);
        CHECK(session.tick(error) && session.weapon_frame_ack(joined.player, ack) &&
              ack->sequence == before + ServerSession::weapon_frames_per_tick);
        CHECK(inv.m_nCurWeaponSlot == 1);
        for (unsigned tick = 0; tick < 60 && ack->sequence != frame.sequence; ++tick)
        {
            const auto previous = ack->sequence;
            CHECK(session.tick(error) && session.weapon_frame_ack(joined.player, ack) &&
                  ack->sequence - previous <= ServerSession::weapon_frames_per_tick);
        }
        CHECK(session.weapon_frame_ack(joined.player, ack) && ack->sequence == frame.sequence &&
              inv.m_nCurWeaponSlot == 0);
        std::vector<CommandAck> commands;
        CHECK(session.take_command_acks(joined.player, commands) && commands.size() == 1 && commands[0].applied);
        CHECK(session.movement_steps == 0); // Weapon frames never integrate CMoveMgr/server movement.
        CHECK(game.objects.remove(target_handle) && game.objects.finish_frame());
        CHECK(session.leave(joined.player));
        CHECK(session.queue_weapon_frame(joined.player, frame) == Reception::Invalid);
        std::cout
            << "Server firing: real animation cadence/damage/ammo/origin, prediction versus authority, duplicates, "
               "blocked/stale-force/dry/old-view gates, bounded bursts and ordered commands passed; actual shots="
            << ack->fired[0] << ", refused=" << ack->blocked << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
