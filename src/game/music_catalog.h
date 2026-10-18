#pragma once
#include "engine/config.h"
#include "engine/dls_bank.h"
#include "engine/dls_sampler.h"
#include "engine/ima_band.h"
#include "engine/ima_cursor.h"
#include "engine/ima_curve.h"
#include "engine/ima_harmony.h"
#include "engine/ima_program.h"
#include "engine/ima_section.h"
#include "engine/ima_style.h"
#include "engine/ima_timeline.h"
#include "game/original/Music.h"
#include <memory>

namespace OpenBlood2
{
    struct MusicAssetRecord
    {
        std::string filename, error, style_reference, bank_error, style_error, bands_error, section_error,
            timeline_error, harmony_error, selected_band_error, band_setup_error, curves_error;
        std::shared_ptr<const DlsBank> bank;
        std::shared_ptr<const ImaStyle> style;
        std::shared_ptr<const ImaCurves> curves;       // Independent bounded decoding; no controller playback.
        std::shared_ptr<const ImaStyle> section_style; // Validated exact binding; no second style copy.
        std::shared_ptr<const ImaBands> bands;
        std::shared_ptr<const ImaBand> selected_band; // Aliasing owner of its immutable source catalogue.
        std::optional<ImaBandChoice> band_choice;
        std::string selected_band_file;
        std::array<ImaMidiSetup, 80> band_setup{}; // Prepared, not submitted to a MIDI device.
        bool band_setup_ready = false;
        std::array<DlsMidiChannel, 16> band_channels{}; // Source order, MIDI channel (source+4)&15.
        std::shared_ptr<const DlsBank> band_bank;       // Configured bank; collection labels never load files.
        std::string band_bank_file, band_bank_error;
        std::shared_ptr<const ImaSection> section;
        std::shared_ptr<const ImaTimeline> timeline;
        std::shared_ptr<const ImaHarmony> harmony;
        MusicAssetKind kind = MusicAssetKind::Section;
        std::size_t bytes = 0, chunks = 0, patterns = 0, note_chunks = 0;
        bool container_valid = false;
    };
    struct MusicPersonalityRecord
    {
        std::string filename, error;
        std::size_t bytes = 0;
        std::shared_ptr<const ImaPersonality> metadata;
    };
    struct MusicCatalog
    {
        MusicAssetPlan plan;
        ImaSectionCursor cursor; // Explicit inspection traversal; reset with the world catalogue.
        std::string cursor_file, cursor_error;
        ImaSectionProgram audition;
        std::string audition_file, audition_error;
        std::uint64_t audition_origin = 0, audition_end = 0, audition_failures = 0;
        bool audition_stopped = false;
        std::vector<MusicAssetRecord> assets;
        std::vector<MusicPersonalityRecord> personalities; // Dependencies, separate from original playlist requests.
        std::string error;
        std::size_t bytes_read = 0;
        bool step_cursor_notes(std::span<const std::uint16_t> draws);
        bool play_section(std::string_view filename, DlsSampler&);
        void inspect(const std::filesystem::path& root, const std::map<std::string, std::string>& properties);
    };
} // namespace OpenBlood2
