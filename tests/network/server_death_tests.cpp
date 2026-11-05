#include "game/game_settings.h"
#include "game/original/GameStartPoint.h"
#include "server/session.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <sstream>
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
    bool run(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        std::array<Joined, 3> joined;
        for (unsigned i = 0; i < joined.size(); ++i)
        {
            JoinProfile profile;
            profile.name = "Death fixture " + std::to_string(i);
            CHECK(session.join(profile, joined[i], error));
        }
        auto& game = const_cast<GameSession&>(session.game());
        std::array<CPlayerObj*, 3> players;
        for (unsigned i = 0; i < players.size(); ++i)
            players[i] = dynamic_cast<CPlayerObj*>(game.objects.get(joined[i].object)->game.get());
        auto& killer = *players[0];
        auto& victim = *players[1];
        auto& finisher = *players[2];
        std::array<std::vector<PrivateEvent>, 3> log;
        const auto drain = [&]() -> bool
        {
            for (unsigned i = 0; i < joined.size(); ++i)
            {
                std::vector<PrivateEvent> events;
                CHECK(session.take_private_events(joined[i].player, events, error));
                log[i].insert(log[i].end(), events.begin(), events.end());
            }
            return true;
        };
        const auto ticks = [&](unsigned count) -> bool
        {
            for (unsigned i = 0; i < count; ++i)
                CHECK(session.tick(error) && drain());
            return true;
        };
        CHECK(ticks(180));
        MovementSetup setup;
        WeaponLoadout weapons;
        CHECK(session.movement_setup(joined[1].player, setup, error));
        CHECK(session.weapon_loadout(joined[1].player, weapons, error));
        CHECK(drain());
        const auto hit = [&](CPlayerObj& attacker, CPlayerObj& target, float amount, DamageType type) -> bool
        {
            DVector position;
            CHECK(game.server.GetObjectPos(target.m_hObject, &position) == DE_OK);
            auto message = game.server.StartMessageToObject(&attacker, target.m_hObject, damage_message);
            CHECK(message &&
                  game.server.WriteDamage(message, {{0, 0, 1}, position, amount, type, attacker.m_hObject}) == DE_OK);
            CHECK(game.server.EndMessage(message) == DE_OK);
            return true;
        };
        // Choose the next source chance through the existing RNG service. No
        // production hook/seed override, and no statistical chance of a flaky branch.
        const auto chance = [&](bool slow) -> bool
        {
            for (unsigned i = 0; i < 10000; ++i)
            {
                std::minstd_rand copy(game.server.save_clock().generator);
                if ((std::uniform_int_distribution<int>(0, 99)(copy) < 10) == slow)
                    return true;
                int discard;
                CHECK(game.server.RandomInt(0, 99, discard));
            }
            return false;
        };
        std::uint64_t command = 0;
        const auto use = [&](bool accepted = true, bool refresh = true) -> bool
        {
            if (refresh)
                CHECK(session.movement_setup(joined[1].player, setup, error));
            CHECK(session.queue_command(joined[1].player, {setup.owner, ++command, {CommandKind::Use}}) ==
                  ServerSession::CommandResult::Queued);
            CHECK(ticks(1));
            std::vector<CommandAck> acks;
            CHECK(session.take_command_acks(joined[1].player, acks) && acks.size() == 1 && acks[0].applied == accepted);
            return true;
        };
        const auto count = [&](unsigned recipient, auto tag)
        {
            using T = decltype(tag);
            return std::count_if(log[recipient].begin(), log[recipient].end(),
                                 [](const auto& e) { return std::holds_alternative<T>(e.value); });
        };
        CHECK(hit(killer, victim, 1000, DamageType::Normal) && chance(false));
        victim.HandleDeath();
        CHECK(victim.m_damage.IsDead() && !victim.m_bDead && !victim.m_bInSlowDeath && victim.m_fDeathTimer == -1);
        CHECK(killer.m_Frags == 0 && count(1, PlayerDead{}) == 0);
        CHECK(ticks(1));
        CHECK(victim.m_bDead && victim.deaths == 1 && victim.death_corpses == 1 && victim.death_error.empty());
        CHECK(killer.m_Frags == 1 && victim.frag_updates == 1 && count(1, PlayerDead{}) == 1);
        for (unsigned i = 0; i < 3; ++i)
            CHECK(count(i, FragEvent{}) == 1);
        CHECK(!(game.server.GetObjectFlags(victim.m_hObject) & FLAG_VISIBLE));
        CHECK(session.weapon_loadout(joined[1].player, weapons, error) && !weapons.alive && !weapons.fire_allowed);
        CHECK(use());
        CHECK(!victim.m_bDead && !victim.m_damage.IsDead() && victim.m_damage.m_fHitPoints == 100 &&
              victim.m_damage.m_fArmorPoints == 0);
        CHECK(victim.m_InventoryMgr.GetAmmoCount(AMMO_BULLET) == 50 && victim.m_fAirLevel == 100 &&
              killer.m_Frags == 1);
        const auto old_force = setup.owner.force_revision;
        CHECK(use(false, false)); // A prior-life USE cannot act on the new body.
        CHECK(session.movement_setup(joined[1].player, setup, error) && setup.owner.force_revision > old_force);
        CHECK(hit(killer, victim, 1000, DamageType::Normal) && chance(true));
        victim.HandleDeath();
        CHECK(drain() && victim.m_bInSlowDeath && !victim.m_bDead && victim.m_fDeathTimer == 15);
        CHECK(victim.m_fSlowDeathSafeTimer == .5f && victim.m_fSlowDeathStayTimer == 5 && killer.m_Frags == 1);
        CHECK(count(1, WonkyVision{}) == 1 && count(0, FinishHimVoice{}) == 1 && !count(2, FinishHimVoice{}));
        CHECK(session.weapon_loadout(joined[1].player, weapons, error) && weapons.alive && !weapons.input_enabled &&
              !weapons.fire_allowed);
        CHECK(!victim.m_damage.Heal(100) && victim.m_damage.m_fHitPoints == 0);
        CHECK(hit(finisher, victim, 1, DamageType::Normal) && !victim.m_bDead); // Safe .5s.
        CHECK(use() && victim.m_bInSlowDeath);                                  // USE before five seconds is a no-op.
        CHECK(ticks(301));
        CHECK(victim.m_bInSlowDeath && victim.move_state == CPlayerObj::MoveState::Humiliation &&
              !victim.humiliation_animation_pending);
        CHECK(use() && !victim.m_bInSlowDeath && !victim.m_damage.IsDead() && victim.m_damage.m_fHitPoints == 1 &&
              victim.slow_rescues == 1);
        CHECK(killer.m_Frags == 1 && count(1, PlayerDead{}) == 1 && count(1, WonkyVision{}) == 2);
        CHECK(hit(killer, victim, 1000, DamageType::Normal) && chance(true));
        victim.HandleDeath();
        CHECK(ticks(31) && victim.m_fSlowDeathSafeTimer == 0 && victim.m_bInSlowDeath);
        CHECK(hit(finisher, victim, 1, DamageType::Normal) && drain());
        CHECK(victim.m_bDead && victim.humiliation_death && victim.deaths == 2 && killer.m_Frags == 2 &&
              !finisher.m_Frags);
        CHECK(ticks(1) && !victim.humiliation_animation_pending);
        CHECK(game.server.GetObjectFlags(victim.m_hObject) & FLAG_VISIBLE);
        CHECK(game.server.GetModelAnimation(victim.m_hObject) == victim.m_Anim_Sound.m_nAnim_HUMILIATION[3]);
        CHECK(victim.death_corpses == 1 && !victim.death_gibs && use() && victim.death_gibs == 1);
        CHECK(hit(killer, victim, 1000, DamageType::Normal) && chance(true));
        victim.HandleDeath();
        CHECK(ticks(899) && victim.m_bInSlowDeath && victim.m_fDeathTimer > 0);
        // Float subtraction is intentionally the source clock; observe the
        // exact expiry, then its separate confirmation update.
        while (victim.m_bInSlowDeath)
            CHECK(ticks(1) && !victim.m_bDead);
        CHECK(killer.m_Frags == 2 && ticks(1) && victim.m_bDead && killer.m_Frags == 3);
        CHECK(use());
        CHECK(hit(victim, victim, 1000, DamageType::FreeFall));
        const auto before = game.server.save_clock().generator;
        std::minstd_rand expected(before);
        const auto index = std::uniform_int_distribution<int>(0, 65535)(expected);
        std::ostringstream expected_state;
        expected_state << expected;
        victim.HandleDeath();
        CHECK(drain() && victim.m_bDead && victim.m_Frags == -1 && victim.m_fDeathTimer == 0);
        CHECK(std::to_string(game.server.save_clock().generator) == expected_state.str());
        for (const auto& recipient : log)
        {
            const FragEvent* last = nullptr;
            for (const auto& event : recipient)
                if (const auto* value = std::get_if<FragEvent>(&event.value))
                    last = value;
            CHECK(last && last->index == index && last->victim == joined[1].object &&
                  last->killer == joined[1].object && last->killer_frags == -1);
        }
        CHECK(use()); // Exactly one broadcast draw for three recipients; no self slow-death chance.
        // Missing starts make the USE respawn terminal: Respawn's side effects ran,
        // so only this player is isolated (the endpoint removes it); no ACK, and
        // the match keeps running.
        CHECK(hit(victim, victim, 1000, DamageType::FreeFall));
        victim.HandleDeath();
        for (const auto id : game.objects.snapshot())
            if (auto* object = game.objects.get(id); object && dynamic_cast<GameStartPoint*>(object->game.get()))
                CHECK(game.objects.remove(object->handle));
        CHECK(game.objects.finish_frame() && session.movement_setup(joined[1].player, setup, error));
        CHECK(session.queue_command(joined[1].player, {setup.owner, ++command, {CommandKind::Use}}) ==
              ServerSession::CommandResult::Queued);
        CHECK(session.tick(error));
        std::vector<CommandAck> refused;
        CHECK(victim.respawn_failed && session.control_failures == 1);
        CHECK(session.take_command_acks(joined[1].player, refused) && refused.empty());
        CHECK(!session.movement_setup(joined[1].player, setup, error) &&
              error.starts_with("Network respawn failed: No Bloodbath GameStartPoint"));
        error.clear();
        CHECK(killer.m_Frags == 3 && victim.m_Frags == -2 && !finisher.m_Frags);
        CHECK(game.server.client_audio == nullptr && session.movement_steps == 0);
        CHECK(victim.Respawn());
        victim.m_damage.m_bDead = true;
        victim.m_damage.m_fHitPoints = 0;
        victim.m_damage.m_fDeathHitPoints = 0;
        victim.m_damage.last_damage.attacker = nullptr;
        victim.HandleDeath();
        CHECK(!victim.m_bDead && !victim.death_error.empty());
        victim.m_damage.last_damage.attacker = finisher.m_hObject;
        CHECK(session.leave(joined[2].player));
        victim.HandleDeath();
        CHECK(!victim.m_bDead && victim.m_fDeathTimer == -1);
        const auto removed_rng = game.server.save_clock().generator;
        victim.HandleDeath();
        CHECK(victim.m_bDead && victim.death_error.empty() && game.server.save_clock().generator == removed_rng &&
              killer.m_Frags == 3);
        CHECK(session.clear_players());
        std::cout << "Retail Bloodbath death: source delay, slow .5/5/15s, healing gate, USE rescue/respawn, "
                     "humiliation, first-killer frags, self-frags, private/broadcast events, old-life refusal and "
                     "terminal failed-start respawn passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
