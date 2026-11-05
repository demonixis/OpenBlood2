#pragma once
#include "client/collision.h"
#include "client/movement_services.h"
#include <optional>

namespace OpenBlood2
{
    // One CMoveMgr substep before collision (see Net::MovementStep).
    struct ReplicaMoveStep
    {
        DVector offset{}, velocity{}, acceleration{};
        float seconds = 0;
        bool on_ground = false;
        DBYTE surface = 0;
    };
    struct ReplicaMoveReport
    {
        ClientObjectId owner;
        std::uint64_t force_revision = 0, sequence = 0;
        // Predicted position after the last step, with that step's pre-collision
        // velocity like the original WritePositionInfo.
        PlayerMove move;
        PlayerControls controls;
        DVector origin{};
        std::vector<ReplicaMoveStep> steps;
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
        bool advance(const PlayerMove&, DVector offset, float seconds) override;
        // Called once per real client frame, separately from capped physics time.
        // First-frame publication, then every 1/30 s or after report_steps
        // substeps (one frame adds at most 12), without a catch-up burst.
        bool report(double seconds, std::optional<ReplicaMoveReport>&);
        static constexpr std::size_t step_limit = 24, report_steps = 12;
        static constexpr float report_period = 1.0f / 30;
        struct Prediction
        {
            ClientMovement state;
            PlayerControls controls;
            DBYTE surface = 0;
            DVector velocity{}, origin{};
            std::vector<ReplicaMoveStep> steps;
        };
        Prediction prediction() const
        {
            return {state, controls_, surface_, velocity_, origin_, steps_};
        }
        bool restore_prediction(const Prediction&);
        bool stance(int character, float eye_level, bool crouched);
        // Reconciliation replays already reported frames without recording them
        // again; it re-records only the frames that are not reported yet.
        void record(bool value)
        {
            recording_ = value;
        }
        std::size_t pending_steps() const
        {
            return steps_.size();
        }

      private:
        const ClientCollisionWorld& collision_;
        std::uint64_t revision_ = 0, next_sequence_ = 0;
        DBYTE code_ = 0, surface_ = 0;
        int character_ = 0;
        float eye_level_ = 0, report_timer_ = 0;
        bool crouched_ = false, positioned_ = false, recording_ = true;
        PlayerControls controls_;
        DVector velocity_{}, origin_{};
        std::vector<ReplicaMoveStep> steps_;
    };
} // namespace OpenBlood2
