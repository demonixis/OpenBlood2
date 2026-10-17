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
        std::optional<DlsLoop> loop;
        bool loop_in_bounds = true; // An unused wave default may be invalid; effective region settings must be valid.
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
    bool read_dls_bank(std::span<const std::uint8_t> bytes, DlsBank& out, std::string& error,
                       std::size_t pcm_budget = max_dls_decoded_bytes);
} // namespace OpenBlood2
