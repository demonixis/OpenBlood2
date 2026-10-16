#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool editor_only_world_class(std::string_view name)
    {
        return name == "DirLight" || name == "Light" || name == "OutsideDef" || name == "ObjectLight" ||
               name == "LightAnimator" || name == "GlobalDirLight";
    }
    bool prepare_world(const WorldRequest& request, const Resources& resources, PreparedWorld& output,
                       std::string& error)
    {
        output = {};
        error.clear();
        std::string normalized;
        if (request.game_type != 0 || request.difficulty > 3 ||
            (request.load_type != LOADTYPE_NEW_GAME && request.load_type != LOADTYPE_NEW_LEVEL) ||
            !resource_path(request.filename, normalized) || normalized != request.filename ||
            !normalized.starts_with("worlds/") || !normalized.ends_with(".dat"))
        {
            error = "World preparation requires a normalized single-player request";
            return false;
        }
        PreparedWorld next;
        std::vector<std::uint8_t> bytes;
        if (!resources.read(request.filename, bytes, error) || !read_world(bytes, next.world, error))
            return false;
        if (next.world.models.empty())
        {
            error = "World has no primary collision model";
            return false;
        }
        if (!next.collision.build(next.world.models.front(), error))
            return false;
        output = std::move(next);
        return true;
    }
    const char* destination_entry_state_name(DestinationEntryState state)
    {
        switch (state)
        {
            case DestinationEntryState::SchemaChecked:
                return "Schema checked; runtime unverified";
            case DestinationEntryState::EditorOnly:
                return "Preprocessor only";
            case DestinationEntryState::UnknownClass:
                return "Class not registered";
            case DestinationEntryState::InvalidProperties:
                return "Invalid registered properties";
        }
        return "Unknown";
    }
    bool GameSession::inspect_destination(const WorldMessage& message, std::string_view start, std::string& error)
    {
        error.clear();
        // A reentrant inspector must not overwrite the outer result or touch world state.
        if (objects.busy())
        {
            error = "Destination inspection during a callback rejected";
            return false;
        }
        DestinationInspection next;
        next.attempts = destination_inspection.attempts + 1;
        next.source_revision = world_revision;
        next.source_world = world_path;
        const auto failed = [&]
        {
            next.error = error;
            destination_inspection = std::move(next);
            return false;
        };
        if (start.size() > 100 || start.find('\0') != std::string_view::npos)
        {
            error = "Invalid destination start-point name";
            return failed();
        }
        next.requested_start = start;
        WorldRequest request;
        if (!parse_world_request(message, request, error))
            return failed();
        next.destination = request.filename;
        PreparedWorld prepared;
        if (!prepare_world(request, resources, prepared, error))
            return failed();
        next.models = prepared.world.models.size();
        next.triangles = prepared.collision.triangles();
        next.collision_nodes = prepared.collision.nodes();
        next.entries.reserve(prepared.world.objects.size());
        for (const auto& source : prepared.world.objects)
        {
            DestinationEntry entry;
            entry.class_name = source.class_name;
            if (const auto* v = source.find("Name"))
                if (const auto* name = std::get_if<std::string>(v))
                    entry.name = *name;
            if (const auto* v = source.find("Pos"))
                if (const auto* pos = std::get_if<DVector>(v))
                {
                    entry.position = *pos;
                    entry.has_position = true;
                }
            if (editor_only_world_class(source.class_name))
            {
                entry.state = DestinationEntryState::EditorOnly;
                ++next.editor_only;
            }
            else if (const auto type = objects.find_class(source.class_name))
            {
                Properties properties;
                for (const auto& property : source.properties)
                    properties.emplace(property.name, property.value);
                if (objects.validate_properties(type, properties, entry.detail) == ObjectError::None)
                {
                    entry.state = DestinationEntryState::SchemaChecked;
                    ++next.schema_checked;
                }
                else
                {
                    entry.state = DestinationEntryState::InvalidProperties;
                    ++next.invalid;
                }
            }
            else
            {
                ++next.unknown;
                entry.detail = "Required original class is not registered";
            }
            next.entries.push_back(std::move(entry));
        }
        next.scan_complete = true;
        destination_inspection = std::move(next);
        // True means the read-only scan completed, never that a transfer is ready.
        return true;
    }
} // namespace OpenBlood2
