#pragma once
#include "engine/multiplayer_rules.h"
#include "game/server_adapter.h"
#include "net/admission.h"
#include "net/audio_events.h"
#include "net/content.h"
#include "net/effect_events.h"
#include "net/movement.h"
#include "net/player_commands.h"
#include "net/player_events.h"
#include "net/weapon_frames.h"
#include "net/world_snapshot.h"
#include "server/clock.h"
#include <deque>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace OpenBlood2
{
    // Dedicated simulation owner. The current shared GameSession storage is
    // transitional until the client replica owns all presentation state.
    class ServerSession
    {
      public:
        ~ServerSession();
        bool start(const Config&, std::string_view world, unsigned mode, std::string& error,
                   const MultiplayerRules& rules = {});
        bool tick(std::string& error);
        bool changing_level() const
        {
            return ending_tick_.has_value();
        }
        bool rotation_due() const
        {
            return ending_tick_ && ticks_ > *ending_tick_ && ticks_ - *ending_tick_ > 450;
        }
        // Called only by the network owner between ticks, before offering the new epoch.
        bool rotate(std::string& error);
        std::uint64_t rotations = 0;
        std::uint64_t ticks() const
        {
            return ticks_;
        }
        std::uint64_t total_ticks() const
        {
            return total_ticks_;
        }
        // Trusted local call after the endpoint's protocol/content/loading gates.
        bool join(const Net::JoinProfile&, Net::Joined&, std::string& error);
        DBYTE player_team(std::uint64_t player);
        bool snapshot(std::uint64_t player, Net::WorldSnapshot&, std::string& error);
        enum class MoveResult
        {
            Queued,
            Ignored,
            Invalid
        };
        MoveResult queue_movement(std::uint64_t player, const Net::MovementReport&);
        bool movement_setup(std::uint64_t player, Net::MovementSetup&, std::string& error);
        // True until the original first update (InitializeClientState) has
        // teleported the client: a setup published before it is superseded at once.
        bool awaiting_first_update(std::uint64_t player);
        bool movement_ack(std::uint64_t player, std::optional<Net::MovementAck>&) const;
        // Reliable PSTATE_ADDVELOCITY deltas after `after`, for the current force.
        bool movement_impulses(std::uint64_t player, std::uint64_t after, std::vector<Net::MovementImpulse>&,
                               std::string& error);
        // Reports are replayed in order, one resolved CMoveMgr substep at a time.
        // dropped: overflow of the bounded queue; catchups: one sweep to a
        // report's origin after a lost report; leashed: steps or catch-ups
        // refused by the speed limit or movement-time budget; failures: players
        // removed after a movement error (the match keeps running).
        std::uint64_t movement_reports = 0, movement_ignored = 0, movement_dropped = 0;
        std::uint64_t movement_steps = 0, movement_holds = 0, movement_catchups = 0, movement_leashed = 0;
        std::uint64_t movement_failures = 0;
        static constexpr std::size_t movement_queue_limit = 8;
        // Real time a player may still move: refilled 5 % faster than the
        // server clock (client/server clock drift), capped at half a second.
        static constexpr double movement_budget_rate = 1.05, movement_budget_limit = .5;
        bool weapon_loadout(std::uint64_t player, Net::WeaponLoadout&, std::string& error);
        enum class CommandResult
        {
            Queued,
            Ignored,
            Invalid
        };
        CommandResult queue_command(std::uint64_t player, const Net::PlayerCommand&);
        CommandResult queue_weapon_frame(std::uint64_t player, const Net::WeaponFrame&);
        bool weapon_frame_ack(std::uint64_t player, std::optional<Net::WeaponFrameAck>&) const;
        bool take_command_acks(std::uint64_t player, std::vector<Net::CommandAck>&);
        std::uint64_t commands_applied = 0, commands_blocked = 0, commands_ignored = 0;
        // Players removed after a command or weapon-frame error.
        std::uint64_t control_failures = 0;
        // Weapon frames: banked credit (half a second) and catch-up rate per tick.
        static constexpr unsigned weapon_credit_limit = 30, weapon_frames_per_tick = 2;
        std::uint64_t weapon_sounds_blocked = 0;
        bool begin_audio(std::uint64_t player, std::string& error);
        bool begin_effects(std::uint64_t player);
        bool take_effect_events(std::uint64_t player, std::vector<Net::EffectEvent>&, std::string& error);
        std::uint64_t effect_events = 0;
        bool take_audio_events(std::uint64_t player, std::vector<Net::AudioEvent>&, std::string& error);
        bool take_private_events(std::uint64_t player, std::vector<Net::PrivateEvent>&, std::string& error);
        std::uint64_t private_events = 0, subsumed_events = 0;
        bool leave(std::uint64_t player);
        bool clear_players();
        std::size_t player_count() const
        {
            return players_.size();
        }
        std::uint64_t world_id() const
        {
            return world_id_;
        }
        unsigned mode() const
        {
            return mode_;
        }
        const GameSession& game() const
        {
            return game_;
        }
        // NPC runtime failures suspend that actor, as in solo; they are reported
        // once per object and never stop the match for every player.
        std::vector<std::string> take_warnings()
        {
            return std::exchange(warnings_, {});
        }
        const Sha256Digest& fingerprint() const
        {
            return fingerprint_;
        }

      private:
        GameSession game_;
        Sha256Digest fingerprint_{};
        bool started_ = false;
        std::vector<std::string> warnings_;
        std::vector<ObjectId> reported_npcs_;
        struct Player
        {
            std::uint64_t id;
            HCLIENT client;
            ObjectId object;
            Net::MovementSetup setup;
            std::vector<Net::PrivateEvent> events;
            std::uint64_t next_event = 1;
            bool roster_failed = false;
            bool audio_started = false, audio_failed = false;
            bool effects_started = false, effects_failed = false;
            std::uint64_t next_effect = 1;
            std::vector<Net::EffectEvent> effects;
            std::size_t effect_bytes = 0;
            std::uint64_t next_audio = 1;
            std::vector<Net::AudioEvent> audio_events;
            std::set<SoundId> audio_voices;
            ObjectId camera_eye, camera_orb;
            std::optional<Net::CameraState> camera;
            std::optional<Net::WeaponLoadout> weapons;
            std::deque<std::variant<Net::PlayerCommand, Net::WeaponFrame>> pending_controls;
            std::size_t queued_commands = 0, queued_weapon_frames = 0;
            std::vector<Net::CommandAck> command_acks;
            std::uint64_t command_sequence = 0;
            struct WeaponState
            {
                std::uint64_t sequence = 0;
                Net::WeaponLoadout value;
            };
            std::deque<WeaponState> weapon_states;
            std::array<CViewWeapon, 2> weapon_timeline;
            std::array<std::uint64_t, 2> weapon_view_revision{}, fired{};
            std::uint64_t weapon_received = 0, weapon_received_state = 0, weapon_processed = 0, weapon_blocked = 0;
            std::uint64_t weapon_applied_state = 0;
            std::uint64_t weapon_sound_life_sequence = 0; // Last issued dead loadout; positional forces are separate.
            double weapon_time = 0;
            unsigned weapon_credit = 0;
            bool weapon_clock_started = false;
            std::deque<Net::MovementReport> pending;
            std::optional<Net::MovementAck> ack;
            // Sent but not yet acknowledged by a report: replayed steps of older
            // reports add these deltas (the client integrated them without).
            std::deque<Net::MovementImpulse> impulses;
            std::uint64_t impulse_sequence = 0, impulse_received = 0;
            PlayerControls controls;
            PlayerRotation aim;
            std::uint64_t source_revision = 0, pose_revision = 0;
            std::uint64_t received_sequence = 0, received_tick = 0, applied_sequence = 0;
            // Starts full: network jitter and the first reports may arrive in a burst.
            double movement_budget = movement_budget_limit;
            // Set once a movement error isolates this player; the endpoint then
            // removes the peer instead of stopping the server.
            std::string failure;
        };
        std::vector<Player> players_;
        struct StreamEmitter
        {
            ObjectId owner;
            ParticleStreamSettings settings;
        };
        struct ParticleEmitter
        {
            ObjectId owner;
            ObjectParticleRecipe recipe;
        };
        std::vector<ParticleEmitter> particle_emitters_;
        std::uint64_t next_particle_ = 1;
        void prune_particles();
        std::uint64_t capture_particle(ObjectId, std::string_view, unsigned, float, DVector, std::string&);
        std::vector<StreamEmitter> stream_emitters_;
        std::uint64_t next_stream_ = 1;
        void prune_streams();
        std::uint64_t capture_stream(ObjectId, const ParticleStreamSettings&, std::string&);
        struct AudioSource
        {
            ObjectId owner, speaker;
            bool private_player = false;
        };
        std::map<SoundId, AudioSource> audio_sources_;
        std::uint64_t world_id_ = 0, next_player_ = 1, next_snapshot_ = 1;
        std::uint64_t ticks_ = 0, total_ticks_ = 0;
        unsigned mode_ = 0;
        MultiplayerRules rules_;
        std::vector<std::string> rotation_;
        std::size_t map_index_ = 0;
        std::optional<std::uint64_t> ending_tick_;
        bool load_map(std::size_t index, std::string& error);
        void update_match();
        void capture_audio(const AudioVoice&, bool started);
        std::uint64_t capture_effect(ObjectId, std::string_view, const InstantEffectRecipe&, std::string&);
        bool queue_audio(Player&, const AudioVoice&, bool started);
        DRESULT capture_event(HCLIENT, const PlayerEvent&);
        DRESULT queue_private(Player&, ObjectId, const Net::PrivateValue&);
        void publish_roster(std::uint64_t excluded = 0);
        bool refresh_camera(Player&, Net::CameraState&, std::string& error);
        bool refresh_movement(Player&, std::string& error);
        bool collect_impulses(Player&, std::string& error);
        bool advance_players(double seconds, std::string& error);
        bool apply_controls(std::string& error);
        bool apply_command(Player&, const Net::PlayerCommand&, std::string& error);
        bool apply_weapon_frame(Player&, const Net::WeaponFrame&, std::string& error);
        bool advance_player(Player&, double seconds, std::string& error);
        bool replay_movement(Player&, const Net::MovementReport&, std::string& error);
    };
} // namespace OpenBlood2
