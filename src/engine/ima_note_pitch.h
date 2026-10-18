#pragma once
#include "engine/ima_style.h"

namespace OpenBlood2
{
    // Explicit effective runtime harmony, NOT an unprocessed authored section chord.
    // Reconciled chord values come from ima_harmony; live selection/activation remain separate.
    struct ImaHarmonicContext
    {
        bool enabled = false;
        std::uint32_t chord_mask = 0, scale_mask = 0;
        std::int8_t chord_root = 0, scale_root = 0;
        std::uint8_t chord_flags = 0;
    };
    struct ImaNotePitch
    {
        std::int16_t pitch = 0; // Feed directly to prepare_ima_note_events; zero suppresses.
        std::uint8_t mode = 0;  // Effective low nibble; zero on raw bypass (see converted).
        bool converted = false;
    };
    // Original mode/harmonic/octave/range arithmetic with bounded failure for a
    // scale-only walk which cannot terminate. Borrows inputs, owns output values,
    // consumes no RNG and changes no authored data. Failure clears output.
    bool resolve_ima_note_pitch(const ImaNote&, const ImaPattern&, const ImaHarmonicContext&, ImaNotePitch&,
                                std::string& error);
} // namespace OpenBlood2
