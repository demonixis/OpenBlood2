#pragma once
#include "client/replica_movement.h"
#include "game/original/MoveMgr.h"
#include "net/admission.h"
#include "net/movement.h"
#include <deque>

namespace OpenBlood2
{
    // Owns only client prediction. Delayed corrections restore a checked report
    // checkpoint and replay later client frames, never server objects/callbacks.
    class NetworkMovement
    {
      public:
        enum class Reception
        {
            Applied,
            Ignored,
            Deferred,
            Refused
        };
        explicit NetworkMovement(const ClientCollisionWorld& collision)
            : movement_(collision), manager_(movement_), collision_(collision)
        {
        }
        bool begin(const Net::Joined&);
        Reception receive(const Net::MovementSetup&, std::string& error);
        Reception receive(const Net::MovementAck&, std::string& error);
        Reception receive(const Net::MovementImpulse&, std::string& error);
        std::uint64_t impulse_sequence() const
        {
            return impulse_sequence_;
        }
        bool update(const MovementInput&, float yaw, float pitch, double seconds, bool commands, bool firing,
                    bool external_camera, std::optional<Net::MovementReport>&, std::string& error);
        bool ready() const
        {
            return setup_.sequence && manager_.ready();
        }
        const CMoveMgr& manager() const
        {
            return manager_;
        }
        const ClientMovement& movement() const
        {
            return movement_.state;
        }
        const Net::MovementSetup& setup() const
        {
            return setup_;
        }
        std::uint64_t corrections = 0, replays = 0, ignored = 0, deferred = 0;
        // Frames whose prediction failed and held the body (see update).
        std::uint64_t prediction_holds = 0;
        static constexpr std::size_t frame_limit = 512, report_limit = 64;
        // Consecutive failed frames tolerated before the prediction is refused.
        static constexpr unsigned hold_limit = 240;

      private:
        struct Frame
        {
            std::uint64_t id;
            MovementInput input;
            float yaw, pitch;
            double seconds;
            bool commands, firing, external_camera;
        };
        // A server velocity delta, applied before frame `frame`. An ACK whose
        // server state predates it replays it at the same frame.
        struct Impulse
        {
            std::uint64_t sequence, frame;
            DVector velocity;
        };
        struct Checkpoint
        {
            std::uint64_t report, frame;
            CMoveMgrCheckpoint manager;
            ReplicaMovement::Prediction prediction;
        };
        ReplicaMovement movement_;
        CMoveMgr manager_;
        const ClientCollisionWorld& collision_;
        Net::Joined joined_;
        Net::MovementSetup setup_;
        std::optional<Net::MovementAck> future_ack_;
        std::deque<Frame> frames_;
        std::deque<Checkpoint> reports_;
        std::deque<Impulse> impulses_;
        std::uint64_t impulse_sequence_ = 0;
        std::uint64_t next_frame_ = 0, sent_ = 0, acknowledged_ = 0, retired_ = 0;
        // Frame of the latest sent report: later frames hold the unsent steps.
        std::uint64_t sent_frame_ = 0;
        unsigned failed_frames_ = 0;
        bool belongs(const Net::MovementIdentity&) const;
        void clear_history();
        void trim();
    };
} // namespace OpenBlood2
