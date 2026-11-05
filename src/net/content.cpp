#include "net/content.h"
#include "net/codec.h"
#include <algorithm>
#include <array>
#include <fstream>

namespace OpenBlood2::Net
{
    bool content_fingerprint(const Resources& resources, Sha256Digest& out, std::string& error)
    {
        error.clear();
        if (resources.demo() || resources.nightmares() || resources.mounts().empty() || resources.mounts().size() > 32)
        {
            error = "Multiplayer requires mounted retail The Chosen data";
            return false;
        }
        Sha256 fingerprint;
        Writer header(64);
        header.text("OpenBlood2 network content v1", 32);
        header.u32(std::uint32_t(resources.mounts().size()));
        fingerprint.update(header.bytes());
        std::array<std::uint8_t, 65536> block{};
        for (const auto& mount : resources.mounts())
        {
            if (mount.role != MountRole::Base)
            {
                error = "Multiplayer data mods are not supported";
                return false;
            }
            std::error_code ec;
            const auto before = std::filesystem::last_write_time(mount.path, ec);
            if (ec)
            {
                error = "Cannot stat mounted archive: " + path_text(mount.path);
                return false;
            }
            const auto size = std::filesystem::file_size(mount.path, ec);
            if (ec || size != mount.size)
            {
                error = "Mounted archive size changed: " + path_text(mount.path);
                return false;
            }
            std::ifstream file(mount.path, std::ios::binary);
            if (!file)
            {
                error = "Cannot open mounted archive: " + path_text(mount.path);
                return false;
            }
            Sha256 digest;
            for (std::uint64_t offset = 0; offset < size;)
            {
                const auto count = std::size_t(std::min<std::uint64_t>(size - offset, block.size()));
                if (!file.read(reinterpret_cast<char*>(block.data()), std::streamsize(count)))
                {
                    error = "Cannot hash mounted archive: " + path_text(mount.path);
                    return false;
                }
                digest.update(std::span(block).first(count));
                offset += count;
            }
            if (file.peek() != std::char_traits<char>::eof() || file.bad() ||
                std::filesystem::last_write_time(mount.path, ec) != before || ec)
            {
                error = "Mounted archive changed during hashing: " + path_text(mount.path);
                return false;
            }
            auto name = path_text(mount.path.filename());
            for (auto& c : name)
                if (c >= 'A' && c <= 'Z')
                    c = char(c + ('a' - 'A'));
            Writer descriptor(1024);
            descriptor.text(name, 255);
            descriptor.u64(size);
            descriptor.raw(digest.finish());
            if (!descriptor.good())
            {
                error = "Archive name exceeds fingerprint bounds";
                return false;
            }
            fingerprint.update(descriptor.bytes());
        }
        out = fingerprint.finish();
        return true;
    }
} // namespace OpenBlood2::Net
