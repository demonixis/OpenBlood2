#pragma once
// Guards destructive test scratch roots taken from the command line. A root is
// emptied only when it is absent, already empty, carries the marker written
// here, or lies strictly inside a CMake build tree. A mistyped argument such as
// the game data directory is refused instead of being deleted.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

namespace OpenBlood2::Testing
{
    inline constexpr const char* scratch_marker = ".openblood2-test-scratch";

    inline bool inside_build_tree(const std::filesystem::path& root)
    {
        std::error_code ec;
        const auto path = std::filesystem::weakly_canonical(std::filesystem::absolute(root, ec), ec);
        if (ec || std::filesystem::exists(path / "CMakeCache.txt", ec))
            return false;
        for (auto parent = path.parent_path(); !parent.empty(); parent = parent.parent_path())
        {
            if (std::filesystem::exists(parent / "CMakeCache.txt", ec))
                return true;
            if (parent == parent.parent_path())
                break;
        }
        return false;
    }

    inline bool scratch_removable(const std::filesystem::path& root)
    {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec))
            return !ec;
        if (!std::filesystem::is_directory(root, ec) || ec)
            return false;
        if (std::filesystem::is_empty(root, ec) && !ec)
            return true;
        return std::filesystem::is_regular_file(root / scratch_marker, ec) || inside_build_tree(root);
    }

    // Removes root if it is test scratch; refuses (and reports) anything else.
    inline bool remove_scratch_dir(const std::filesystem::path& root)
    {
        if (root.empty() || !scratch_removable(root))
        {
            std::cerr << "refusing to delete non-scratch directory: " << root.string() << '\n';
            return false;
        }
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        return !ec;
    }

    // Empties root and recreates it as marked test scratch.
    inline bool reset_scratch_dir(const std::filesystem::path& root)
    {
        if (!remove_scratch_dir(root))
            return false;
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        if (ec)
            return false;
        std::ofstream(root / scratch_marker) << "OpenBlood2 test scratch; safe to delete\n";
        return std::filesystem::is_regular_file(root / scratch_marker, ec);
    }
} // namespace OpenBlood2::Testing
