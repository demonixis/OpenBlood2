#pragma once
#include "engine/resources.h"
#include <array>
#include <fstream>
#include <memory>

namespace OpenBlood2
{
    // PCM-only REZ slice reader. No resource payload or decoded clip is retained.
    class PcmStream
    {
      public:
        static constexpr std::size_t window_frames = 4096, buffer_bytes = window_frames * 4 * 2;
        static std::shared_ptr<PcmStream> open(const Resource&, std::string&);
        bool sample(std::size_t frame, unsigned channel, float&, std::string&);
        bool validate(std::string&);
        unsigned rate = 0, channels = 0, bits = 0;
        std::size_t frames = 0;
        bool loop_metadata = false;
        std::uint64_t bytes_read = 0, reads = 0, refills = 0;
        std::string error;

      private:
        Resource source_;
        std::ifstream file_;
        std::uint32_t data_offset_ = 0;
        unsigned block_ = 0, next_window_ = 0;
        struct Window
        {
            std::array<std::uint8_t, window_frames * 4> bytes{};
            std::size_t begin = 0, count = 0;
        };
        std::array<Window, 2> windows_;
        bool read(std::uint64_t offset, std::span<std::uint8_t>, std::string&);
        bool header(std::string&);
    };
} // namespace OpenBlood2
