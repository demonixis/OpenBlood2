#pragma once
#include "client/camera.h"
#include "net/player_events.h"

namespace OpenBlood2
{
    class NetworkCamera
    {
      public:
        enum class Reception
        {
            Applied,
            Ignored,
            Deferred,
            Refused
        };
        bool begin(const Net::Joined&);
        Reception receive(const Net::PrivateEvent&, std::string& error);
        // The caller supplies its mapped world clock and sampled remote poses;
        // local position stays predicted. No server object or callback is used.
        Reception update(const ClientWorld&, const ClientCollisionWorld&, const ClientMovement&,
                         std::uint64_t force_revision, float pitch, float yaw, bool zoom, double time,
                         std::string& error);
        ClientCamera& camera()
        {
            return camera_;
        }
        const ClientCamera& camera() const
        {
            return camera_;
        }
        bool ready() const
        {
            return camera_.frame().sequence != 0;
        }

      private:
        Net::Joined owner_;
        std::optional<Net::CameraState> state_;
        ClientCamera camera_;
        std::uint64_t sequence_ = 0;
        double time_ = 0;
    };
} // namespace OpenBlood2
