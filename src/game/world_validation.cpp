#include "game/world_validation.h"
#include "engine/world_view.h"
#include "game/original/SkyObjects.h"
#include "game/server_adapter.h"
#include <set>

namespace OpenBlood2
{
    bool check_world_initialization(GameSession& game, WorldValidation& report, std::string& error)
    {
        // Run normal original callbacks first. A registered class or a successful
        // player spawn alone is not permission to discard the old world.
        report.entries.clear();
        if (game.entries.size() != game.world.objects.size())
        {
            error = "World source-entry accounting is incomplete";
            return false;
        }
        report.unknown = report.failed = report.omitted = 0;
        for (std::size_t i = 0; i < game.entries.size(); ++i)
        {
            const auto& source = game.world.objects[i];
            const auto& result = game.entries[i];
            DestinationTrialEntry row{source.class_name, {}, result.detail, result.state};
            if (const auto* value = source.find("Name"))
                if (const auto* name = std::get_if<std::string>(value))
                    row.name = *name;
            report.unknown += row.state == EntryState::Unsupported;
            report.failed += row.state == EntryState::Failed;
            report.omitted += row.state == EntryState::MissingWorldModel;
            report.entries.push_back(std::move(row));
        }
        report.unavailable_services.clear();
        for (const auto& [name, count] : game.server.unavailable_services)
            if (count)
                report.unavailable_services.push_back(name);
        if (report.unknown || report.failed || !report.unavailable_services.empty())
        {
            error = "World initialization rejected: " + std::to_string(report.unknown) + " unsupported classes, " +
                    std::to_string(report.failed) + " failed entries, " +
                    std::to_string(report.unavailable_services.size()) + " unavailable services";
            return false;
        }
        return true;
    }
    bool check_world_presentation(GameSession& game, WorldValidation& report, std::string& error)
    {
        WorldMesh mesh;
        if (!mesh.build(game.world, error, &game.resources))
            return false;
        std::set<std::string> checked;
        std::size_t bytes = mesh.texture_bytes;
        for (const auto& material : mesh.materials)
        {
            if (!material.error.empty())
            {
                error = "World texture " + material.path + ": " + material.error;
                return false;
            }
            if (!material.path.empty() && material.frames.empty())
                checked.insert(material.path);
        }
        auto image = [&](std::string_view path)
        {
            std::string normalized;
            if (!resource_path(path, normalized))
            {
                error = "Invalid world image path";
                return false;
            }
            if (checked.contains(normalized))
                return true;
            Texture texture;
            if (!read_texture_resource(game.resources, normalized, texture, error))
            {
                error = "World image " + normalized + ": " + error;
                return false;
            }
            if (checked.size() >= 4096 || bytes + texture.rgba.size() > 128 * 1024 * 1024)
            {
                error = "World presentation resource budget exceeded";
                return false;
            }
            bytes += texture.rgba.size();
            checked.insert(std::move(normalized));
            return true;
        };
        auto pending_sky = game.server.sky;
        for (auto id : game.objects.snapshot())
        {
            const auto& object = *game.objects.get(id);
            if (object.life != Life::Alive)
                continue;
            if (const auto* pointer = dynamic_cast<const SkyPointer*>(object.game.get()))
            {
                // Preflight without consuming the original first update or advancing simulation.
                auto* names = game.server.FindNamedObjects(pointer->target.c_str());
                const auto h = names && names->m_pFirstLink ? names->m_pFirstLink->m_hObject : nullptr;
                const bool valid = pending_sky.add(game.objects, h, unsigned(pointer->index));
                if (!valid)
                    error = pending_sky.error;
                if (names)
                    game.server.RelinquishList(names);
                if (!valid)
                    return false;
            }
            if (!object.model_error.empty())
            {
                error = "World object " + std::string(object.state.m_Name) + ": " + object.model_error;
                return false;
            }
            if (object.model && object.state.m_SkinName[0] && !image(object.state.m_SkinName))
                return false;
            if (object.sprite)
                for (const auto& frame : object.sprite->frames)
                    if (!image(frame))
                        return false;
        }
        for (const auto& instance : game.server.particle_streams.active)
            if (game.server.IsObjectAlive(instance.owner) && !image(instance.effect.texture))
                return false;
        // First-person models are client-owned, outside the server object table.
        for (const auto& weapon : game.client_shell.weapons)
            if (weapon.model)
            {
                const auto* definition = weapon_definition(weapon.type);
                if (!definition || !image(definition->m_szViewModelSkin))
                    return false;
            }
        report.checked_textures = checked.size();
        return true;
    }
} // namespace OpenBlood2
