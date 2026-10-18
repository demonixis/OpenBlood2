#include "engine/ima_transport.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace OpenBlood2
{
    bool ImaLoopTransport::sample(std::int64_t tick, std::uint64_t& frame, std::string& error) const
    {
        frame = 0;
        if (tick < 0 || loop_.duration_ticks <= 0)
        {
            error = "Unprepared IMA repeat clock or negative tick";
            return false;
        }
        if (tick < first_end_)
            return first_clock_.sample(std::int32_t(tick), frame, error);
        const auto delta = std::uint64_t(tick - first_end_);
        const auto cycles = delta / loop_.duration_ticks;
        if (cycles > (UINT64_MAX - first_time_) / loop_time_)
        {
            error = "IMA repeat clock overflow";
            return false;
        }
        return loop_clock_.sample(std::int32_t(delta % loop_.duration_ticks), frame, error,
                                  first_time_ + cycles * loop_time_);
    }

    bool ImaLoopTransport::start(std::span<const ImaSectionProgram> sections, std::size_t loop_begin,
                                 std::shared_ptr<const DlsBank> bank, DlsSampler& sampler, double gain,
                                 std::string& error)
    {
        error.clear();
        if (active_ || !sampler.pending_midi().empty() || !sampler.voices().empty() || !bank || !std::isfinite(gain) ||
            gain < 0 || gain > 1 || loop_begin >= sections.size())
        {
            error = "IMA repeat requires an idle sampler, bank, valid gain and nonempty loop suffix";
            return false;
        }
        ImaLoopTransport next;
        if (!prepare_ima_playlist(sections, next.first_, error) ||
            !prepare_ima_playlist(sections.subspan(loop_begin), next.loop_, error))
            return false;
        // Remove the suffix's local preroll. Every repetition shares the first
        // pass's epoch; anticipated notes use the preceding section's tempo.
        for (auto& event : next.loop_.events)
            event.time -= next.loop_.preroll_ticks;
        for (std::size_t i = 1; i < next.loop_.tempos.size(); ++i)
            next.loop_.tempos[i].tick -= next.loop_.preroll_ticks;
        next.loop_earliest_ = std::min_element(next.loop_.events.begin(), next.loop_.events.end(),
                                               [](auto a, auto b) { return a.time < b.time; })
                                  ->time;
        const auto origin = sampler.sample_clock();
        next.first_end_ = std::int64_t(next.first_.duration_ticks) + next.first_.preroll_ticks;
        if (next.first_end_ > INT32_MAX || !next.first_clock_.prepare(next.first_.tempos, origin, error) ||
            !next.loop_clock_.prepare(next.loop_.tempos, origin, error) ||
            !next.first_clock_.time_at(std::int32_t(next.first_end_), next.first_time_, error) ||
            !next.loop_clock_.time_at(next.loop_.duration_ticks, next.loop_time_, error))
            return false;
        // Positive authored durations/tempos imply a strictly positive period.
        next.bank_ = std::move(bank);
        next.gain_ = gain;
        next.active_ = true;
        next.next_due_ = origin;
        std::uint64_t end;
        if (!next.sample(next.first_end_, end, error) || !next.pump(sampler, origin, error))
            return false;
        *this = std::move(next);
        return true;
    }

    bool ImaLoopTransport::pump(DlsSampler& sampler, std::uint64_t horizon, std::string& error)
    {
        error.clear();
        if (!active_)
            return true;
        if (horizon < sampler.sample_clock() || sampler.sample_clock() < origin())
        {
            error = "IMA repeat horizon precedes its audio clock";
            return false;
        }
        auto passes = passes_, due = next_due_;
        std::vector<DlsTimedMidi> messages;
        unsigned batches = 0;
        while (due <= horizon)
        {
            const auto& events = passes ? loop_.events : first_.events;
            if (++batches > 128 || events.size() > DlsSampler::max_pending_midi - messages.size() ||
                passes > std::uint64_t((INT64_MAX - first_end_ - INT32_MAX) / loop_.duration_ticks))
            {
                error = "IMA repeat scheduling work/message/tick limit";
                return false;
            }
            const auto offset = passes ? first_end_ + std::int64_t(passes - 1) * loop_.duration_ticks : 0;
            for (const auto& event : events)
            {
                std::uint64_t frame;
                if (!sample(offset + event.time, frame, error))
                    return false;
                messages.push_back({frame, event.status, event.key, event.velocity});
            }
            ++passes;
            if (!sample(first_end_ + std::int64_t(passes - 1) * loop_.duration_ticks + loop_earliest_, due, error))
                return false;
        }
        if (!messages.empty() && !sampler.enqueue(bank_, messages, gain_, error))
            return false;
        passes_ = passes;
        next_due_ = due;
        return true;
    }

    void ImaLoopTransport::stop(DlsSampler& sampler)
    {
        active_ = false;
        bank_.reset();
        sampler.silence(); // Instrument-only developer cancellation; never reset the shared clock.
    }
} // namespace OpenBlood2
