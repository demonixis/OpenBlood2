#pragma once
#include "client/network_camera.h"
#include "client/network_clock.h"
#include "client/network_input.h"
#include "client/network_movement.h"
#include "client/network_player_events.h"
#include "client/network_weapons.h"
#include "client/network_world.h"
#include "game/original/ClientCrosshair.h"
#include "net/content.h"
#include "net/player_commands.h"
#include "net/transport.h"
#include <chrono>
namespace OpenBlood2
{
    // One admission per lifetime. A reconnect constructs a new endpoint and gets
    // a new server identity. Never constructs GameSession or gameplay objects.
    class NetworkClient
    {
      public:
        using Clock = std::chrono::steady_clock;
        enum class Phase
        {
            Idle,
            Connecting,
            Offer,
            Baseline,
            Active,
            Closing,
            Closed,
            Failed
        };
        NetworkClient() : models_(resources_), sprites_(resources_), movement_(receiver_.collision()) {}
        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;
        bool connect(const Config&, std::string_view host, std::uint16_t port, const Net::JoinProfile&,
                     Clock::time_point, std::string& error);
        bool update(Clock::time_point, const MovementInput&, bool commands, std::string& error);
        bool look(float pitch_delta, float yaw_delta);
        bool command(const Net::CommandAction&, std::string& error);
        std::uint64_t command_acks = 0, commands_applied = 0;
        bool disconnect(Clock::time_point);
        Phase phase() const
        {
            return phase_;
        }
        bool ready() const
        {
            return phase_ == Phase::Active && camera_.ready() && weapons_.ready(movement_.setup().owner.force_revision);
        }
        const std::string& reason() const
        {
            return reason_;
        }
        const Resources& resources() const
        {
            return resources_;
        }
        const World& geometry() const
        {
            return geometry_;
        }
        const Net::WorldOffer& offer() const
        {
            return offer_;
        }
        const Net::Joined& joined() const
        {
            return joined_;
        }
        const ClientWorld& world() const
        {
            return displayed_;
        }
        const NetworkWorldReceiver& receiver() const
        {
            return receiver_;
        }
        const NetworkMovement& movement() const
        {
            return movement_;
        }
        const ClientCamera& camera() const
        {
            return camera_.camera();
        }
        ClientCamera& camera()
        {
            return camera_.camera();
        }
        const NetworkWeapons& weapons() const
        {
            return weapons_;
        }
        const ClientHud& hud() const
        {
            return inbox_.hud();
        }
        void take_events(std::vector<Net::PrivateEvent>& events)
        {
            inbox_.take(events);
        }
        std::uint64_t event_sequence() const
        {
            return inbox_.sequence();
        }
        float pitch() const
        {
            return pitch_;
        }
        float yaw() const
        {
            return yaw_;
        }
        double time() const
        {
            return mapped_time_;
        }
        bool zoomed() const
        {
            return zoom_;
        }
        DBYTE zoom_crosshair() const
        {
            return zoom_ ? zoom_crosshair_ : 0;
        }
        float fov_x() const;
        std::uint64_t reports = 0, state_packets = 0, discarded_ticks = 0;

      private:
        Resources resources_;
        ModelStore models_;
        SpriteStore sprites_;
        World geometry_;
        Net::Transport transport_;
        Net::PeerId peer_ = 0;
        Sha256Digest digest_;
        Net::JoinProfile profile_;
        Net::WorldOffer offer_;
        Net::Joined joined_;
        std::optional<Net::BaselineReceiver> baseline_;
        std::optional<Net::WorldSnapshot> initial_;
        std::optional<Net::MovementSetup> setup_;
        NetworkWorldReceiver receiver_;
        NetworkMovement movement_;
        NetworkCamera camera_;
        NetworkWeapons weapons_;
        NetworkPlayerEvents inbox_;
        NetworkClock clock_;
        ClientWorld displayed_;
        Phase phase_ = Phase::Idle;
        Clock::time_point origin_, last_, deadline_;
        NetworkInput input_;
        double mapped_time_ = 0;
        float pitch_ = 0, yaw_ = 0;
        bool zoom_ = false;
        DBYTE zoom_crosshair_ = 0;
        float fov_x_ = original_fov_normal;
        std::string reason_;
        std::uint64_t command_sequence_ = 0;
        std::vector<Net::PlayerCommand> pending_commands_;
        bool send(const Net::AdmissionMessage&, std::string&);
        bool receive(const Net::Event&, Clock::time_point, std::string&);
        bool complete_baseline(Clock::time_point, std::string&);
        bool observe(double server, Clock::time_point);
        bool fail(std::string&, const char* fallback);
    };
} // namespace OpenBlood2
