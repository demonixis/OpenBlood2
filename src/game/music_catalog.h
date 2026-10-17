#pragma once
#include "engine/config.h"
#include "engine/dls_bank.h"
#include "engine/ima_band.h"
#include "engine/ima_style.h"
#include "game/original/Music.h"
#include <memory>

namespace OpenBlood2
{
    struct MusicAssetRecord
    {
        std::string filename, error, style_reference, bank_error, style_error, bands_error;
        std::shared_ptr<const DlsBank> bank;
        std::shared_ptr<const ImaStyle> style;
        std::shared_ptr<const ImaBands> bands;
        MusicAssetKind kind = MusicAssetKind::Section;
        std::size_t bytes = 0, chunks = 0, patterns = 0, note_chunks = 0;
        bool container_valid = false;
    };
    struct MusicCatalog
    {
        MusicAssetPlan plan;
        std::vector<MusicAssetRecord> assets;
        std::string error;
        std::size_t bytes_read = 0;
        void inspect(const std::filesystem::path& root, const std::map<std::string, std::string>& properties);
    };
} // namespace OpenBlood2
