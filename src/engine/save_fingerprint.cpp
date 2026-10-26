#include "engine/save_fingerprint.h"

namespace OpenBlood2
{
    namespace
    {
        void put32(Sha256& hash, std::uint32_t v)
        {
            const std::array<std::uint8_t, 4> b{std::uint8_t(v), std::uint8_t(v >> 8), std::uint8_t(v >> 16),
                                                std::uint8_t(v >> 24)};
            hash.update(b);
        }
        const char* edition_name(SaveEdition edition)
        {
            return edition == SaveEdition::Demo ? "demo" : edition == SaveEdition::Nightmares ? "nightmares" : "retail";
        }
        const char* role_name(MountRole role)
        {
            return role == MountRole::Mod ? "mod" : "base";
        }
    } // namespace
    bool resource_index_digest(const Resources& resources, Sha256Digest& output, std::string& error)
    {
        error.clear();
        const auto& mounts = resources.mounts();
        Sha256 hash;
        put32(hash, std::uint32_t(resources.index().size()));
        for (const auto& [path, resource] : resources.index())
        {
            // Later mounts win, so a repeated archive path resolves to its last mount.
            std::size_t ordinal = mounts.size();
            for (std::size_t i = 0; i < mounts.size(); ++i)
                if (mounts[i].path == resource.archive)
                    ordinal = i;
            if (ordinal == mounts.size())
            {
                error = "Resource " + path + " does not belong to a mounted archive";
                return false;
            }
            put32(hash, std::uint32_t(path.size()));
            hash.update({reinterpret_cast<const std::uint8_t*>(path.data()), path.size()});
            put32(hash, std::uint32_t(ordinal));
            put32(hash, resource.offset);
            put32(hash, resource.size);
        }
        output = hash.finish();
        return true;
    }
    bool build_fingerprint(const Resources& resources, std::string_view world_path, std::vector<SaveClass> classes,
                           SaveFingerprint& output, SaveError& error)
    {
        error = {};
        SaveFingerprint result;
        result.edition = resources.demo()         ? SaveEdition::Demo
                         : resources.nightmares() ? SaveEdition::Nightmares
                                                  : SaveEdition::Retail;
        if (resources.mounts().empty() || resources.mounts().size() > save_format::max_mounts)
        {
            error = {SaveFault::Bound, "Fingerprint needs 1.." + std::to_string(save_format::max_mounts) + " mounts"};
            return false;
        }
        for (const auto& mount : resources.mounts())
        {
            SaveMount saved{mount.role, {}, mount.size, mount.directory_digest};
            if (!resource_path(path_text(mount.path.filename()), saved.name) ||
                saved.name.size() > save_format::max_archive_name)
            {
                error = {SaveFault::Format, "Archive name cannot be normalized: " + path_text(mount.path.filename())};
                return false;
            }
            result.mounts.push_back(std::move(saved));
        }
        std::string text;
        if (!resource_index_digest(resources, result.index, text))
        {
            error = {SaveFault::Format, text};
            return false;
        }
        std::string world;
        if (!resource_path(world_path, world) || !resources.find(world))
        {
            error = {SaveFault::Fingerprint, "World " + std::string(world_path) + " is not in the mounted resources"};
            return false;
        }
        std::vector<std::uint8_t> bytes;
        if (!resources.read(world, bytes, text))
        {
            error = {SaveFault::Io, text};
            return false;
        }
        result.world = sha256(bytes);
        for (std::size_t i = 0; i < classes.size(); ++i)
            if (classes[i].name.empty() || classes[i].name.size() > save_format::max_class_name ||
                classes[i].payload_version == 0 || (i && !(classes[i - 1].name < classes[i].name)))
            {
                error = {SaveFault::Format, "Class schema must be unique, ascending and versioned"};
                return false;
            }
        if (classes.size() > save_format::max_classes)
        {
            error = {SaveFault::Bound, "Class schema exceeds " + std::to_string(save_format::max_classes) + " classes"};
            return false;
        }
        result.class_schema = class_schema_digest(classes);
        result.classes = std::move(classes);
        output = std::move(result);
        return true;
    }
    bool check_fingerprint(const SaveFingerprint& saved, const SaveFingerprint& current,
                           const SaveClassSchema& registered, SaveError& error)
    {
        error = {};
        std::vector<std::string> differences;
        if (saved.edition != current.edition)
            differences.push_back(std::string("edition ") + edition_name(saved.edition) + ", installed " +
                                  edition_name(current.edition));
        if (saved.world != current.world)
            differences.push_back("world DAT differs");
        for (const auto& c : saved.classes)
        {
            const auto found = registered.find(c.name);
            if (found == registered.end())
                differences.push_back("class " + c.name + " is not registered");
            else if (found->second != c.payload_version)
                differences.push_back("class " + c.name + " payload version " + std::to_string(c.payload_version) +
                                      ", build " + std::to_string(found->second));
        }
        const auto find = [](const std::vector<SaveMount>& mounts, const std::string& name) -> const SaveMount*
        {
            for (const auto& mount : mounts)
                if (mount.name == name)
                    return &mount;
            return nullptr;
        };
        bool mounts_differ = false;
        for (const auto& mount : saved.mounts)
        {
            const auto* other = find(current.mounts, mount.name);
            if (!other)
                differences.push_back(std::string(role_name(mount.role)) + " archive " + mount.name + " removed");
            else if (other->role != mount.role || other->size != mount.size || other->directory != mount.directory)
                differences.push_back(std::string(role_name(mount.role)) + " archive " + mount.name + " changed");
            mounts_differ |= !other || !(*other == mount);
        }
        for (const auto& mount : current.mounts)
            if (!find(saved.mounts, mount.name))
            {
                differences.push_back(std::string(role_name(mount.role)) + " archive " + mount.name + " added");
                mounts_differ = true;
            }
        if (!mounts_differ && saved.mounts != current.mounts)
            differences.push_back("archive mount order differs");
        else if (!mounts_differ && saved.index != current.index)
            differences.push_back("effective resource index differs");
        if (differences.empty())
            return true;
        error.fault = SaveFault::Fingerprint;
        error.text = "Save does not match this installation: ";
        for (std::size_t i = 0; i < differences.size(); ++i)
            error.text += (i ? "; " : "") + differences[i];
        return false;
    }
} // namespace OpenBlood2
