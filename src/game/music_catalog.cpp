#include "game/music_catalog.h"
#include "engine/resources.h"
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
        std::size_t reserved_bytes = 0, decoded_bytes = 0;
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
    }
} // namespace OpenBlood2
