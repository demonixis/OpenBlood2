#pragma once
#include "engine/ima_note_pitch.h"
#include "engine/ima_timeline.h"

namespace OpenBlood2
{
    // Bounded REPs metadata only. Embedded chord graphs remain opaque, never host pointers.
    struct ImaPersonality
    {
        std::string name, description, default_name;
        std::uint32_t scale_mask = 0, header_bytes = 0;
        std::vector<MusicChunk> opaque_chunks;
    };
    bool read_ima_personality(std::span<const std::uint8_t>, ImaPersonality&, std::string& error);

    struct ImaPreparedChord
    {
        std::size_t source = 0;
        std::int32_t time = 0;
        ImaHarmonicContext harmony; // enabled remains false: live activation is a separate decision.
        std::uint16_t variation_flags = 0;
    };
    struct ImaHarmony
    {
        ImaHarmonicContext base;
        bool base_flag = false; // Original chord byte 27, copied from section default_chord bit 7.
        std::vector<ImaPreparedChord> chords;
    };
    // Loader pitch flags and post-meter reconciliation of retained chords, in authored order.
    // Caller provides the resolved personality; this does not choose the current chord or play music.
    bool prepare_ima_harmony(const ImaSection&, const ImaTimeline&, const ImaPersonality&, ImaHarmony&,
                             std::string& error);
} // namespace OpenBlood2
