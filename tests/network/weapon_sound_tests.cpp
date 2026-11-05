#include "client/network_audio.h"
#include "client/network_weapons.h"
#include "game/game_settings.h"
#include "server/session.h"
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
    bool run(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined a, b;
        JoinProfile profile;
        profile.weapons = {WEAP_SHOTGUN};
        CHECK(session.join(profile, a, error) && session.join({}, b, error) && session.tick(error));
        auto& game = const_cast<GameSession&>(session.game());
        auto* player = dynamic_cast<CPlayerObj*>(game.objects.get(a.object)->game.get());
        CHECK(player && !game.server.client_audio);
        Resources resources;
        CHECK(resources.mount_game(config, error));
        ModelStore models(resources);
        NetworkWeapons weapons;
        CHECK(weapons.begin(a));
        NetworkAudio audio[2];
        CHECK(audio[0].begin(a) && audio[1].begin(b));
        audio[0].mixer.inspect_pcm = audio[1].mixer.inspect_pcm = true;
        CHECK(session.begin_audio(a.player, error) && session.begin_audio(b.player, error));
        ClientWorld world;
        CHECK(world.begin(a.world));
        MovementSetup setup;
        WeaponLoadout loadout;
        std::vector<PrivateEvent> events;
        std::vector<AudioEvent> sounds;
        unsigned keyed_starts[2]{};
        float peaks[2]{};
        const auto receive_audio = [&]() -> bool
        {
            for (unsigned i = 0; i < 2; ++i)
            {
                CHECK(session.take_audio_events(i ? b.player : a.player, sounds, error));
                for (const auto& event : sounds)
                {
                    if (const auto* start = std::get_if<SoundStart>(&event.value);
                        start && start->owner == a.object && start->parameters.priority == 2)
                    {
                        CHECK(start->speaker == a.object && start->parameters.space == SoundSpace::Spatial);
                        CHECK(start->parameters.outer_radius == 1000 && start->parameters.inner_radius == 100);
                        CHECK(start->parameters.reverb_requested && !start->parameters.streaming);
                        ++keyed_starts[i];
                    }
                    CHECK(audio[i].receive(event, resources, error));
                }
                const auto position = game.objects.get(a.object)->state.m_Pos;
                CHECK(audio[i].advance(ServerClock::step, world, position, {}));
                peaks[i] = std::max(peaks[i], audio[i].mixer.peak);
            }
            return true;
        };
        const auto publish = [&]() -> bool
        {
            CHECK(session.movement_setup(a.player, setup, error));
            CHECK(session.weapon_loadout(a.player, loadout, error));
            CHECK(session.take_private_events(a.player, events, error));
            for (const auto& event : events)
                CHECK(weapons.receive(event, models, resources, error) != NetworkWeapons::Reception::Refused);
            CHECK(session.take_private_events(b.player, events, error));
            return true;
        };
        CHECK(publish() && receive_audio());
        WeaponFrame frame{setup.owner, 0, weapons.state_sequence(), {-1.4f, 0}, false, false};
        const auto step = [&](bool fire, bool current = true) -> bool
        {
            if (current)
                CHECK(publish());
            frame.owner.force_revision = weapons.state()->force_revision;
            frame.state = weapons.state_sequence();
            frame.primary = fire;
            ++frame.sequence;
            CHECK(session.queue_weapon_frame(a.player, frame) == ServerSession::CommandResult::Queued);
            CHECK(session.queue_weapon_frame(a.player, frame) == ServerSession::CommandResult::Ignored);
            CHECK(weapons.update(frame.owner.force_revision, ServerClock::step, error, {fire}, frame.aim));
            CHECK(session.tick(error) && receive_audio());
            return true;
        };
        for (unsigned i = 0; i < 120; ++i)
            CHECK(step(false));
        for (unsigned i = 0; i < 300; ++i)
            CHECK(step(true));
        CHECK(player->weapon_sound_starts && !player->weapon_sound_failures && weapons.sound_requests);
        CHECK(weapons.views()[0].animation_sound_error.empty());
        CHECK(keyed_starts[0] == player->weapon_sound_starts && keyed_starts[1] == keyed_starts[0]);
        CHECK(peaks[0] > 0 && peaks[1] > 0 && !audio[0].local_started && !audio[1].local_started);
        const auto actual = player->weapon_sound_starts;
        CHECK(player->TeleportClientToServerPos());
        for (unsigned i = 0; i < 180; ++i)
            CHECK(step(true, false));
        CHECK(player->weapon_sound_starts > actual && !session.weapon_sounds_blocked);
        CHECK(publish());
        for (unsigned i = 0; i < 60; ++i)
            CHECK(step(false));
        // One retained loop per player, shared by both hands. A missing replacement
        // still kills the old loop, while an ordinary one-shot does not replace it.
        constexpr auto sound = "sounds/weapons/beretta/fire.wav";
        CHECK(player->HandleWeaponSound(WeaponSoundAction::Loop, sound));
        auto loop = player->m_hWeapSound;
        CHECK(loop && game.server.audio.get(loop)->request.loop && receive_audio());
        CHECK(audio[0].mixer.get(loop) && audio[1].mixer.get(loop));
        CHECK(player->HandleWeaponSound(WeaponSoundAction::OneShot, sound) && player->m_hWeapSound == loop);
        CHECK(player->HandleWeaponSound(WeaponSoundAction::Loop, sound));
        CHECK(player->m_hWeapSound != loop && !game.server.audio.get(loop) && receive_audio());
        CHECK(!audio[0].mixer.get(loop) && !audio[1].mixer.get(loop));
        loop = player->m_hWeapSound;
        CHECK(!player->HandleWeaponSound(WeaponSoundAction::Loop, "missing.wav") && !player->m_hWeapSound);
        CHECK(!player->weapon_sound_error.empty() && !game.server.audio.get(loop) && receive_audio());
        CHECK(!audio[0].mixer.get(loop) && !audio[1].mixer.get(loop));
        CHECK(player->HandleWeaponSound(WeaponSoundAction::Loop, sound) && receive_audio());
        loop = player->m_hWeapSound;
        CHECK(player->HandleWeaponSound(WeaponSoundAction::Stop, {}) && !player->m_hWeapSound && receive_audio());
        CHECK(!game.server.audio.get(loop) && !audio[0].mixer.get(loop));
        CHECK(player->HandleWeaponSound(WeaponSoundAction::Loop, sound) && receive_audio());
        loop = player->m_hWeapSound;
        CHECK(player->m_InventoryMgr.ChangeWeaponSlot(0) == InventoryResult::Ok && publish() && receive_audio());
        CHECK(!player->m_hWeapSound && !game.server.audio.get(loop) && !audio[1].mixer.get(loop));
        // Historical shotgun keys cannot restart or stop the current knife loop.
        CHECK(player->HandleWeaponSound(WeaponSoundAction::Loop, sound) && receive_audio());
        loop = player->m_hWeapSound;
        auto old = frame;
        old.primary = true;
        const auto blocked_before = session.weapon_sounds_blocked;
        for (unsigned i = 0; i < 180; ++i)
        {
            ++old.sequence;
            CHECK(session.queue_weapon_frame(a.player, old) == ServerSession::CommandResult::Queued);
            CHECK(session.tick(error) && receive_audio() && player->m_hWeapSound == loop);
        }
        CHECK(session.weapon_sounds_blocked > blocked_before);
        // Exercise the retail Minigun's actual SOUNDLOOP/SOUNDSTOP keys through
        // the validated timeline, not just the player's sound service directly.
        frame.sequence = old.sequence;
        CHECK(player->m_InventoryMgr.ObtainWeapon(WEAP_MINIGUN) == InventoryResult::Ok);
        CHECK(player->m_InventoryMgr.ChangeWeapon(WEAP_MINIGUN) == InventoryResult::Ok);
        player->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 1000);
        CHECK(publish() && !player->m_hWeapSound && receive_audio());
        for (unsigned i = 0; i < 120; ++i)
            CHECK(step(false));
        for (unsigned i = 0; i < 180; ++i)
            CHECK(step(true));
        const auto minigun_loop = player->m_hWeapSound;
        CHECK(minigun_loop && game.server.audio.get(minigun_loop)->request.loop);
        CHECK(audio[0].mixer.get(minigun_loop) && audio[1].mixer.get(minigun_loop));
        const auto stops_before = player->weapon_sound_stops;
        CHECK(player->TeleportClientToServerPos());
        for (unsigned i = 0; i < 180; ++i)
            CHECK(step(false, false)); // Same life/view, delayed pre-correction SOUNDSTOP.
        CHECK(!player->m_hWeapSound && player->weapon_sound_stops == stops_before + 1);
        CHECK(!game.server.audio.get(minigun_loop) && !audio[0].mixer.get(minigun_loop) &&
              !audio[1].mixer.get(minigun_loop));
        CHECK(player->HandleWeaponSound(WeaponSoundAction::Loop, sound) && receive_audio());
        loop = player->m_hWeapSound;
        player->m_damage.m_bDead = true;
        player->m_damage.m_fHitPoints = 0;
        player->m_damage.last_damage.attacker = player->m_hObject;
        player->m_damage.reaction_damage_type = DamageType::FreeFall;
        player->HandleDeath();
        CHECK(player->m_bDead && !player->m_hWeapSound && !game.server.audio.get(loop) && receive_audio());
        CHECK(!audio[0].mixer.get(loop) && !audio[1].mixer.get(loop));
        CHECK(player->Respawn() && player->GoToStartPoint(nullptr, false));
        CHECK(publish());
        CHECK(player->HandleWeaponSound(WeaponSoundAction::Loop, sound) && receive_audio());
        loop = player->m_hWeapSound;
        const auto new_life_starts = player->weapon_sound_starts, old_life_blocked = session.weapon_sounds_blocked;
        old = frame;
        // Same weapon/view, but all these frames reference the preceding life.
        for (unsigned i = 0; i < 360; ++i)
        {
            old.primary = i < 180;
            ++old.sequence;
            CHECK(session.queue_weapon_frame(a.player, old) == ServerSession::CommandResult::Queued);
            CHECK(session.tick(error) && receive_audio() && player->m_hWeapSound == loop);
        }
        CHECK(player->weapon_sound_starts == new_life_starts && session.weapon_sounds_blocked > old_life_blocked);
        CHECK(session.leave(a.player) && !game.server.audio.get(loop));
        CHECK(session.take_audio_events(b.player, sounds, error));
        for (const auto& event : sounds)
            CHECK(audio[1].receive(event, resources, error));
        CHECK(!audio[1].mixer.get(loop) && session.movement_steps == 0);
        std::cout << "Weapon keys: actual shotgun timeline, duplicate/stale gates, two independent PCM receivers, "
                     "retail Minigun loop/stop, one player loop, replacement/selection/death/departure; starts="
                  << actual << ", blocked=" << session.weapon_sounds_blocked << ", peaks=" << peaks[0] << ','
                  << peaks[1] << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
