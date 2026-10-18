#include "engine/dls_sampler.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace OpenBlood2
{
    namespace
    {
        bool is_note(std::uint8_t status)
        {
            return (status & 0xf0) == 0x80 || (status & 0xf0) == 0x90;
        }
        unsigned control_index(const DlsTimedMidi& m)
        {
            if ((m.status & 0xf0) == 0xc0)
                return 0;
            if ((m.status & 0xf0) == 0xe0)
                return 1;
            return m.first == 7 ? 2 : m.first == 10 ? 3 : 4;
        }
    } // namespace
    bool DlsSampler::enqueue(std::shared_ptr<const DlsBank> bank, std::span<const DlsTimedMidi> messages, double gain,
                             std::string& error)
    {
        error.clear();
        auto fail = [&](std::string reason)
        {
            error = last_queue_error = std::move(reason);
            ++queue_rejected;
            return false;
        };
        if (messages.size() > max_pending_midi || !std::isfinite(gain) || gain < 0 || gain > 1)
            return fail("Invalid MIDI batch size or gain");
        auto banks = pending_banks_;
        std::vector<DlsQueuedMidi> added;
        added.reserve(messages.size());
        for (const auto& m : messages)
        {
            const unsigned kind = m.status & 0xf0, slot = m.status & 15;
            if (m.frame < sample_clock_)
                return fail("MIDI message precedes the current sample boundary");
            if (kind != 0xe0 && (m.first > 127 || (kind != 0xc0 && m.second > 127)))
                return fail("Invalid scheduled MIDI data byte");
            if (kind == 0xb0 && (m.first == 0 || m.first == 32))
            {
                // Original bank selectors latch immediately; only a subsequent program queues a patch.
                banks[slot][m.first == 0 ? 0 : 1] = m.second;
                continue;
            }
            if (!is_note(m.status) && kind != 0xc0)
            {
                DlsControllers check;
                if (!apply_dls_controller(check, m.status, m.first, m.second, error))
                    return fail(error);
            }
            const bool note_on = kind == 0x90 && m.second != 0;
            if (note_on && !bank)
                return fail("Scheduled note-on has no immutable DLS bank");
            if (added.size() >= max_pending_midi - pending_midi_.size())
                return fail("MIDI pending-message budget exceeded");
            added.push_back({m, note_on ? bank : nullptr, gain, banks[slot][0], banks[slot][1]});
        }
        // Stable sorting also retains ties between earlier batches and this batch. Source input
        // need not be time-ordered: original IMA notes and curves can run beyond their current grid.
        auto next = pending_midi_;
        next.insert(next.end(), added.begin(), added.end());
        std::stable_sort(next.begin(), next.end(),
                         [](const auto& a, const auto& b) { return a.message.frame < b.message.frame; });
        pending_midi_ = std::move(next);
        pending_banks_ = banks;
        queued_messages += messages.size();
        last_queue_error.clear();
        return true;
    }
    bool DlsSampler::advance(std::uint64_t frames, std::span<float> stereo)
    {
        if (frames > rate * 60 + 1 || stereo.size() % 2 || stereo.size() / 2 > frames || stereo.size() > 24000 ||
            frames > std::numeric_limits<std::uint64_t>::max() - sample_clock_)
            return false;
        const auto start = sample_clock_, end = start + frames;
        auto render_until = [&](std::uint64_t boundary)
        {
            const auto offset = std::min<std::uint64_t>(sample_clock_ - start, stereo.size() / 2);
            const auto count = std::min<std::uint64_t>(boundary - sample_clock_, stereo.size() / 2 - offset);
            advance_voices(boundary - sample_clock_, stereo.subspan(offset * 2, count * 2));
            sample_clock_ = boundary;
        };
        bool success = true;
        std::size_t consumed = 0;
        while (consumed < pending_midi_.size() && pending_midi_[consumed].message.frame <= end)
        {
            const auto boundary = pending_midi_[consumed].message.frame;
            render_until(boundary);
            auto limit = consumed;
            while (limit < pending_midi_.size() && pending_midi_[limit].message.frame == boundary)
                ++limit;
            // Original per-control timelines return the last value <= the note/render time.
            // Coalesce same-control ties before applying any notes, regardless of submission order.
            std::array<std::array<std::size_t, 5>, 16> last;
            for (auto& channel : last)
                channel.fill(limit);
            for (auto i = consumed; i < limit; ++i)
            {
                const auto& m = pending_midi_[i].message;
                if (!is_note(m.status))
                    last[m.status & 15][control_index(m)] = i;
            }
            auto deliver = [&](const DlsQueuedMidi& q)
            {
                const auto& m = q.message;
                ++dispatched_messages;
                std::string error;
                if (!receive(q.bank, m.status, m.first, m.second, q.gain, error))
                {
                    ++dispatch_failures;
                    failed_dispatch_frame = boundary;
                    last_dispatch_error = std::move(error);
                    success = false;
                }
                else if ((m.status & 0xf0) == 0xc0)
                {
                    // The program captured the submission-time bank, not a later bank selector.
                    auto& channel = midi_channels_[m.status & 15];
                    channel.bank_msb = q.bank_msb;
                    channel.bank_lsb = q.bank_lsb;
                }
            };
            for (auto i = consumed; i < limit; ++i)
            {
                const auto& m = pending_midi_[i].message;
                if (is_note(m.status))
                    continue;
                if (last[m.status & 15][control_index(m)] != i)
                    ++coalesced_messages;
                else
                    deliver(pending_midi_[i]);
            }
            for (auto i = consumed; i < limit; ++i)
                if (is_note(pending_midi_[i].message.status))
                    deliver(pending_midi_[i]);
            consumed = limit;
        }
        render_until(end);
        pending_midi_.erase(pending_midi_.begin(), pending_midi_.begin() + consumed);
        return success; // Failed messages are consumed once; time and later events still progress.
    }
} // namespace OpenBlood2
