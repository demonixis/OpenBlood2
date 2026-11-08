#include "engine/hud_surface.h"
#include "engine/smacker.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>

namespace
{
    using Bytes = std::vector<std::uint8_t>;

    void word(Bytes& bytes, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes.push_back(std::uint8_t(value >> (i * 8)));
    }

    struct BitWriter
    {
        Bytes bytes;
        unsigned position = 0;
        void put(unsigned value, unsigned count = 1)
        {
            for (unsigned i = 0; i < count; ++i, ++position)
            {
                if (position % 8 == 0)
                    bytes.push_back(0);
                bytes.back() |= ((value >> i) & 1) << (position % 8);
            }
        }
        void constant(unsigned value)
        {
            put(1);
            put(0);
            put(value, 8);
            put(0);
        }
        void pair(unsigned left, unsigned right)
        {
            put(1);
            put(1);
            put(0);
            put(left, 8);
            put(0);
            put(right, 8);
            put(0);
        }
    };

    Bytes movie(std::size_t& audio_size_position)
    {
        BitWriter trees;
        trees.put(0, 3); // The color/map/full trees are unused.
        trees.put(1);
        trees.pair(3, 2);
        trees.pair(1, 0);
        for (auto marker : {0xfffcu, 0xfffdu, 0xfffeu})
            trees.put(marker, 16);
        trees.put(1);
        trees.put(0, 3); // Fill with color 1.
        trees.put(0);
        trees.put(3, 2); // Preserve indices.
        trees.put(0);

        BitWriter dpcm;
        dpcm.put(7, 3); // Present, stereo, 16-bit.
        for (auto delta : {2u, 0u, 1u, 0u})
            dpcm.constant(delta);
        for (auto value : {255u, 255u, 0u, 255u})
            dpcm.put(value, 8); // Right base 0xffff, left base 0x00ff.

        Bytes first{2, 0x80, 63, 0, 0, 0xff, 0xfd, 0};
        word(first, unsigned(dpcm.bytes.size()) + 8);
        word(first, 8);
        first.insert(first.end(), dpcm.bytes.begin(), dpcm.bytes.end());
        first.resize((first.size() + 4) & ~std::size_t(3)); // Fill block plus frame alignment.
        const Bytes second{2, 0x80, 0, 0, 63, 0xff, 0xfd, 0, 1, 0, 0, 0};

        Bytes file{'S', 'M', 'K', '2'};
        for (auto value : {4u, 4u, 2u, 50u, 0u, 8u})
            word(file, value);
        for (unsigned i = 0; i < 6; ++i)
            word(file, 0);
        word(file, unsigned(trees.bytes.size()));
        for (auto value : {0u, 0u, 0u, 12u, 0xf0005622u})
            word(file, value);
        for (unsigned i = 0; i < 7; ++i)
            word(file, 0);
        word(file, unsigned(first.size()));
        word(file, unsigned(second.size()));
        file.insert(file.end(), {3, 1});
        file.insert(file.end(), trees.bytes.begin(), trees.bytes.end());
        audio_size_position = file.size() + 8;
        file.insert(file.end(), first.begin(), first.end());
        file.insert(file.end(), second.begin(), second.end());
        return file;
    }

    bool write(const std::filesystem::path& path, const Bytes& bytes)
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        return bool(out);
    }

    bool test(const std::filesystem::path& path)
    {
        OpenBlood2::SmackerDecoder decoder;
        std::string error;
#define CHECK(condition)                                                                                               \
        do                                                                                                             \
        {                                                                                                              \
            if (!(condition))                                                                                          \
            {                                                                                                          \
                std::cerr << __LINE__ << ": " #condition ": " << error << '\n';                                          \
                return false;                                                                                          \
            }                                                                                                          \
        } while (false)
        std::size_t audio_size_position = 0;
        auto file = movie(audio_size_position);
        CHECK(write(path, file) && decoder.open(path, error));
        const auto& info = decoder.info();
        CHECK(info.width == 4 && info.height == 4 && info.frames == 2 && info.frame_seconds == 0.05);
        CHECK(info.audio_channels == 2 && info.audio_rate == 22050 && decoder.next(error));
        const std::array<std::int16_t, 4> expected_audio{255, -1, 257, 0};
        CHECK(std::ranges::equal(decoder.audio(), expected_audio));
        const auto color = [&](unsigned r, unsigned g, unsigned b)
        {
            const auto& pixels = decoder.video().rgba;
            if (pixels.size() != 64)
                return false;
            for (std::size_t i = 0; i < pixels.size(); i += 4)
                if (pixels[i] != r || pixels[i + 1] != g || pixels[i + 2] != b || pixels[i + 3] != 255)
                    return false;
            return true;
        };
        CHECK(color(255, 0, 0));
        CHECK(decoder.next(error) && color(0, 0, 255) && decoder.audio().empty());
        CHECK(!decoder.next(error) && error.empty() && decoder.decoded_frames() == 2);

        file[audio_size_position + 3] = 0xff;
        CHECK(write(path, file) && decoder.open(path, error) && !decoder.next(error));
        CHECK(!error.empty() && decoder.audio().empty() && decoder.decoded_frames() == 0);
        file.pop_back();
        CHECK(write(path, file) && !decoder.open(path, error));
        CHECK(!error.empty() && decoder.info().frames == 0 && decoder.video().rgba.empty());
        return true;
#undef CHECK
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const auto path = OpenBlood2::path_from_utf8(argv[1]);
    const bool passed = test(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return passed ? 0 : 1;
}
