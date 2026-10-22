#include "engine/audio_stream.h"
#include <algorithm>
#include <chrono>
#include <cstring>
namespace OpenBlood2
{
    namespace
    {
        unsigned u16(std::span<const std::uint8_t> b, unsigned at)
        {
            return b[at] | (unsigned(b[at + 1]) << 8);
        }
        std::uint32_t u32(std::span<const std::uint8_t> b, unsigned at)
        {
            return u16(b, at) | (std::uint32_t(u16(b, at + 2)) << 16);
        }
    } // namespace
    std::shared_ptr<PcmStream> PcmStream::open(const Resource& resource, std::string& error, std::size_t window_phase)
    {
        error.clear();
        auto stream = std::make_shared<PcmStream>();
        stream->source_ = resource;
        stream->window_phase = window_phase % window_frames;
        if (resource.size < 12 || resource.size > 128 * 1024 * 1024)
        {
            error = "Streaming WAVE envelope exceeds 128 MiB or is truncated";
            return {};
        }
        stream->file_.open(resource.archive, std::ios::binary);
        if (!stream->file_ || !stream->validate(error))
        {
            if (error.empty())
                error = "Cannot open streaming resource archive";
            return {};
        }
        if (!stream->header(error))
            return {};
        return stream;
    }
    bool PcmStream::validate(std::string& out)
    {
        if (!error.empty())
        {
            out = error;
            return false;
        }
        std::error_code ec;
        auto size = std::filesystem::file_size(source_.archive, ec);
        if (ec || source_.offset > size || source_.size > size - source_.offset)
        {
            out = error = "Streaming resource archive changed/truncated";
            return false;
        }
        return true;
    }
    bool PcmStream::read(std::uint64_t offset, std::span<std::uint8_t> bytes, std::string& out)
    {
        struct Cost
        {
            std::uint64_t& total;
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            ~Cost()
            {
                total += std::uint64_t(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin)
                        .count());
            }
        } cost{read_ns};
        if (!validate(out))
            return false;
        if (offset > source_.size || bytes.size() > source_.size - offset)
        {
            out = error = "Streaming read exceeds resource slice";
            return false;
        }
        file_.clear();
        file_.seekg(std::uint64_t(source_.offset) + offset);
        if (!file_ || !file_.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size())))
        {
            out = error = "Streaming resource read failed";
            return false;
        }
        bytes_read += bytes.size();
        ++reads;
        return true;
    }
    bool PcmStream::header(std::string& error)
    {
        auto fail = [&](const char* message)
        {
            error = message;
            return false;
        };
        std::array<std::uint8_t, 16> bytes{};
        if (!read(0, std::span(bytes).first(12), error))
            return false;
        if (std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
            return fail("Invalid streaming RIFF WAVE signature");
        const auto riff_end = std::uint64_t(u32(bytes, 4)) + 8;
        if (riff_end != source_.size && riff_end + 1 != source_.size)
            return fail("Invalid streaming RIFF envelope");
        bool seen_fmt = false, seen_data = false, final_odd_data = false;
        std::uint32_t data_size = 0;
        unsigned chunks = 0;
        for (std::uint64_t pos = 12; pos < source_.size;)
        {
            if (++chunks > 1024 || source_.size - pos < 8)
                return fail("Invalid streaming WAVE chunk list");
            if (!read(pos, std::span(bytes).first(8), error))
                return false;
            const auto n = u32(bytes, 4);
            const bool data = !std::memcmp(bytes.data(), "data", 4), fmt = !std::memcmp(bytes.data(), "fmt ", 4);
            const bool list = !std::memcmp(bytes.data(), "LIST", 4);
            loop_metadata |= !std::memcmp(bytes.data(), "cue ", 4) || !std::memcmp(bytes.data(), "smpl", 4);
            const auto payload_end = pos + 8 + n;
            final_odd_data = data && (n & 1) && (payload_end == source_.size || payload_end + 1 == source_.size);
            const auto end = payload_end + ((final_odd_data && payload_end == source_.size) ? 0 : (n & 1));
            if (end > source_.size)
                return fail("Truncated streaming WAVE chunk/padding");
            if (list && n >= 4)
            {
                if (!read(pos + 8, std::span(bytes).first(4), error))
                    return false;
                loop_metadata |= !std::memcmp(bytes.data(), "adtl", 4);
            }
            if (fmt)
            {
                if (seen_fmt || n < 16)
                    return fail("Duplicate/truncated streaming WAVE format");
                seen_fmt = true;
                if (!read(pos + 8, bytes, error))
                    return false;
                channels = u16(bytes, 2);
                rate = u32(bytes, 4);
                block_ = u16(bytes, 12);
                bits = u16(bytes, 14);
                if (u16(bytes, 0) != 1)
                    return fail("Streaming supports PCM8/16 only; compressed streaming is unavailable");
                if (channels < 1 || channels > 2 || rate < 8000 || rate > 96000 || (bits != 8 && bits != 16) ||
                    block_ != channels * (bits / 8) || u32(bytes, 8) != rate * block_)
                    return fail("Invalid streaming PCM layout");
            }
            if (data)
            {
                if (seen_data || !n)
                    return fail("Duplicate/empty streaming WAVE data");
                seen_data = true;
                data_offset_ = std::uint32_t(pos + 8);
                data_size = n;
            }
            pos = end;
        }
        if (riff_end != source_.size && !final_odd_data)
            return fail("Streaming RIFF length differs outside terminal odd data padding");
        if (!seen_fmt || !seen_data || data_size % block_)
            return fail("Missing/unaligned streaming PCM data");
        frames = data_size / block_;
        return frames != 0;
    }
    bool PcmStream::sample(std::size_t frame, unsigned channel, float& value, std::string& out)
    {
        if (frame >= frames || channel >= channels)
        {
            out = "Streaming sample out of range";
            return false;
        }
        if (!error.empty())
        {
            out = error;
            return false;
        }
        Window* window = nullptr;
        for (auto& w : windows_)
            if (frame >= w.begin && frame - w.begin < w.count)
            {
                window = &w;
                break;
            }
        if (!window)
        {
            auto& w = windows_[next_window_];
            next_window_ ^= 1;
            const auto end = frame < window_phase
                                 ? window_phase
                                 : window_phase + ((frame - window_phase) / window_frames + 1) * window_frames;
            w.begin = frame < window_phase ? 0 : end - window_frames;
            w.count = 0;
            const auto count = std::min(end, frames) - w.begin;
            if (!read(std::uint64_t(data_offset_) + w.begin * block_, std::span(w.bytes).first(count * block_), out))
                return false;
            w.count = count;
            ++refills;
            window = &w;
        }
        auto at = (frame - window->begin) * block_ + channel * (bits / 8);
        if (bits == 8)
            value = (int(window->bytes[at]) - 128) / 128.0f;
        else
        {
            const auto raw = u16(window->bytes, unsigned(at));
            const int signed_sample = raw < 32768 ? int(raw) : int(raw) - 65536;
            value = signed_sample / 32768.0f;
        }
        return true;
    }
} // namespace OpenBlood2
