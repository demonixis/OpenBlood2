#pragma once
#include "game/legacy/basedefs_de.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Save-message primitives (save_load_contracts "Message primitives"). These are
// OpenBlood2 bounds and context types, not the undocumented 1998 record layout.
namespace OpenBlood2
{
    inline constexpr std::uint32_t save_null_reference = 0xFFFFFFFFu;
    inline constexpr std::size_t max_message_string = 4096; // String/HString payload bytes.
    inline constexpr std::size_t max_nested_message = 16384;
    inline constexpr std::size_t max_save_references = 65536; // Save-list record count bound.

    // Dense save-list index table. Saving maps a handle to its record index;
    // loading maps the index back to the restored handle. Order is the caller's
    // save order; this table never inspects or owns the objects.
    class SaveIndex
    {
      public:
        bool add(HOBJECT handle)
        {
            if (!handle || handles_.size() >= max_save_references || indices_.contains(handle))
                return false;
            indices_.emplace(handle, static_cast<std::uint32_t>(handles_.size()));
            handles_.push_back(handle);
            return true;
        }
        // Missing handles are outside the list and are saved as NULL references.
        bool find(HOBJECT handle, std::uint32_t& index) const
        {
            const auto found = indices_.find(handle);
            if (found == indices_.end())
                return false;
            index = found->second;
            return true;
        }
        HOBJECT at(std::uint32_t index) const
        {
            return index < handles_.size() ? handles_[index] : nullptr;
        }
        std::size_t size() const
        {
            return handles_.size();
        }

      private:
        std::vector<HOBJECT> handles_;
        std::unordered_map<HOBJECT, std::uint32_t> indices_;
    };

    // One MID_SAVEOBJECT/MID_LOADOBJECT dispatch. Counters are per object, so a
    // caller can aggregate them per class; they never change payload bytes.
    struct SavePayloadReport
    {
        std::string class_name;
        std::size_t bytes = 0, consumed = 0;
        std::uint32_t references = 0;      // LoadSave references resolved to a record.
        std::uint32_t null_references = 0; // NULL, stale, pending or outside the list.
        std::uint32_t plain_objects = 0;   // Plain object writes/reads stored as NULL (D7).
        std::uint32_t nested = 0;          // Nested messages written or read.
        std::uint32_t impure = 0;          // Services refused by the save purity check.
        std::string error;                 // First latched failure; empty on success.
    };

    // Per-class aggregate of one SaveObjects/RestoreObjects call (D7 counts plain-object NULLs per class).
    struct SaveClassCount
    {
        std::string name;
        std::uint32_t records = 0, references = 0, null_references = 0, plain_objects = 0;
        std::size_t bytes = 0;
    };
    // Owned report values; handles are checked, non-reused session tokens.
    struct SaveObjectsReport
    {
        std::string file; // Logical store name.
        std::uint32_t listed = 0, saved = 0, always_load = 0, pending = 0, duplicates = 0;
        std::uint32_t links = 0, attachments = 0;
        std::vector<SaveClassCount> classes;
        std::vector<std::string> refused; // "Class (support)" for listed classes that cannot be saved.
        SavePayloadReport failed;         // The failing dispatch, if any.
        std::string error;
    };
    struct RestoreObjectsReport
    {
        std::string file;
        std::uint32_t records = 0, links = 0, existing_links = 0, attachments = 0, existing_attachments = 0;
        bool console_restored = false, clock_restored = false;
        std::vector<HOBJECT> objects; // Record order.
        HOBJECT local_client = nullptr;
        std::vector<SaveClassCount> classes;
        SavePayloadReport failed;
        std::string error;
    };
} // namespace OpenBlood2
