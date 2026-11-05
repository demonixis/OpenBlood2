#pragma once
#include "game/original/ViewWeapon.h"
#include "net/player_events.h"
namespace OpenBlood2
{
    class NetworkWeapons
    {
      public:
        enum class Reception
        {
            Applied,
            Ignored,
            Refused
        };
        bool begin(const Net::Joined&);
        Reception receive(const Net::PrivateEvent&, ModelStore&, Resources&, std::string& error);
        // One client simulation tick, never a renderer update or movement replay.
        // Inputs predict animation only; NetworkClient sends the corresponding frame.
        // Actual animation sound output remains a separate consumer.
        bool update(std::uint64_t force_revision, double seconds, std::string& error, WeaponInput input = {},
                    PlayerRotation aim = {});
        bool ready(std::uint64_t force_revision) const
        {
            return state_ && state_->force_revision == force_revision;
        }
        const std::array<CViewWeapon, 2>& views() const
        {
            return views_;
        }
        const std::optional<Net::WeaponLoadout>& state() const
        {
            return state_;
        }
        double time() const
        {
            return time_;
        }
        std::uint64_t state_sequence() const
        {
            return sequence_;
        }
        std::uint64_t updates = 0, sound_requests = 0;

      private:
        Net::Joined owner_;
        std::uint64_t sequence_ = 0;
        std::optional<Net::WeaponLoadout> state_;
        std::array<CViewWeapon, 2> views_;
        double time_ = 0;
    };
} // namespace OpenBlood2
