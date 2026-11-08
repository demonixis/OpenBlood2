#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

namespace OpenBlood2
{
    struct HudSurface;

    struct SmackerInfo
    {
        unsigned width = 0, height = 0, frames = 0;
        unsigned audio_rate = 0, audio_channels = 0;
        double frame_seconds = 0;
    };

    // Sequential decoder for the SMK2 clips supplied with retail Blood II.
    class SmackerDecoder
    {
      public:
        SmackerDecoder();
        ~SmackerDecoder();
        SmackerDecoder(const SmackerDecoder&) = delete;
        SmackerDecoder& operator=(const SmackerDecoder&) = delete;
        bool open(const std::filesystem::path&, std::string& error);
        void close();
        // False with an empty error marks normal end of file. The first call decodes frame zero.
        bool next(std::string& error);
        const SmackerInfo& info() const;
        const HudSurface& video() const;
        // Native signed 16-bit interleaved samples contributed by the current frame.
        std::span<const std::int16_t> audio() const;
        unsigned decoded_frames() const;

      private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace OpenBlood2
