#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OpenBlood2
{
    struct AtomicWriteOptions
    {
        // Remove `<name>.tmp-*` siblings left by earlier interrupted writes, only after this write
        // has been renamed into place. Preferences keep them, so concurrent instances never collide.
        bool remove_stale_temporaries = false;
    };
    // Creates the parent directory, writes a unique sibling temporary file and renames it over
    // `path`. Any failure removes the temporary file and leaves the previous file untouched. A crash
    // leaves the old or the new file; no fsync durability is claimed. `what` names the file in the
    // error ("Cannot create <what> directory: ...", "Cannot atomically write <what>: ...").
    bool write_file_atomic(const std::filesystem::path& path, std::span<const std::uint8_t> bytes,
                           std::string_view what, std::string& error, const AtomicWriteOptions& options = {});
    // Reads a whole regular file of at most `limit` bytes; the size is checked before allocating.
    bool read_file_bounded(const std::filesystem::path& path, std::uint64_t limit, std::vector<std::uint8_t>& bytes,
                           std::string& error);
} // namespace OpenBlood2
