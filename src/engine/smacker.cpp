#include "engine/smacker.h"
#include "engine/atomic_file.h"
#include "engine/formats/reader.h"
#include "engine/hud_surface.h"
#include <algorithm>
#include <array>
#include <bit>

namespace OpenBlood2
{
    namespace
    {
        constexpr std::size_t max_file_bytes = 64 * 1024 * 1024;
        constexpr unsigned max_pixels = 1024 * 1024, max_frames = 12000;
        constexpr unsigned max_tree_bytes = 1024 * 1024, max_audio_bytes = 1024 * 1024;
        constexpr std::uint32_t compressed = 0x80000000, present = 0x40000000, sixteen_bit = 0x20000000,
                                stereo = 0x10000000, other_codec = 0x0c000000;

        class Bits
        {
          public:
            explicit Bits(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
            unsigned read(unsigned count)
            {
                while (available_ < count)
                {
                    if (bytes_.empty())
                    {
                        good_ = false;
                        return 0;
                    }
                    reservoir_ |= std::uint32_t(bytes_.front()) << available_;
                    bytes_ = bytes_.subspan(1);
                    available_ += 8;
                }
                const unsigned value = reservoir_ & ((1u << count) - 1);
                reservoir_ >>= count;
                available_ -= count;
                return value;
            }
            bool good() const
            {
                return good_;
            }

          private:
            std::span<const std::uint8_t> bytes_;
            std::uint32_t reservoir_ = 0;
            unsigned available_ = 0;
            bool good_ = true;
        };

        struct Node
        {
            unsigned right = 0; // A branch's left child immediately follows it in preorder.
            unsigned value = 0;
            unsigned cache_slot = 3;
        };
        using Tree = std::vector<Node>;

        template <typename Leaf>
        bool read_node(Bits& bits, Tree& tree, unsigned limit, unsigned depth, const Leaf& leaf)
        {
            if (depth == 64 || tree.size() == limit)
                return false;
            const auto index = tree.size();
            tree.emplace_back();
            if (!bits.read(1))
            {
                tree[index] = leaf();
                return bits.good();
            }
            if (!read_node(bits, tree, limit, depth + 1, leaf))
                return false;
            tree[index].right = unsigned(tree.size());
            return read_node(bits, tree, limit, depth + 1, leaf);
        }

        const Node& decode_node(Bits& bits, const Tree& tree)
        {
            std::size_t at = 0;
            while (tree[at].right)
                at = bits.read(1) ? tree[at].right : at + 1;
            return tree[at];
        }

        bool read_byte_tree(Bits& bits, Tree& tree)
        {
            if (!bits.read(1))
            {
                tree.emplace_back();
                return bits.good();
            }
            if (!read_node(bits, tree, 511, 0, [&] { return Node{0, bits.read(8)}; }))
                return false;
            bits.read(1); // Tree terminator.
            return bits.good();
        }

        struct WideTree
        {
            Tree nodes;
            std::array<unsigned, 3> history{};

            bool read(Bits& bits, unsigned storage_bytes)
            {
                if (!bits.read(1))
                {
                    nodes.emplace_back();
                    return bits.good();
                }
                Tree low, high;
                if (!read_byte_tree(bits, low) || !read_byte_tree(bits, high))
                    return false;
                const std::array markers{bits.read(16), bits.read(16), bits.read(16)};
                const unsigned limit = std::min(131071u, (storage_bytes + 3) / 4);
                nodes.reserve(limit);
                if (!read_node(bits, nodes, limit, 0, [&]
                    {
                        const auto lo = decode_node(bits, low).value;
                        const auto hi = decode_node(bits, high).value;
                        const auto value = lo | (hi << 8);
                        const auto marker = std::ranges::find(markers, value);
                        return Node{0, value, unsigned(marker - markers.begin())};
                    }))
                    return false;
                bits.read(1);
                return bits.good();
            }

            unsigned decode(Bits& bits)
            {
                const auto& node = decode_node(bits, nodes);
                const auto value = node.cache_slot < history.size() ? history[node.cache_slot] : node.value;
                if (value != history[0])
                    history = {value, history[0], history[1]};
                return value;
            }
        };

        using Palette = std::array<std::array<std::uint8_t, 3>, 256>;

        bool read_palette(Reader& frame, Palette& palette)
        {
            const unsigned size = 4 * frame.u8();
            if (!size || size - 1 > frame.remaining())
                return false;
            Reader changes(frame.take(size - 1));
            const auto old = palette;
            std::size_t i = 0;
            while (i < palette.size() && changes.good())
            {
                const auto command = changes.u8();
                if (command & 0x80)
                    i += (command & 0x7f) + 1;
                else if (command & 0x40)
                {
                    const unsigned source = changes.u8(), count = (command & 0x3f) + 1;
                    if (count > old.size() - source || count > palette.size() - i)
                        return false;
                    std::copy_n(old.begin() + source, count, palette.begin() + i);
                    i += count;
                }
                else
                {
                    const auto expand = [](unsigned value)
                    {
                        value &= 0x3f;
                        return std::uint8_t((value << 2) | (value >> 4));
                    };
                    palette[i++] = {expand(command), expand(changes.u8()), expand(changes.u8())};
                }
            }
            return changes.good() && i == palette.size();
        }

        std::int16_t pcm_sample(unsigned value, unsigned sample_bytes)
        {
            if (sample_bytes == 1)
                return std::int16_t((int(value & 0xff) - 128) * 256);
            return std::bit_cast<std::int16_t>(std::uint16_t(value));
        }

        bool decode_audio(std::span<const std::uint8_t> packet, std::uint32_t format, unsigned limit,
                          std::vector<std::int16_t>& samples)
        {
            Reader reader(packet);
            const auto packed = (format & compressed) != 0;
            const unsigned size = packed ? reader.u32() : unsigned(packet.size());
            const unsigned channels = (format & stereo) ? 2 : 1;
            const unsigned sample_bytes = (format & sixteen_bit) ? 2 : 1;
            if (!reader.good() || size > limit || size % (channels * sample_bytes))
                return false;
            if (!packed)
            {
                samples.resize(size / sample_bytes);
                for (auto& value : samples)
                    value = pcm_sample(sample_bytes == 2 ? reader.u16() : reader.u8(), sample_bytes);
                return true;
            }

            Bits bits(reader.take(reader.remaining()));
            if (!bits.read(1))
                return bits.good(); // An explicitly empty DPCM packet contributes no samples.
            const unsigned packet_channels = bits.read(1) + 1, packet_sample_bytes = bits.read(1) + 1;
            if (packet_channels != channels || packet_sample_bytes != sample_bytes || size < channels * sample_bytes)
                return false;
            std::array<Tree, 4> trees;
            for (unsigned i = 0; i < channels * sample_bytes; ++i)
                if (!read_byte_tree(bits, trees[i]))
                    return false;

            std::array<unsigned, 2> value{};
            samples.resize(size / sample_bytes);
            // The starting values are channel-reversed and their bytes are most significant first.
            for (unsigned i = channels; i > 0; --i)
            {
                const unsigned first = bits.read(8);
                value[i - 1] = sample_bytes == 2 ? (first << 8) | bits.read(8) : first;
                samples[i - 1] = pcm_sample(value[i - 1], sample_bytes);
            }
            for (std::size_t i = channels; i < samples.size() && bits.good(); i += channels)
                for (unsigned ch = 0; ch < channels; ++ch)
                {
                    const auto low = decode_node(bits, trees[ch * sample_bytes]).value;
                    const auto high = sample_bytes == 2 ? decode_node(bits, trees[ch * sample_bytes + 1]).value : 0;
                    value[ch] = (value[ch] + (low | (high << 8))) & 0xffff;
                    samples[i + ch] = pcm_sample(value[ch], sample_bytes);
                }
            return bits.good();
        }
    } // namespace

    struct SmackerDecoder::State
    {
        struct Frame
        {
            unsigned size = 0;
            std::uint8_t type = 0;
        };
        SmackerInfo info;
        HudSurface video;
        std::vector<std::uint8_t> file, indices;
        std::vector<std::int16_t> audio;
        std::vector<Frame> frames;
        Palette palette{};
        std::array<WideTree, 4> trees;
        std::size_t offset = 0;
        unsigned decoded = 0, audio_limit = 0;
        std::uint32_t audio_format = 0;
        bool failed = false;

        bool parse(std::string& error)
        {
            Reader header(file);
            const auto signature = header.text(4);
            info.width = header.u32();
            info.height = header.u32();
            info.frames = header.u32();
            const auto rate = std::bit_cast<std::int32_t>(header.u32());
            const auto flags = header.u32();
            audio_limit = header.u32();
            header.skip(6 * 4); // Other tracks' maximum audio packet sizes.
            const unsigned tree_bytes = header.u32();
            std::array<unsigned, 4> storage{};
            for (auto& size : storage)
                size = header.u32();
            audio_format = header.u32();
            unsigned track_types = (audio_format & present) ? 2 : 0;
            for (unsigned i = 2; i < 8; ++i)
                if (header.u32() & present)
                    track_types |= 1u << i;
            header.skip(4); // Reserved.
            info.frame_seconds = rate > 0 ? rate / 1000.0 : rate < 0 ? -double(rate) / 100000.0 : 0.1;
            if (!header.good() || signature != "SMK2" || flags || !info.width || !info.height ||
                info.width % 4 || info.height % 4 || info.width > 2048 || info.height > 2048 ||
                info.width > max_pixels / info.height || !info.frames || info.frames > max_frames ||
                info.frame_seconds < 0.008 || info.frame_seconds > 1 ||
                info.frame_seconds * info.frames > 1200)
            {
                error = "Invalid or unsupported SMK2 movie header";
                return false;
            }
            if (track_types & 2)
            {
                info.audio_rate = audio_format & 0x00ffffff;
                info.audio_channels = (audio_format & stereo) ? 2 : 1;
                if (info.audio_rate < 8000 || info.audio_rate > 96000 || (audio_format & other_codec) ||
                    !audio_limit || audio_limit > max_audio_bytes)
                {
                    error = "Invalid or unsupported Smacker audio format";
                    return false;
                }
            }
            if (!tree_bytes || tree_bytes > max_tree_bytes ||
                std::ranges::any_of(storage, [](unsigned size) { return size > max_tree_bytes; }))
            {
                error = "Smacker Huffman tables exceed the size limit";
                return false;
            }
            frames.resize(info.frames);
            for (auto& frame : frames)
                frame.size = header.u32() & ~3u;
            for (auto& frame : frames)
                frame.type = header.u8();
            Bits table_bits(header.take(tree_bytes));
            offset = header.position();
            std::size_t remaining = header.remaining();
            for (const auto& frame : frames)
            {
                if (!frame.size || frame.size > remaining || (frame.type & ~(track_types | 1u)))
                {
                    error = "Invalid Smacker frame table";
                    return false;
                }
                remaining -= frame.size;
            }
            if (!header.good() || remaining)
            {
                error = "Truncated Smacker header or invalid frame sizes";
                return false;
            }
            for (std::size_t i = 0; i < trees.size(); ++i)
                if (!trees[i].read(table_bits, storage[i]))
                {
                    error = "Invalid Smacker Huffman table";
                    return false;
                }
            const auto pixels = std::size_t(info.width) * info.height;
            indices.resize(pixels);
            video = {info.width, info.height, std::vector<std::uint8_t>(4 * pixels)};
            return true;
        }

        bool draw(std::span<const std::uint8_t> bytes)
        {
            Bits bits(bytes);
            for (auto& tree : trees)
                tree.history.fill(0);
            auto& map_tree = trees[0];
            auto& color_tree = trees[1];
            auto& pixel_tree = trees[2];
            auto& type_tree = trees[3];
            constexpr std::array long_runs{128u, 256u, 512u, 1024u, 2048u};
            const unsigned columns = info.width / 4, blocks = unsigned(indices.size() / 16);
            for (unsigned block = 0; block < blocks && bits.good();)
            {
                const unsigned code = type_tree.decode(bits), run_code = (code >> 2) & 63;
                const unsigned run = run_code < 59 ? run_code + 1 : long_runs[run_code - 59];
                const unsigned stop = std::min(block + run, blocks);
                const unsigned kind = code & 3;
                if (kind == 2) // A void block retains its indices even when the palette changes.
                {
                    block = stop;
                    continue;
                }
                for (; block < stop && bits.good(); ++block)
                {
                    auto* pixel = indices.data() + 4 * ((block / columns) * info.width + block % columns);
                    if (kind == 0)
                    {
                        const unsigned colors = color_tree.decode(bits);
                        unsigned map = map_tree.decode(bits);
                        for (unsigned y = 0; y < 4; ++y)
                            for (unsigned x = 0; x < 4; ++x, map >>= 1)
                                pixel[y * info.width + x] = std::uint8_t((map & 1) ? colors >> 8 : colors);
                    }
                    else if (kind == 1)
                    {
                        for (unsigned y = 0; y < 4; ++y)
                        {
                            const auto right = pixel_tree.decode(bits), left = pixel_tree.decode(bits);
                            auto* row = pixel + y * info.width;
                            row[0] = std::uint8_t(left);
                            row[1] = std::uint8_t(left >> 8);
                            row[2] = std::uint8_t(right);
                            row[3] = std::uint8_t(right >> 8);
                        }
                    }
                    else
                        for (unsigned y = 0; y < 4; ++y)
                            std::fill_n(pixel + y * info.width, 4, std::uint8_t(code >> 8));
                }
            }
            if (!bits.good())
                return false;
            auto* pixel = video.rgba.data();
            for (const auto index : indices)
            {
                const auto& color = palette[index];
                pixel[0] = color[0];
                pixel[1] = color[1];
                pixel[2] = color[2];
                pixel[3] = 255;
                pixel += 4;
            }
            return true;
        }

        bool step(std::string& error)
        {
            const auto& frame = frames[decoded];
            Reader reader(std::span<const std::uint8_t>(file).subspan(offset, frame.size));
            if ((frame.type & 1) && !read_palette(reader, palette))
            {
                error = "Invalid Smacker palette update";
                return false;
            }
            for (unsigned track = 0; track < 7; ++track)
                if (frame.type & (2u << track))
                {
                    const unsigned size = reader.u32();
                    if (!reader.good() || size < 4 || size - 4 > reader.remaining())
                    {
                        error = "Invalid Smacker audio packet";
                        return false;
                    }
                    const auto payload = reader.take(size - 4);
                    if (!track && !decode_audio(payload, audio_format, audio_limit, audio))
                    {
                        error = "Invalid Smacker audio data";
                        return false;
                    }
                }
            if (!draw(reader.take(reader.remaining())))
            {
                error = "Truncated Smacker video frame";
                return false;
            }
            offset += frame.size;
            ++decoded;
            return true;
        }
    };

    SmackerDecoder::SmackerDecoder() : state_(std::make_unique<State>()) {}
    SmackerDecoder::~SmackerDecoder() = default;

    bool SmackerDecoder::open(const std::filesystem::path& path, std::string& error)
    {
        close();
        if (read_file_bounded(path, max_file_bytes, state_->file, error) && state_->parse(error))
            return true;
        close();
        return false;
    }

    void SmackerDecoder::close()
    {
        *state_ = State{};
    }

    bool SmackerDecoder::next(std::string& error)
    {
        error.clear();
        state_->audio.clear();
        if (state_->failed)
        {
            error = "Smacker decoding stopped after an invalid frame";
            return false;
        }
        if (state_->decoded == state_->frames.size())
            return false;
        if (state_->step(error))
            return true;
        state_->audio.clear();
        state_->failed = true;
        return false;
    }

    const SmackerInfo& SmackerDecoder::info() const
    {
        return state_->info;
    }

    const HudSurface& SmackerDecoder::video() const
    {
        return state_->video;
    }

    std::span<const std::int16_t> SmackerDecoder::audio() const
    {
        return state_->audio;
    }

    unsigned SmackerDecoder::decoded_frames() const
    {
        return state_->decoded;
    }
} // namespace OpenBlood2
