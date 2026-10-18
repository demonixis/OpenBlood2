#pragma once
#include "engine/ima_playlist.h"

namespace OpenBlood2
{
    // Bounded repetition of a prepared sequence. Intro sections play once; the
    // suffix repeats. Pattern/variation regeneration and live switches are separate.
    class ImaLoopTransport
    {
      public:
        bool start(std::span<const ImaSectionProgram>, std::size_t loop_begin, std::shared_ptr<const DlsBank>,
                   DlsSampler&, double gain, std::string& error);
        // Schedule before the mixer advances through this absolute sample horizon.
        // Admission is atomic. Failure leaves this transport and the sampler intact.
        bool pump(DlsSampler&, std::uint64_t horizon, std::string& error);
        void stop(DlsSampler&);
        bool active() const
        {
            return active_;
        }
        std::uint64_t passes_submitted() const
        {
            return passes_;
        }
        std::uint64_t origin() const
        {
            return first_clock_.origin_frame();
        }
        std::uint64_t next_submission() const
        {
            return next_due_;
        }
        std::size_t template_events() const
        {
            return first_.events.size() + loop_.events.size();
        }
        // Exact repeated-map conversion; useful for inspecting boundaries/tails.
        bool sample(std::int64_t tick, std::uint64_t& frame, std::string& error) const;
        std::int64_t first_end_tick() const
        {
            return first_end_;
        }
        std::int32_t loop_ticks() const
        {
            return loop_.duration_ticks;
        }

      private:
        ImaPlaylistProgram first_, loop_;
        ImaClock first_clock_, loop_clock_;
        std::shared_ptr<const DlsBank> bank_;
        std::int64_t first_end_ = 0;
        std::int32_t loop_earliest_ = 0;
        std::uint64_t first_time_ = 0, loop_time_ = 0, passes_ = 0, next_due_ = 0;
        double gain_ = 0;
        bool active_ = false;
    };
} // namespace OpenBlood2
