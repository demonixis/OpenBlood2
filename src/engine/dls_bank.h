#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2
{
    struct DlsLoop
    {
        std::uint32_t start = 0, length = 0;
    };
    struct DlsSampleSettings
    {
        std::uint16_t unity_note = 0;
        std::int16_t fine_tune = 0;
        std::int32_t attenuation = 0; // Raw signed DLS fixed-point value, not applied gain.
        std::uint32_t options = 0;
        // Wave defaults keep the authored loop. Region settings hold the effective loop of the
        // installed MSynth25 synth (msynth_effective_loop), which may end one frame past the
        // wave: that guard frame repeats the loop start sample.
        std::optional<DlsLoop> loop;
        std::optional<DlsLoop> authored_loop; // Region settings: the region's or inherited wave's record.
        bool loop_in_bounds = true;           // The authored loop fits its wave's frames.
    };
    struct DlsConnection
    {
        std::uint16_t source = 0, control = 0, destination = 0, transform = 0;
        std::int32_t scale = 0; // Retained graph data; no envelope/modulation evaluation yet.
    };
    struct DlsArticulation
    {
        bool present = false; // An empty authored graph differs from no graph.
        std::vector<DlsConnection> connections;
    };
    struct DlsWave
    {
        std::uint32_t pool_offset = 0, rate = 0;
        std::vector<float> samples; // Owned mono PCM, no resampling or tuning applied.
        std::optional<DlsSampleSettings> settings;
    };
    struct DlsRegion
    {
        std::uint16_t key_low = 0, key_high = 0, velocity_low = 0, velocity_high = 0;
        std::uint16_t options = 0, key_group = 0, wave_options = 0, phase_group = 0;
        std::uint32_t channel = 0, cue = 0, wave = 0;
        bool settings_override = false;
        DlsSampleSettings settings; // Region wsmp, otherwise the linked wave's wsmp.
        DlsArticulation articulation;
    };
    struct DlsInstrument
    {
        std::uint32_t bank = 0, program = 0;
        std::vector<DlsRegion> regions; // Preserve order/overlap; do not flatten layered regions.
        DlsArticulation articulation;
    };
    struct DlsBank
    {
        std::vector<DlsWave> waves;
        std::vector<std::uint32_t> cues; // Cue index -> wave index, not physical pool order.
        std::vector<DlsInstrument> instruments;
        std::size_t decoded_bytes = 0, opaque_chunks = 0;
    };
    inline constexpr std::size_t max_dls_decoded_bytes = 32 * 1024 * 1024;
    // Bounded DLS1 data decoding, not a synthesizer. Output is empty on failure.
    // Callers can lower the PCM budget when loading several banks.
    // Installed MSynth25.dll loop rules (wave verification at parse, region link, region
    // verification): the stored end is start + length in 32 bits; it is at least 1 and at most
    // frames + 1 (a guard copy of the loop start sample); a start at or past frames + 1 restarts
    // at zero; the start never passes the end; a loop shorter than 5 (wave) or 6 (region)
    // frames plays once (nullopt).
    std::optional<DlsLoop> msynth_effective_loop(DlsLoop authored, std::size_t frames, bool inherited_from_wave);
    bool read_dls_bank(std::span<const std::uint8_t> bytes, DlsBank& out, std::string& error,
                       std::size_t pcm_budget = max_dls_decoded_bytes);
} // namespace OpenBlood2
