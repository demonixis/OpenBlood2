#pragma once
#include "engine/config.h"
#include <map>
#include <span>

namespace OpenBlood2
{
    struct Resource
    {
        std::string path;
        std::filesystem::path archive;
        std::uint32_t offset = 0, size = 0;
        unsigned overrides = 0;
    };
    struct Mount
    {
        std::filesystem::path path;
        std::size_t entries = 0;
    };
    bool resource_path(std::string_view input, std::string& output);
    // REZ v1 index. The input is a whole synthetic archive in tests; production
    // uses bounded directory reads from disk without retaining the archive payload.
    bool read_rez(std::span<const std::uint8_t> bytes, std::vector<Resource>& entries, std::string& error);
    class Resources
    {
      public:
        bool mount(const std::filesystem::path& path, std::string& error);
        bool mount_game(const Config& config, std::string& error);
        const Resource* find(std::string_view path) const;
        bool read(std::string_view path, std::vector<std::uint8_t>& bytes, std::string& error) const;
        const auto& index() const
        {
            return index_;
        }
        const auto& mounts() const
        {
            return mounts_;
        }

      private:
        std::map<std::string, Resource> index_;
        std::vector<Mount> mounts_;
    };
} // namespace OpenBlood2
