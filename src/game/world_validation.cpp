#include "game/world_validation.h"
#include "engine/world_view.h"
#include "game/original/ClientLightningSFX.h"
#include "game/original/Minefield.h"
#include "game/original/SkyObjects.h"
#include "game/original/SparksObj.h"
#include "game/server_adapter.h"
#include <set>

namespace OpenBlood2
{
    namespace
    {
        bool omitted_emitter_sound(const GameSession& game, const ObjectRecord& object, std::string_view path,
                                   std::string_view failure)
        {
            if ((!failure.empty() && failure != "Missing or oversized sound resource") || game.resources.find(path))
                return false;
            // Exact absent shipped references. These original callers continue visual/gameplay work after PlaySound
            // failure.
            struct Row
            {
                const char* world;
                const char* type;
                const char* name;
                const char* sound;
            };
            constexpr Row rows[]{
                {"worlds/20_powerstation.dat", "SparksObj", "CVBC", "Sounds\\lightpop2.wav"},
                {"worlds/20_powerstation.dat", "CClientLightningSFX", "KABOOM", "Sounds\\Thunder.wav"},
                {"worlds/25_ancientone.dat", "CClientLightningSFX", "Lightning1", "Sounds\\Thunder.wav"},
                {"worlds/25_ancientone.dat", "CClientLightningSFX", "Lightning2", "Sounds\\Thunder.wav"},
                {"worlds/25_ancientone.dat", "CClientLightningSFX", "Lightning3", "Sounds\\Thunder.wav"},
                {"worlds/25_ancientone.dat", "CClientLightningSFX", "Lightning4", "Sounds\\Thunder.wav"},
                {"worlds/25_ancientone.dat", "CClientLightningSFX", "Lightning5", "Sounds\\Thunder.wav"}};
            for (const auto& row : rows)
                if (game.world_path == row.world && object.type->name == row.type &&
                    std::string_view(object.state.m_Name) == row.name && path == row.sound)
                    for (std::size_t i = 0; i < game.entries.size() && i < game.world.objects.size(); ++i)
                        if (game.entries[i].object == object.id && game.entries[i].state == EntryState::Created)
                            if (const auto* property = game.world.objects[i].find(
                                    object.type->name == "SparksObj" ? "SparkSound" : "Sound"))
                                if (const auto* authored = std::get_if<std::string>(property);
                                    authored && *authored == path)
                                    return true;
            return false;
        }
    } // namespace

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
            report.omitted += row.state == EntryState::MissingWorldModel ||
                              row.state == EntryState::AbsentOriginalClass || row.state == EntryState::MissingResource;
            report.entries.push_back(std::move(row));
        }
        for (auto id : game.objects.snapshot())
            if (const auto* r = game.objects.get(id); r && r->life == Life::Alive)
                if (const auto* mine = dynamic_cast<const Minefield*>(r->game.get()); mine && !mine->error.empty())
                {
                    error = "Minefield " + std::string(r->state.m_Name) + ": " + mine->error;
                    return false;
                }
        for (auto id : game.objects.snapshot())
            if (const auto* r = game.objects.get(id); r && r->life == Life::Alive)
                if (const auto* emitter = dynamic_cast<const SparksObj*>(r->game.get()))
                {
                    // Exact shipped CVBC sound absence. Keep the emitter and real particles;
                    // the original PlaySparkSound failure does not stop AddSparks.
                    const bool absent_authored_sound =
                        omitted_emitter_sound(game, *r, emitter->sound, emitter->sound_error);
                    if (!emitter->error.empty() || (!emitter->sound_error.empty() && !absent_authored_sound))
                    {
                        error = "SparksObj " + std::string(r->state.m_Name) + ": " + emitter->error + " " +
                                emitter->sound_error;
                        return false;
                    }
                    if (absent_authored_sound)
                        for (auto& entry : report.entries)
                            if (entry.class_name == "SparksObj" && entry.name == "CVBC")
                                entry.detail += "; missing exact authored sound Sounds\\lightpop2.wav; particles "
                                                "retained, no sound alias";
                }
        for (auto id : game.objects.snapshot())
            if (const auto* r = game.objects.get(id); r && r->life == Life::Alive)
                if (const auto* emitter = dynamic_cast<const CClientLightningSFX*>(r->game.get()))
                {
                    const bool absent_authored_sound =
                        omitted_emitter_sound(game, *r, emitter->sound, emitter->sound_error);
                    if (!emitter->error.empty() || (!emitter->sound_error.empty() && !absent_authored_sound))
                    {
                        error = "Lightning " + std::string(r->state.m_Name) + ": " + emitter->error + " " +
                                emitter->sound_error;
                        return false;
                    }
                    if (absent_authored_sound)
                        for (auto& entry : report.entries)
                            if (entry.class_name == "CClientLightningSFX" && entry.name == r->state.m_Name)
                                entry.detail += "; missing exact authored sound Sounds\\Thunder.wav; bolt/damage "
                                                "retained, no sound alias";
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
        // Container BSP faces are never drawn (volumes, conveyors, liquids): their textures
        // cannot block presentation. Any other model may be drawn now or later (debris).
        std::vector<bool> container_only(game.world.models.size(), false), world_bound(game.world.models.size(), false);
        for (auto id : game.objects.snapshot())
            if (const auto* r = game.objects.get(id); r && r->world_model_index < game.world.models.size())
                (r->container() ? container_only : world_bound)[r->world_model_index] = true;
        std::set<std::string> drawn;
        for (std::size_t i = 0; i < game.world.models.size(); ++i)
            if (i == 0 || !container_only[i] || world_bound[i])
                for (const auto& path : game.world.models[i].textures)
                    if (std::string key; resource_path(path, key))
                        drawn.insert(key);
        std::set<std::string> checked;
        std::size_t bytes = mesh.texture_bytes;
        for (const auto& material : mesh.materials)
        {
            if (!material.error.empty() && drawn.contains(material.path))
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
                if (!definition || !image(weapon.skin_path))
                    return false;
            }
        report.checked_textures = checked.size();
        return true;
    }
} // namespace OpenBlood2
