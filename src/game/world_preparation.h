#pragma once
#include "engine/collision.h"
#include "game/session.h"
namespace OpenBlood2
{
    // Owned CPU data only, with no session handles, callbacks, clocks or live resources.
    struct PreparedWorld
    {
        World world;
        CollisionWorld collision;
    };
    bool prepare_world(const WorldRequest&, const Resources&, PreparedWorld&, std::string&);
    bool editor_only_world_class(std::string_view);
    enum class DestinationEntryState
    {
        SchemaChecked,
        EditorOnly,
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
                    invalid = 0;
        std::vector<DestinationEntry> entries;
    };
} // namespace OpenBlood2
