#include "game/music_catalog.h"
#include "engine/resources.h"
#include <algorithm>
#include <fstream>
#include <set>

namespace OpenBlood2
{
    namespace
    {
        bool resolve(const std::filesystem::path& root, const std::string& relative, std::filesystem::path& out,
                     std::string& error)
        {
            std::string normalized;
            if (root.empty() || !resource_path(relative, normalized))
            {
                error = "Music content root/path unavailable";
                return false;
            }
            auto current = root;
            std::error_code ec;
            std::size_t first = 0, depth = 0;
            while (first < normalized.size())
            {
                if (++depth > 8)
                {
                    error = "Music directory depth limit";
                    return false;
                }
                auto last = normalized.find('/', first);
                if (last == normalized.npos)
                    last = normalized.size();
                const auto component = normalized.substr(first, last - first);
                std::filesystem::path matched;
                std::size_t count = 0;
                auto it = std::filesystem::directory_iterator(current, ec);
                for (auto end = std::filesystem::directory_iterator{}; !ec && it != end; it.increment(ec))
                {
                    if (++count > 4096)
                    {
                        error = "Music directory entry limit";
                        return false;
                    }
                    std::string name;
                    if (!resource_path(path_text(it->path().filename()), name) || name != component)
                        continue;
                    if (!matched.empty())
                    {
                        error = "Ambiguous music path: " + normalized;
                        return false;
                    }
                    const auto status = it->symlink_status(ec);
                    if (ec || std::filesystem::is_symlink(status))
                    {
                        error = "Unreadable or symlinked music path: " + normalized;
                        return false;
                    }
                    matched = it->path();
                }
                if (ec || matched.empty())
                {
                    error = "Missing/unreadable music path: " + normalized;
                    return false;
                }
                current = std::move(matched);
                first = last + 1;
            }
            if (!std::filesystem::is_regular_file(current, ec) || ec)
            {
                error = "Music resource is not a regular file: " + normalized;
                return false;
            }
            out = std::move(current);
            return true;
        }
    } // namespace
    void MusicCatalog::inspect(const std::filesystem::path& root, const std::map<std::string, std::string>& properties)
    {
        *this = {};
        if (!CMusic::BuildAssetPlan(properties, plan, error))
            return;
        // Own all names. Never retain directory iterators, resource spans or game objects.
        auto requests = plan.requests;
        std::set<std::string> seen;
        std::size_t reserved_bytes = 0, decoded_bytes = 0, decoded_notes = 0, decoded_bands = 0,
                    decoded_section_records = 0;
        for (std::size_t i = 0; i < requests.size(); ++i)
        {
            const auto request = requests[i];
            std::string key;
            if (!resource_path(request.filename, key) || !seen.insert(key).second)
                continue;
            if (assets.size() >= 256)
            {
                error = "Music asset count limit";
                break;
            }
            MusicAssetRecord record;
            record.filename = request.filename;
            record.kind = request.kind;
            std::filesystem::path path;
            if (resolve(root, plan.directory + "/" + key, path, record.error))
            {
                std::error_code ec;
                const auto size = std::filesystem::file_size(path, ec);
                if (ec || size > max_music_asset_bytes || size > 64 * 1024 * 1024 - reserved_bytes)
                    record.error = "Music file size/read budget exceeded or unavailable";
                else
                {
                    reserved_bytes += size;
                    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
                    std::ifstream stream(path, std::ios::binary);
                    const bool read = bool(stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size()));
                    record.bytes = static_cast<std::size_t>(stream.gcount());
                    bytes_read += record.bytes;
                    if (!read || stream.peek() != std::char_traits<char>::eof())
                        record.error = "Music file changed or failed during read";
                    else
                    {
                        MusicAssetInfo info;
                        record.container_valid = read_music_asset(bytes, request.kind, info, record.error);
                        if (record.container_valid && request.kind == MusicAssetKind::Instruments)
                        {
                            auto bank = std::make_shared<DlsBank>();
                            if (read_dls_bank(bytes, *bank, record.bank_error, 64 * 1024 * 1024 - decoded_bytes))
                            {
                                decoded_bytes += bank->decoded_bytes;
                                record.bank = std::move(bank);
                            }
                        }
                        if (record.container_valid && request.kind == MusicAssetKind::Style)
                        {
                            auto style = std::make_shared<ImaStyle>();
                            if (read_ima_style(bytes, *style, record.style_error, 262144 - decoded_notes))
                            {
                                decoded_notes += style->notes;
                                record.style = std::move(style);
                            }
                        }
                        if (record.container_valid && request.kind != MusicAssetKind::Instruments)
                        {
                            auto bands = std::make_shared<ImaBands>();
                            if (read_ima_bands(bytes, request.kind, *bands, record.bands_error, 4096 - decoded_bands))
                            {
                                decoded_bands += bands->bands.size();
                                record.bands = std::move(bands);
                            }
                        }
                        if (record.container_valid && request.kind == MusicAssetKind::Section)
                        {
                            auto section = std::make_shared<ImaSection>();
                            if (read_ima_section(bytes, *section, record.section_error,
                                                 16384 - decoded_section_records))
                            {
                                decoded_section_records += section->records();
                                record.section = std::move(section);
                            }
                        }
                        record.chunks = info.chunks.size();
                        record.style_reference = info.style_reference;
                        for (const auto& chunk : info.chunks)
                        {
                            record.patterns += chunk.id == "RIFF" && chunk.form == "AAPT";
                            record.note_chunks += chunk.id == "note";
                        }
                        if (!info.style_reference.empty())
                            requests.push_back(
                                {"Section style reference", info.style_reference + ".sty", MusicAssetKind::Style});
                    }
                }
            }
            assets.push_back(std::move(record));
        }
        // All exact-path style requests have now been processed. Never resolve labels or search other directories.
        std::size_t timeline_records = 0;
        for (auto& asset : assets)
        {
            if (!asset.section)
                continue;
            std::string key;
            if (!resource_path(asset.style_reference + ".sty", key))
            {
                asset.timeline_error = "IMA timeline: invalid style reference";
                continue;
            }
            std::shared_ptr<const ImaStyle> style;
            for (const auto& candidate : assets)
            {
                std::string candidate_key;
                if (candidate.kind == MusicAssetKind::Style && resource_path(candidate.filename, candidate_key) &&
                    candidate_key == key)
                    style = candidate.style;
            }
            if (!style)
            {
                asset.timeline_error = "IMA timeline: referenced style missing or undecoded";
                continue;
            }
            const auto& id = asset.section->style_id;
            if (std::any_of(id.begin(), id.end(), [](auto b) { return b != 0; }) && id != style->identifier)
            {
                asset.timeline_error = "IMA timeline: style name and identifier disagree";
                continue;
            }
            auto timeline = std::make_shared<ImaTimeline>();
            if (prepare_ima_timeline(*asset.section, *style, *timeline, asset.timeline_error, 32768 - timeline_records))
            {
                timeline_records += timeline->records();
                asset.timeline = std::move(timeline);
                asset.section_style = std::move(style);
            }
        }
        for (auto& asset : assets)
        {
            if (asset.kind != MusicAssetKind::Section)
                continue;
            if (!asset.section || !asset.bands)
            {
                asset.selected_band_error = "IMA band selection: section or band decoding unavailable";
                continue;
            }
            std::shared_ptr<const ImaBands> style_bands;
            std::string style_file;
            // Use the existing validated filename/identity binding, never the instruments
            // bootstrap style or an arbitrary catalogue entry.
            if (asset.section_style)
                for (const auto& candidate : assets)
                    if (candidate.kind == MusicAssetKind::Style && candidate.style == asset.section_style)
                    {
                        style_bands = candidate.bands;
                        style_file = candidate.filename;
                        break;
                    }
            if (!select_ima_band(*asset.bands, style_bands.get(), asset.band_choice, asset.selected_band_error))
                continue;
            const bool own = asset.band_choice->scope == ImaBandScope::Section;
            const auto& source = own ? asset.bands : style_bands;
            asset.selected_band = std::shared_ptr<const ImaBand>(source, &source->bands[asset.band_choice->index]);
            asset.selected_band_file = own ? asset.filename : style_file;
            asset.band_setup_ready = ima_band_setup(*asset.selected_band, asset.band_setup, asset.band_setup_error);
            if (asset.band_setup_ready)
            {
                for (unsigned ch = 0; ch < 16; ++ch)
                {
                    const auto* setup = asset.band_setup.data() + ch * 5;
                    asset.band_channels[ch] = {
                        setup[2].data2, setup[3].data2, setup[4].data1, ch == 5, {setup[0].data2, 127, setup[1].data2}};
                }
                // The original InitInstruments plan supplies one DLS bank. This explicit
                // diagnostic treats its decoded instruments as resident; download tracking
                // and the OS GM collection remain separate, unavailable services.
                const MusicAssetRecord* bank = nullptr;
                for (const auto& candidate : assets)
                    if (candidate.kind == MusicAssetKind::Instruments)
                    {
                        if (bank)
                        {
                            asset.band_bank_error = "Multiple configured DLS banks are unsupported";
                            break;
                        }
                        bank = &candidate;
                    }
                if (asset.band_bank_error.empty())
                {
                    if (!bank || !bank->bank)
                        asset.band_bank_error = "Configured DLS bank unavailable";
                    else
                    {
                        asset.band_bank = bank->bank;
                        asset.band_bank_file = bank->filename;
                    }
                }
            }
        }
        // Bounded installed-file profile, not the historical general UUID/resource resolver.
        // Never turn a personality label into a directory or resolve it across music folders.
        for (auto& asset : assets)
        {
            if (!asset.section)
                continue;
            const auto& section = *asset.section;
            std::string label;
            if (section.personality.find_first_of("/\\") != std::string::npos ||
                !resource_path(section.personality, label) ||
                std::any_of(section.personality_id.begin(), section.personality_id.end(),
                            [](auto v) { return v != 0; }))
            {
                asset.harmony_error = "IMA harmony: unsupported personality label or nonzero identity";
                continue;
            }
            const auto filename = label + ".per";
            auto index = std::find_if(personalities.begin(), personalities.end(),
                                      [&](const auto& p) { return p.filename == filename; });
            if (index == personalities.end())
            {
                if (personalities.size() == 64)
                {
                    asset.harmony_error = "IMA harmony: personality dependency count limit";
                    continue;
                }
                MusicPersonalityRecord record;
                record.filename = filename;
                std::filesystem::path path;
                if (resolve(root, plan.directory + "/" + filename, path, record.error))
                {
                    std::error_code ec;
                    const auto size = std::filesystem::file_size(path, ec);
                    if (ec || size > 1024 * 1024 || size > 64 * 1024 * 1024 - reserved_bytes)
                        record.error = "IMA personality: file size/read budget exceeded or unavailable";
                    else
                    {
                        reserved_bytes += size;
                        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
                        std::ifstream stream(path, std::ios::binary);
                        const bool read = bool(stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size()));
                        record.bytes = static_cast<std::size_t>(stream.gcount());
                        bytes_read += record.bytes;
                        if (!read || stream.peek() != std::char_traits<char>::eof())
                            record.error = "IMA personality: file changed or failed during read";
                        else
                        {
                            auto metadata = std::make_shared<ImaPersonality>();
                            if (read_ima_personality(bytes, *metadata, record.error))
                                record.metadata = std::move(metadata);
                        }
                    }
                }
                personalities.push_back(std::move(record));
                index = personalities.end() - 1;
            }
            std::string decoded_name;
            if (!index->metadata)
                asset.harmony_error = "IMA harmony: personality metadata unavailable: " + index->error;
            else if (!resource_path(index->metadata->name, decoded_name) || decoded_name != label)
                asset.harmony_error = "IMA harmony: personality filename and embedded name disagree";
            else if (!asset.timeline)
                asset.harmony_error = "IMA harmony: reconciled timeline unavailable";
            else
            {
                auto harmony = std::make_shared<ImaHarmony>();
                if (prepare_ima_harmony(section, *asset.timeline, *index->metadata, *harmony, asset.harmony_error))
                    asset.harmony = std::move(harmony);
            }
        }
    }
} // namespace OpenBlood2
