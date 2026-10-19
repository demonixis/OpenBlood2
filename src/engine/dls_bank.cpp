#include "engine/dls_bank.h"
#include "engine/formats/reader.h"
#include "engine/music_asset.h"
#include <algorithm>
#include <array>
#include <map>
#include <set>

namespace OpenBlood2
{
    std::optional<DlsLoop> msynth_effective_loop(DlsLoop authored, std::size_t frames, bool inherited)
    {
        // MSynth25.dll (sha256 b30b8be7...): the wave object stores frames + 1 samples, the last
        // one a guard. 0x354962f0 verifies a wave's own loop when its chunk ends; 0x35496190
        // copies it into a region without wsmp, restarts a start at or past the stored count at
        // zero and writes the guard; 0x354960f0 then verifies the region loop.
        const auto stored = static_cast<std::uint32_t>(frames + 1);
        std::uint32_t start = authored.start;
        std::uint32_t end = authored.start + authored.length; // Unsigned 32-bit, as stored at parse.
        const auto verify = [&](std::uint32_t minimum)
        {
            if (end < 1)
                end = 1;
            else if (end > stored)
                end = stored;
            if (start > end)
                start = end;
            return end - start >= minimum;
        };
        if (inherited && !verify(5))
            return std::nullopt;
        if (start >= stored)
            start = 0;
        if (!verify(6))
            return std::nullopt;
        return DlsLoop{start, end - start};
    }
    namespace
    {
        class BankReader
        {
          public:
            std::span<const std::uint8_t> bytes;
            const MusicAssetInfo& info;
            std::string& error;
            DlsBank bank;
            std::size_t budget, region_count = 0, connection_count = 0;
            std::vector<std::vector<std::size_t>> children;
            std::vector<bool> used;

            BankReader(std::span<const std::uint8_t> b, const MusicAssetInfo& i, std::string& e, std::size_t limit)
                : bytes(b), info(i), error(e), budget(std::min(limit, max_dls_decoded_bytes)),
                  children(i.chunks.size()), used(i.chunks.size())
            {
                std::array<std::size_t, 17> parents{};
                for (std::size_t n = 0; n < i.chunks.size(); ++n)
                {
                    const auto depth = i.chunks[n].depth;
                    if (depth)
                        children[parents[depth - 1]].push_back(n);
                    parents[depth] = n;
                }
                used[0] = true;
            }
            bool fail(std::string_view why)
            {
                if (error.empty())
                    error = std::string("DLS: ") + std::string(why);
                return false;
            }
            std::span<const std::uint8_t> payload(std::size_t n) const
            {
                const auto& c = info.chunks[n];
                return bytes.subspan(c.offset + 8, c.size);
            }
            bool one(std::size_t parent, const char* id, const char* form, std::size_t& result, bool required = true)
            {
                result = 0; // Root is never a child; zero represents an absent optional record.
                for (auto n : children[parent])
                    if (info.chunks[n].id == id && info.chunks[n].form == form)
                    {
                        if (result)
                            return fail("Duplicate required/optional record");
                        result = n;
                        used[n] = true;
                    }
                return result || !required || fail("Missing required record");
            }
            bool settings(std::size_t n, std::size_t frames, DlsSampleSettings& out)
            {
                Reader r(payload(n));
                const auto size = r.u32();
                out.unity_note = r.u16();
                out.fine_tune = std::bit_cast<std::int16_t>(r.u16());
                out.attenuation = std::bit_cast<std::int32_t>(r.u32());
                out.options = r.u32();
                const auto loops = r.u32();
                if (!r.good() || size != 20 || out.unity_note > 127 || (out.options & ~3U) || loops > 1)
                    return fail("Unsupported/invalid sample settings");
                if (loops)
                {
                    const auto loop_size = r.u32(), type = r.u32();
                    const auto start = r.u32(), length = r.u32();
                    if (!r.good() || loop_size != 16 || type != 0 || !length)
                        return fail("Unsupported/out-of-range forward sample loop");
                    out.loop = DlsLoop{start, length};
                    out.loop_in_bounds = start < frames && length <= frames - start;
                }
                return !r.remaining() || fail("Unexpected sample settings extension");
            }
            bool articulation(std::size_t owner, DlsArticulation& out)
            {
                std::size_t list = 0, art = 0;
                if (!one(owner, "LIST", "lart", list, false))
                    return false;
                if (!list)
                    return true;
                if (!one(list, "art1", "", art))
                    return false;
                if (children[list].size() != 1)
                    return fail("Unsupported articulation list records");
                Reader r(payload(art));
                const auto size = r.u32(), count = r.u32();
                if (!r.good() || size != 8 || count > 8192 - connection_count || r.remaining() != count * 12ULL)
                    return fail("Invalid articulation count/size or connection budget");
                connection_count += count;
                out.present = true;
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    DlsConnection c;
                    c.source = r.u16();
                    c.control = r.u16();
                    c.destination = r.u16();
                    c.transform = r.u16();
                    c.scale = std::bit_cast<std::int32_t>(r.u32());
                    out.connections.push_back(c);
                }
                return true;
            }
            bool wave(std::size_t n, std::uint32_t base)
            {
                std::size_t fmt = 0, data = 0, wsmp = 0;
                if (!one(n, "fmt ", "", fmt) || !one(n, "data", "", data) || !one(n, "wsmp", "", wsmp, false))
                    return false;
                Reader f(payload(fmt));
                const auto format = f.u16(), channels = f.u16();
                const auto rate = f.u32(), byte_rate = f.u32();
                const auto block = f.u16(), bits = f.u16();
                // PCM ignores WAVEFORMATEX.cbSize. Retail banks store 18 there,
                // despite having no extension. The containing fmt size remains exact.
                if (!f.good() || (f.remaining() != 0 && f.remaining() != 2) || format != 1 || channels != 1 ||
                    rate < 8000 || rate > 96000 || (bits != 8 && bits != 16) || block != bits / 8 ||
                    byte_rate != rate * block)
                    return fail("Unsupported/invalid mono PCM format");
                const auto pcm = payload(data);
                if (pcm.empty() || pcm.size() % block ||
                    pcm.size() / block > (budget - bank.decoded_bytes) / sizeof(float))
                    return fail("Invalid PCM length or decoded sample budget");
                DlsWave w;
                w.pool_offset = info.chunks[n].offset - base;
                w.rate = rate;
                const auto frames = pcm.size() / block;
                if (wsmp)
                {
                    w.settings.emplace();
                    if (!settings(wsmp, frames, *w.settings))
                        return false;
                }
                w.samples.resize(frames);
                Reader samples(pcm);
                for (auto& s : w.samples)
                    s = bits == 8 ? (int(samples.u8()) - 128) / 128.0f
                                  : std::bit_cast<std::int16_t>(samples.u16()) / 32768.0f;
                bank.decoded_bytes += frames * sizeof(float);
                bank.waves.push_back(std::move(w));
                return true;
            }
            bool region(std::size_t n, DlsRegion& out)
            {
                std::size_t header = 0, link = 0, wsmp = 0;
                if (!one(n, "rgnh", "", header) || !one(n, "wlnk", "", link) || !one(n, "wsmp", "", wsmp, false))
                    return false;
                Reader h(payload(header)), l(payload(link));
                out.key_low = h.u16();
                out.key_high = h.u16();
                out.velocity_low = h.u16();
                out.velocity_high = h.u16();
                out.options = h.u16();
                out.key_group = h.u16();
                out.wave_options = l.u16();
                out.phase_group = l.u16();
                out.channel = l.u32();
                out.cue = l.u32();
                if (!h.good() || h.remaining() || !l.good() || l.remaining() || out.key_low > out.key_high ||
                    out.key_high > 127 || out.velocity_low > out.velocity_high || out.velocity_high > 127 ||
                    (out.options & ~1U) || out.key_group > 15 || (out.wave_options & ~1U) || out.channel != 1 ||
                    out.cue >= bank.cues.size())
                    return fail("Invalid/unsupported region range or wave link");
                out.wave = bank.cues[out.cue];
                const auto& w = bank.waves[out.wave];
                out.settings_override = wsmp != 0;
                if (wsmp)
                {
                    if (!settings(wsmp, w.samples.size(), out.settings))
                        return false;
                }
                else if (w.settings)
                    out.settings = *w.settings;
                else
                    return fail("Neither region nor linked wave supplies sample settings");
                // Effective loop of the installed synth: Crudux/Track1 regions end past their
                // waves (664633/32025 for 64633/23025 frames) and play up to the guard frame.
                out.settings.authored_loop = out.settings.loop;
                if (out.settings.loop)
                    out.settings.loop = msynth_effective_loop(*out.settings.loop, w.samples.size(), !wsmp);
                return articulation(n, out.articulation);
            }
            bool read()
            {
                std::size_t colh = 0, pool = 0, table = 0, instruments = 0;
                if (!one(0, "colh", "", colh) || !one(0, "LIST", "wvpl", pool) || !one(0, "ptbl", "", table) ||
                    !one(0, "LIST", "lins", instruments))
                    return false;
                Reader h(payload(colh));
                const auto count = h.u32();
                if (!h.good() || h.remaining() || !count || count > 256 || children[instruments].size() != count)
                    return fail("Invalid instrument collection count");
                if (children[pool].empty() || children[pool].size() > 1024)
                    return fail("Wave pool count limit");
                std::map<std::uint32_t, std::uint32_t> offsets;
                for (auto n : children[pool])
                {
                    used[n] = true;
                    if (info.chunks[n].id != "LIST" || info.chunks[n].form != "wave")
                        return fail("Unsupported external/non-wave pool entry");
                    if (!wave(n, info.chunks[pool].offset + 12))
                        return false;
                    offsets.emplace(bank.waves.back().pool_offset, bank.waves.size() - 1);
                }
                Reader t(payload(table));
                const auto size = t.u32(), cues = t.u32();
                if (!t.good() || size != 8 || !cues || cues > 4096 || t.remaining() != cues * 4ULL)
                    return fail("Invalid pool table count/size");
                for (std::uint32_t i = 0; i < cues; ++i)
                {
                    const auto found = offsets.find(t.u32());
                    if (found == offsets.end())
                        return fail("Pool cue does not identify a wave boundary");
                    bank.cues.push_back(found->second);
                }
                std::set<std::pair<std::uint32_t, std::uint32_t>> locales;
                for (auto n : children[instruments])
                {
                    used[n] = true;
                    if (info.chunks[n].id != "LIST" || info.chunks[n].form != "ins ")
                        return fail("Unsupported instrument list entry");
                    std::size_t insh = 0, lrgn = 0;
                    if (!one(n, "insh", "", insh) || !one(n, "LIST", "lrgn", lrgn))
                        return false;
                    DlsInstrument instrument;
                    Reader i(payload(insh));
                    const auto regions = i.u32();
                    instrument.bank = i.u32();
                    instrument.program = i.u32();
                    if (!i.good() || i.remaining() || !regions || regions > 4096 - region_count ||
                        children[lrgn].size() != regions || instrument.program > 127 ||
                        !locales.emplace(instrument.bank, instrument.program).second)
                        return fail("Invalid/ambiguous instrument locale or region count");
                    region_count += regions;
                    if (!articulation(n, instrument.articulation))
                        return false;
                    for (auto rn : children[lrgn])
                    {
                        used[rn] = true;
                        if (info.chunks[rn].id != "LIST" || info.chunks[rn].form != "rgn ")
                            return fail("Unsupported region type");
                        DlsRegion r;
                        if (!region(rn, r))
                            return false;
                        instrument.regions.push_back(std::move(r));
                    }
                    bank.instruments.push_back(std::move(instrument));
                }
                bank.opaque_chunks = std::count(used.begin(), used.end(), false);
                return true;
            }
        };
    } // namespace
    bool read_dls_bank(std::span<const std::uint8_t> bytes, DlsBank& out, std::string& error, std::size_t pcm_budget)
    {
        out = {};
        MusicAssetInfo info;
        if (!read_music_asset(bytes, MusicAssetKind::Instruments, info, error))
            return false;
        BankReader reader(bytes, info, error, pcm_budget);
        if (!reader.read())
            return false;
        out = std::move(reader.bank);
        return true;
    }
} // namespace OpenBlood2
