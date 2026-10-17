#include "engine/ima_band.h"
#include "engine/formats/reader.h"
#include <algorithm>

namespace OpenBlood2
{
    namespace
    {
        bool label(std::span<const std::uint8_t> bytes, bool wide, std::string& out)
        {
            Reader r(bytes);
            while (r.remaining())
            {
                const auto c = wide ? r.u16() : r.u8();
                if (!c)
                    return r.good(); // Ignore original padding after first NUL.
                if (c < 32 || c > 126)
                    return false;
                out.push_back(static_cast<char>(c));
            }
            return false;
        }
        bool decode(std::span<const std::uint8_t> bytes, ImaBand& band)
        {
            if (bytes.size() != 202 || !label(bytes.first(32), true, band.name) ||
                !label(bytes.subspan(170, 32), false, band.collection))
                return false;
            Reader r(bytes);
            r.skip(32);
            for (auto& v : band.reserved32)
                v = r.u8();
            for (auto& c : band.channels)
                c.legacy_program = r.u8();
            for (auto& c : band.channels)
                c.volume = r.u8();
            for (auto& c : band.channels)
                c.pan = r.u8();
            for (auto& c : band.channels)
                c.transpose = std::bit_cast<std::int8_t>(r.u8());
            band.byte104 = std::bit_cast<std::int8_t>(r.u8());
            band.byte105 = r.u8();
            for (auto& c : band.channels)
                c.bank = r.u16();
            for (auto& c : band.channels)
                c.program = r.u8();
            for (auto& c : band.channels)
                c.byte154 = r.u8();
            return r.good();
        }
    } // namespace
    bool read_ima_bands(std::span<const std::uint8_t> bytes, MusicAssetKind kind, ImaBands& out, std::string& error,
                        std::size_t band_budget)
    {
        out = {};
        error.clear();
        ImaBands next;
        if (kind != MusicAssetKind::Style && kind != MusicAssetKind::Section)
        {
            error = "IMA bands: expected a style or section";
            return false;
        }
        if (!read_music_asset(bytes, kind, next.source, error))
            return false;
        const auto& chunks = next.source.chunks;
        bool valid = true;
        for (std::size_t i = 1; i < chunks.size() && valid; ++i)
        {
            const auto& c = chunks[i];
            if (c.depth != 1 || c.id != "RIFF" || c.form != "AABN")
                continue;
            ImaBand band;
            bool header = false;
            for (std::size_t j = i + 1; j < chunks.size() && chunks[j].depth > 1 && valid; ++j)
                if (chunks[j].depth == 2 && chunks[j].id == "band")
                {
                    valid = !header && decode(bytes.subspan(chunks[j].offset + 8, chunks[j].size), band);
                    header = true;
                    band.offset = chunks[j].offset;
                }
            valid &= header && next.bands.size() < std::min<std::size_t>(256, band_budget);
            if (valid)
                next.bands.push_back(std::move(band));
        }
        if (!valid)
        {
            error = "IMA bands: missing/duplicate header, unsupported size/name or band budget exceeded";
            return false;
        }
        out = std::move(next);
        return true;
    }
    std::uint32_t ima_band_patch(const ImaBandChannel& c, bool has_collection, bool percussion)
    {
        std::uint32_t patch = (std::uint32_t(c.bank) << 7) | c.program;
        if (!has_collection && !(patch & 0x200000))
            patch |= 0x400000;
        if (percussion)
            patch |= 0x80000000;
        return patch;
    }
    bool ima_band_setup(const ImaBand& band, std::array<ImaMidiSetup, 80>& out, std::string& error)
    {
        out = {};
        error.clear();
        std::array<ImaMidiSetup, 80> next{};
        for (unsigned i = 0; i < 16; ++i)
        {
            const auto& c = band.channels[i];
            const auto patch = ima_band_patch(c, !band.collection.empty(), i == 5);
            const bool legacy = (patch & 0x600000) == 0x400000;
            const auto program = legacy ? c.legacy_program : std::uint8_t(patch & 127);
            if (c.volume > 127 || c.pan > 127 || program > 127)
            {
                error = "IMA band setup: volume, pan or selected program exceeds MIDI range";
                return false;
            }
            const auto channel = (i + 4) & 15;
            const auto cc = static_cast<std::uint8_t>(0xb0 | channel);
            next[i * 5] = {cc, 7, c.volume};
            next[i * 5 + 1] = {cc, 10, c.pan};
            next[i * 5 + 2] = {cc, 0, static_cast<std::uint8_t>(legacy ? 0 : (patch >> 14) & 127)};
            next[i * 5 + 3] = {cc, 32, static_cast<std::uint8_t>(legacy ? 0 : (patch >> 7) & 127)};
            next[i * 5 + 4] = {static_cast<std::uint8_t>(0xc0 | channel), program, 0};
        }
        out = next;
        return true;
    }
} // namespace OpenBlood2
