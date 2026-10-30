#pragma once
#include "client/collision.h"
#include "client/movement_services.h"
#include <optional>

namespace OpenBlood2
{
    struct ReplicaMoveReport
    {
        ClientObjectId owner;
        std::uint64_t force_revision = 0, sequence = 0;
        PlayerMove move;
        PlayerControls controls;
    };
    // Prediction against an already admitted collision replica. This has no
    // original object, server shell, audio device, or transport dependency.
    class ReplicaMovement final : public MovementServices
    {
      public:
        explicit ReplicaMovement(const ClientCollisionWorld& collision) : collision_(collision) {}
        bool bind(ClientObjectId, std::uint64_t force_revision, DBYTE code, int character, float eye_level,
                  bool crouched);
        ClientMovement state;
        ClientMotionState& body() override
        {
            return state.body;
        }
        void clear() override;
        bool valid() const override;
        bool synchronize() override;
        bool force_position(const PlayerPosition&) override;
        bool controls(const PlayerControls&, bool send, bool& crouched) override;
        bool environment(ClientContainerState&, int& character) override;
        bool finish(float seconds, ClientContainerState&) override;
        bool jump_voice(int, bool) override
        {
            return false;
        }
        bool advance(const PlayerMove&) override;
        // Called once per real client frame, separately from capped physics time.
        // Original 0.14 s countdown, first-frame publication, no catch-up burst.
        bool report(double seconds, std::optional<ReplicaMoveReport>&);
        struct Prediction
        {
            ClientMovement state;
            PlayerControls controls;
            DBYTE surface = 0;
        };
        Prediction prediction() const
        {
            return {state, controls_, surface_};
        }
        bool restore_prediction(const Prediction&);
        bool stance(int character, float eye_level, bool crouched);

      private:
        const ClientCollisionWorld& collision_;
        std::uint64_t revision_ = 0, next_sequence_ = 0;
        DBYTE code_ = 0, surface_ = 0;
        int character_ = 0;
        float eye_level_ = 0, report_timer_ = 0;
        bool crouched_ = false, positioned_ = false;
        PlayerControls controls_;
    };
} // namespace OpenBlood2
