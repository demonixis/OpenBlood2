#pragma once
#include "engine/objects.h"
#include "engine/save_file.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Engine records, clock, links and attachments of a save (save_load_contracts "Engine
// record", lot 3). These are the CLCK/OBJS/LINK/ATCH payloads of the version-1
// container and the in-memory logical store SaveObjects/RestoreObjects use. Pure
// data: no handles, pointers or host layout; every length and value is bounded.
namespace OpenBlood2
{
    namespace save_records
    {
        inline constexpr std::size_t max_records = 65536;    // OBJS record count.
        inline constexpr std::size_t max_payload = 65536;    // Game payload, the existing message bound.
        inline constexpr std::size_t max_class_name = 64;    // As FPRT class names.
        inline constexpr std::size_t max_string = MAX_CS_FILENAME_LEN; // Name, filename, skin, node.
        inline constexpr std::size_t max_hidden_nodes = 4096;
        inline constexpr std::size_t max_sprite_frame = 4096; // Sprite resources are bounded to 4096 frames.
        inline constexpr std::uint32_t no_index = 0xFFFFFFFFu;
        inline constexpr std::uint32_t sky_slots = 30; // Original MAX_SKYOBJECTS.
        inline constexpr double max_delay = 1e9;      // Remaining update/deactivation seconds.
        inline constexpr std::uint8_t world_body = 1, character_body = 2, model_body = 4;
    } // namespace save_records

    // CLCK: simulation clock, world-time origin, global force and the server gameplay
    // generator (std::minstd_rand state, 1..2147483646). Restored before objects (D9, D10).
    struct SaveClock
    {
        double time = 0, frame_time = 0, world_origin = 0;
        DVector global_force{};
        std::uint32_t generator = 1;
        bool operator==(const SaveClock&) const; // Bitwise for floating-point fields.
    };

    // One OBJS record: the class name, OpenBlood2's equivalent of the LithTech engine
    // fields, then the game MID_SAVEOBJECT payload. Delays are remaining seconds
    // relative to the saved clock; an overdue deadline is saved as zero.
    struct SaveEngineRecord
    {
        std::string class_name;
        std::uint16_t type = OT_NORMAL;
        std::uint32_t flags = 0, user_flags = 0;
        DVector position{}, scale{1, 1, 1};
        DRotation rotation{};
        std::string name, filename, skin; // filename is the BSP name of world and container models.
        std::uint16_t container_code = 0;
        // World and container models: the first same-named model in file order and the
        // authored basis rotation (identity and no_index for every other type).
        std::uint32_t world_model_index = save_records::no_index;
        DRotation world_model_basis{};
        DVector color{};
        float alpha = 1;
        DVector dimensions{}, velocity{}, acceleration{};
        std::uint8_t body = 0; // save_records::*_body, at most one bit.
        WorldBodyProperties body_properties;
        std::uint8_t blocking_priority = 0;
        std::uint32_t activity = OBJSTATE_ACTIVE;
        bool auto_deactivate = false;
        double deactivation_period = 0, deactivation_remaining = 0;
        bool scheduled = false;
        double update_remaining = 0;
        std::uint32_t animation = 0;
        double animation_time = 0; // Milliseconds.
        bool animation_looping = true, animation_done = false, animation_first = true;
        std::vector<std::uint8_t> hidden_nodes; // Empty, or one 0/1 byte per model node.
        double sprite_milliseconds = 0;
        std::uint32_t sprite_frame = 0;
        bool sprite_has_frame = true;
        bool in_sky = false;                          // Original INSKY survives replacement of its slot.
        std::uint32_t sky_slot = save_records::no_index; // 0..29 while the object holds a slot.
        bool local_client = false;                    // OpenBlood2's equivalent of the CIF_LOCAL client reference.
        std::vector<std::uint8_t> payload;            // Game MID_SAVEOBJECT bytes.
        bool operator==(const SaveEngineRecord&) const;
    };

    // LINK: an interlink whose ends are both saved, as record indices in link-creation order.
    struct SaveLink
    {
        std::uint32_t owner = 0, target = 0;
        bool operator==(const SaveLink&) const = default;
    };
    // ATCH: an attachment whose parent and child are both saved (the 1998 API attaches by node name).
    struct SaveAttachment
    {
        std::uint32_t parent = 0, child = 0;
        std::string node;
        DVector offset{};
        DRotation rotation{};
        bool operator==(const SaveAttachment&) const;
    };

    bool encode_save_clock(const SaveClock&, std::vector<std::uint8_t>&, SaveError&);
    bool decode_save_clock(std::span<const std::uint8_t>, SaveClock&, SaveError&);
    bool encode_save_records(std::span<const SaveEngineRecord>, std::vector<std::uint8_t>&, SaveError&);
    bool decode_save_records(std::span<const std::uint8_t>, std::vector<SaveEngineRecord>&, SaveError&);
    // Indices are checked against the OBJS record count.
    bool encode_save_links(std::span<const SaveLink>, std::size_t records, std::vector<std::uint8_t>&, SaveError&);
    bool decode_save_links(std::span<const std::uint8_t>, std::size_t records, std::vector<SaveLink>&, SaveError&);
    bool encode_save_attachments(std::span<const SaveAttachment>, std::size_t records, std::vector<std::uint8_t>&,
                                 SaveError&);
    bool decode_save_attachments(std::span<const std::uint8_t>, std::size_t records, std::vector<SaveAttachment>&,
                                 SaveError&);
    // Field rules shared by encoding, decoding and capture; `error` names the first violation.
    bool valid_save_record(const SaveEngineRecord&, std::string& error);
    bool valid_save_clock(const SaveClock&, std::string& error);

    // Remaining seconds to a deadline, zero once it is due (a frame boundary never holds a
    // negative delay). restored_deadline is its exact inverse whenever the clock is the
    // saved one: the returned deadline d satisfies remaining_delay(clock, d) == remaining.
    double remaining_delay(double clock, double deadline);
    double restored_deadline(double clock, double remaining);

    // The original's three logical save files (BloodServerShell.cpp: Save\Current\Current.sav,
    // Auto.sav and the level-change Keep.sav). Lot 6 maps them to slot files.
    enum class SaveStoreSlot : std::uint8_t
    {
        Current,
        Auto,
        Keep
    };
    const char* save_store_slot_name(SaveStoreSlot);
    // What one SaveObjects call produced: encoded section payloads plus the class schema the
    // FPRT section will carry. Stored whole or not at all.
    struct SaveImage
    {
        std::uint32_t param = 0, flags = 0; // SaveObjects dwParam and SAVEOBJECTS_* flags.
        bool console_saved = false;         // SAVEOBJECTS_SAVEGAMECONSOLE.
        std::vector<ConsoleVariable> console;
        std::vector<SaveClass> classes; // Every class present in OBJS, strictly ascending.
        std::vector<std::uint8_t> clock, objects, links, attachments;
        std::uint32_t records = 0;
        bool operator==(const SaveImage&) const = default;
    };
    class SaveStore
    {
      public:
        const SaveImage* find(SaveStoreSlot slot) const
        {
            const auto& image = images_[static_cast<std::size_t>(slot)];
            return image ? &*image : nullptr;
        }
        void put(SaveStoreSlot slot, SaveImage image)
        {
            images_[static_cast<std::size_t>(slot)] = std::move(image);
            ++revision_;
        }
        void erase(SaveStoreSlot slot)
        {
            images_[static_cast<std::size_t>(slot)].reset();
            ++revision_;
        }
        std::uint64_t revision() const
        {
            return revision_;
        }

      private:
        std::array<std::optional<SaveImage>, 3> images_;
        std::uint64_t revision_ = 0;
    };
} // namespace OpenBlood2
