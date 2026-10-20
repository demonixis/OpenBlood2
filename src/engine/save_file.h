#pragma once
#include "engine/resources.h"
#include "engine/sha256.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

// OpenBlood2 save container (save_load_contracts.md, "Save file format" and "Fingerprint").
// Little-endian, bounded, CRC-checked sections in one fixed order. Version 1 has no migration:
// any other container or section version is refused. CLCK, OBJS, LINK and ATCH are bounded
// opaque payloads here; their contents belong to the engine-record lot.
namespace OpenBlood2
{
    namespace save_format
    {
        inline constexpr std::array<std::uint8_t, 8> magic{'O', 'B', '2', 'S', 'A', 'V', 'E', 0x1A};
        inline constexpr std::uint32_t container_version = 1;
        inline constexpr std::uint32_t section_version = 1; // Every version-1 section.
        inline constexpr std::size_t header_size = 24, section_overhead = 20;
        inline constexpr std::uint64_t max_file = 64ull << 20, max_level_entry = 32ull << 20;
        inline constexpr std::size_t max_meta = 4096, max_fingerprint = 64 << 10, max_clock = 16 << 10;
        inline constexpr std::size_t max_links = 16 << 20, max_attachments = 16 << 20;
        inline constexpr std::size_t max_display_name = 255, max_world_title = 255, max_world_path = 1024;
        inline constexpr std::size_t max_version_text = 64, max_mounts = 32, max_archive_name = 255;
        inline constexpr std::size_t max_classes = 1024, max_class_name = 64;
        inline constexpr std::size_t max_console = 256, max_console_name = 64, max_console_value = 1024;
        inline constexpr std::size_t max_console_payload =
            4 + max_console * (8 + max_console_name + max_console_value);
        // Tags are their four ASCII bytes in file order; END is padded with a space.
        constexpr std::uint32_t tag(const char (&text)[5])
        {
            return std::uint32_t(std::uint8_t(text[0])) | std::uint32_t(std::uint8_t(text[1])) << 8 |
                   std::uint32_t(std::uint8_t(text[2])) << 16 | std::uint32_t(std::uint8_t(text[3])) << 24;
        }
        inline constexpr std::uint32_t meta = tag("META"), fingerprint = tag("FPRT"), console = tag("CONS"),
                                       clock = tag("CLCK"), objects = tag("OBJS"), links = tag("LINK"),
                                       attachments = tag("ATCH"), level_entry = tag("AUTO"), end = tag("END ");
    } // namespace save_format

    // Reason categories the menu shows for an unreadable or refused save.
    enum class SaveFault : std::uint8_t
    {
        None,
        Io,          // The file cannot be opened or read.
        Bound,       // A size, count or string exceeds its limit.
        Truncated,   // The file or a section ends early, or the declared length differs.
        Format,      // Bad magic, malformed field, trailing bytes or an inexact payload.
        Version,     // A container or section version this build does not read.
        Crc,         // A section checksum mismatch.
        Order,       // An unknown, missing, duplicate or misplaced section.
        Fingerprint, // Edition, data, mods, world or class schema differ from this installation.
    };
    const char* save_fault_name(SaveFault);
    struct SaveError
    {
        SaveFault fault = SaveFault::None;
        std::string text;
    };

    enum class SaveKind : std::uint8_t
    {
        Slot = 0,
        Quick = 1,
        LevelEntry = 2 // The Restart snapshot; also the nested AUTO container.
    };
    struct SaveMeta
    {
        SaveKind kind = SaveKind::Slot;
        std::uint8_t slot = 0;    // 0..7 for Slot, 0 otherwise.
        std::string display_name; // World title plus the original "[Mmm dd hh:mm:ss]" suffix.
        std::uint64_t utc_seconds = 0;
        std::string world_path; // Normalized DAT resource path.
        std::string world_title;
        std::uint32_t game_type = 0, character = 0, difficulty = 0;
        std::string version; // OpenBlood2 version text (informational).
        double simulated_seconds = 0;
        bool operator==(const SaveMeta&) const = default;
    };

    enum class SaveEdition : std::uint8_t
    {
        Retail = 0,
        Demo = 1
    };
    struct SaveMount
    {
        MountRole role = MountRole::Base;
        std::string name; // Normalized archive filename, never a host path.
        std::uint64_t size = 0;
        Sha256Digest directory{};
        bool operator==(const SaveMount&) const = default;
    };
    struct SaveClass
    {
        std::string name;
        std::uint32_t payload_version = 1;
        bool operator==(const SaveClass&) const = default;
    };
    struct SaveFingerprint
    {
        SaveEdition edition = SaveEdition::Retail;
        std::vector<SaveMount> mounts;   // Mount order.
        Sha256Digest index{};            // Effective resource index.
        Sha256Digest world{};            // World DAT bytes.
        std::vector<SaveClass> classes;  // Strictly ascending names (byte order).
        Sha256Digest class_schema{};     // class_schema_digest(classes); checked on read.
        bool operator==(const SaveFingerprint&) const = default;
    };
    struct ConsoleVariable
    {
        std::string name, value;
        bool operator==(const ConsoleVariable&) const = default;
    };
    struct SaveContainer
    {
        SaveMeta meta;
        SaveFingerprint fingerprint;
        std::vector<ConsoleVariable> console; // File order; unique names.
        std::vector<std::uint8_t> clock, objects, links, attachments;
        // A complete level-entry container without its own AUTO, kept as validated bytes.
        std::optional<std::vector<std::uint8_t>> level_entry;
        bool operator==(const SaveContainer&) const = default;
    };

    // SHA-256 of u32 count, then per class u32 name length, name bytes, u32 payload version.
    Sha256Digest class_schema_digest(std::span<const SaveClass>);
    // Validates every field and bound before producing bytes; a refused container writes nothing.
    bool encode_save(const SaveContainer&, std::vector<std::uint8_t>&, SaveError&);
    // Parses and validates the whole file, including the nested AUTO container. Output changes only on success.
    bool decode_save(std::span<const std::uint8_t>, SaveContainer&, SaveError&);
    // Header and META only. `file_size` is the whole file's length, `prefix` its first bytes.
    bool read_save_meta(std::span<const std::uint8_t> prefix, std::uint64_t file_size, SaveMeta&, SaveError&);
    bool read_save_meta_file(const std::filesystem::path&, SaveMeta&, SaveError&);
    bool read_save_file(const std::filesystem::path&, SaveContainer&, SaveError&);
    // Encodes and replaces the file atomically; the previous file survives any failure.
    bool write_save_file(const std::filesystem::path&, const SaveContainer&, SaveError&);
} // namespace OpenBlood2
