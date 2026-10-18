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
        if (!next.prepare(sections, loop_begin, std::move(bank), sampler.sample_clock(), gain, error) ||
            !next.pump(sampler, sampler.sample_clock(), error))
            return false;
        *this = std::move(next);
        return true;
    }

    bool ImaLoopTransport::prepare(std::span<const ImaSectionProgram> sections, std::size_t loop_begin,
                                   std::shared_ptr<const DlsBank> bank, std::uint64_t origin, double gain,
                                   std::string& error)
    {
        if (!bank || !std::isfinite(gain) || gain < 0 || gain > 1 || loop_begin >= sections.size())
        {
            error = "Invalid IMA replacement bank/gain/loop suffix";
            return false;
        }
        auto& next = *this; // Private staging object, never the active transport.
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
        return next.sample(next.first_end_, end, error);
    }

    bool ImaLoopTransport::pump(DlsSampler& sampler, std::uint64_t horizon, std::string& error)
    {
        error.clear();
        if (!active_)
            return true;
        if (horizon < sampler.sample_clock())
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
                if (!replacement_ || frame < replacement_->origin())
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

    bool ImaLoopTransport::next_song(std::uint64_t now, std::uint64_t& frame, std::string& error) const
    {
        error.clear();
        frame = 0;
        if (!active_ || now < origin())
        {
            error = "No active IMA song at this clock";
            return false;
        }
        for (const auto& part : first_.parts)
        {
            if (!sample(std::int64_t(part.start_tick) + part.duration, frame, error))
                return false;
            if (frame > now)
                return true;
        }
        // Locate the repeat containing now without walking every elapsed song.
        std::uint64_t low = 0, high = std::uint64_t(INT32_MAX) * 10000 / loop_time_ + 1;
        while (low < high)
        {
            const auto middle = low + (high - low) / 2;
            if (!sample(first_end_ + std::int64_t(middle) * loop_.duration_ticks, frame, error) || frame > now)
                high = middle;
            else
                low = middle + 1;
        }
        error.clear();
        const auto offset = first_end_ + std::int64_t(low ? low - 1 : 0) * loop_.duration_ticks;
        for (const auto& part : loop_.parts)
        {
            if (!sample(offset + part.start_tick - loop_.preroll_ticks + part.duration, frame, error))
                return false;
            if (frame > now)
                return true;
        }
        error = "IMA next-song boundary exceeds the supported clock";
        frame = 0;
        return false;
    }

    bool ImaLoopTransport::request(std::span<const ImaSectionProgram> sections, std::size_t loop_begin,
                                   std::shared_ptr<const DlsBank> bank, DlsSampler& sampler, double gain,
                                   bool immediate, std::uint8_t level, std::string& error)
    {
        error.clear();
        auto boundary = sampler.sample_clock();
        if (!immediate && active_ && !next_song(boundary, boundary, error))
            return false;
        auto next = std::make_unique<ImaLoopTransport>();
        if (!next->prepare(sections, loop_begin, std::move(bank), boundary, gain, error))
            return false;
        // Preflight message conversion/admission before cancelling any live audio.
        DlsSampler check;
        if (!next->pump(check, boundary, error))
            return false;
        next->passes_ = 0;
        next->next_due_ = boundary;
        next->level_ = level;
        replacement_ = std::move(next);
        sampler.discard_midi_from(boundary); // Once per accepted request, never scan the queue every audio frame.
        if (boundary == sampler.sample_clock())
            return advance(sampler, 0, {}, error);
        return true;
    }

    bool ImaLoopTransport::advance(DlsSampler& sampler, std::uint64_t frames, std::span<float> stereo,
                                   std::string& error)
    {
        error.clear();
        const auto start = sampler.sample_clock();
        if (frames > DlsSampler::rate * 60 + 1 || frames > UINT64_MAX - start || stereo.size() % 2 ||
            stereo.size() / 2 > frames || stereo.size() > 24000)
        {
            error = "Invalid IMA audio step";
            return false;
        }
        if (!active_ && !replacement_)
        {
            const bool played = sampler.advance(frames, stereo); // Preserve standalone audition failure semantics.
            if (!played)
                error =
                    sampler.last_dispatch_error.empty() ? "Invalid instrument audio step" : sampler.last_dispatch_error;
            return played;
        }
        const auto end = start + frames;
        if (replacement_ && replacement_->origin() < start)
        {
            error = "IMA replacement deadline was bypassed outside the mixer";
            stop(sampler);
            sampler.advance(frames, {});
            return false;
        }
        auto step = [&](std::uint64_t until)
        {
            const auto offset = std::min<std::uint64_t>(sampler.sample_clock() - start, stereo.size() / 2);
            const auto count = std::min<std::uint64_t>(until - sampler.sample_clock(), stereo.size() / 2 - offset);
            const bool queued = pump(sampler, until, error);
            if (!queued)
                stop(sampler);
            const bool played = sampler.advance(until - sampler.sample_clock(), stereo.subspan(offset * 2, count * 2));
            if (!played && error.empty())
                error = sampler.last_dispatch_error;
            return queued && played;
        };
        if (replacement_ && replacement_->origin() <= end)
        {
            const auto boundary = replacement_->origin();
            if (!step(boundary))
            {
                stop(sampler);
                sampler.advance(end - sampler.sample_clock(), {});
                return false;
            }
            auto next = std::move(replacement_);
            next->switches_ = switches_ + 1;
            sampler.silence(); // Cut old voices/tails and future events at the exact boundary.
            *this = std::move(*next);
        }
        if (!step(end))
        {
            stop(sampler);
            return false;
        }
        return true;
    }

    void ImaLoopTransport::stop(DlsSampler& sampler)
    {
        active_ = false;
        replacement_.reset();
        bank_.reset();
        sampler.silence(); // Instrument-only developer cancellation; never reset the shared clock.
    }
} // namespace OpenBlood2
