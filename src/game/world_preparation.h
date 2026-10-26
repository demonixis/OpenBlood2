#pragma once
#include "engine/collision.h"
#include "engine/visibility.h"
#include "game/session.h"
namespace OpenBlood2
{
    // Owned CPU data only, with no session handles, callbacks, clocks or live resources.
    struct PreparedWorld
    {
        World world;
        CollisionWorld collision;
        WorldVisibility visibility;
    };
    bool prepare_world(const WorldRequest&, const Resources&, PreparedWorld&, std::string&);
    bool editor_only_world_class(std::string_view);
    bool absent_original_world_class(std::string_view);
    enum class DestinationEntryState
    {
        SchemaChecked,
        EditorOnly,
        AbsentOriginalClass,
        UnknownClass,
        InvalidProperties
    };
    const char* destination_entry_state_name(DestinationEntryState);
    struct DestinationEntry
    {
        std::string class_name, name, detail;
        DestinationEntryState state = DestinationEntryState::UnknownClass;
        DVector position{};
        bool has_position = false;
    };
    struct DestinationInspection
    {
        std::uint64_t attempts = 0, source_revision = 0;
        std::string source_world, destination, requested_start, error;
        bool scan_complete = false;
        std::size_t models = 0, triangles = 0, collision_nodes = 0, schema_checked = 0, editor_only = 0, unknown = 0,
                    invalid = 0, omitted = 0;
        std::vector<DestinationEntry> entries;
    };
    struct DestinationTrialEntry
    {
        std::string class_name, name, detail;
        EntryState state = EntryState::Unsupported;
    };
    struct DestinationTrial
    {
        std::uint64_t attempts = 0, source_revision = 0;
        std::string source_world, destination, requested_start, selected_start, error;
        bool world_loaded = false, player_entered = false, cleanup_complete = false, entry_camera_active = false;
        float health = 0, armor = 0, bullets = 0;
        std::size_t keys = 0;
        std::size_t created = 0, sounds = 0, collected = 0, editor_only = 0, filtered = 0, omitted = 0, unknown = 0,
                    failed = 0;
        std::size_t runtime_objects = 0, attachments = 0, voices = 0;
        std::vector<DestinationTrialEntry> entries;
    };
    enum class LevelChangeResult
    {
        None,
        Rejected,
        Committed
    };
    struct LevelChangeRequest
    {
        WorldMessage message;
        std::string start;
    };
    struct WorldValidation
    {
        std::size_t unknown = 0, failed = 0, omitted = 0, checked_textures = 0;
        std::vector<DestinationTrialEntry> entries;
        std::vector<std::string> unavailable_services;
    };
    struct SessionStartReport : WorldValidation
    {
        std::uint64_t attempts = 0, revision = 0;
        double time = 0;
        bool accepted = false, player_entered = false;
        std::string error;
    };
    struct LevelChangeReport : WorldValidation
    {
        std::uint64_t attempts = 0, committed = 0, rejected = 0, source_revision = 0, destination_revision = 0;
        std::string source_world, destination, requested_start, selected_start, error;
        bool player_captured = false, world_loaded = false, player_entered = false, cleanup_complete = false;
        bool source_retained = true, start_fallback = false;
        float health = 0, armor = 0, bullets = 0;
        std::size_t keys = 0;
    };
} // namespace OpenBlood2
