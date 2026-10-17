#include "keyframe_smoke.h"
#include "engine/world_model.h"
#include "game/original/KeyFramer.h"
#include <spdlog/spdlog.h>
namespace OpenBlood2
{
    bool prepare_keyframe_smoke(GameSession& game, ObjectId& selected, bool world_path)
    {
        if (game.world_path == "worlds/12_train_station.dat")
        {
            if (!game.step(1.0 / 60))
                return false;
            for (auto id : game.objects.snapshot())
            {
                const auto* record = game.objects.get(id);
                auto* f = record ? dynamic_cast<KeyFramer*>(record->game.get()) : nullptr;
                if (!f || f->base_key_name != "tr")
                    continue;
                selected = id;
                const auto controller = record->handle;
                auto text = game.server.CreateString("ON");
                auto message = game.server.StartMessageToObject(nullptr, controller, 1000);
                const bool ok = text && message && game.server.WriteToMessageHString(message, text) == DE_OK &&
                                game.server.EndMessage(message) == DE_OK;
                game.server.FreeString(text);
                if (!ok)
                    return false;
                // Diagnostic ON only: retain authored six-second delay, keys,
                // world-model target and normal session/audio updates.
                for (unsigned frame = 0; frame < 720 && !f->sound_requests; ++frame)
                    if (!game.step(1.0 / 60) || !game.server.IsObjectAlive(controller))
                        return false;
                return f->sound_starts == 1 && !f->sound_failures;
            }
            return false;
        }
        for (auto id : game.objects.snapshot())
            if (const auto* r = game.objects.get(id);
                r && std::string_view(r->state.m_Name) == (world_path ? "htrain1" : "tracking1") &&
                dynamic_cast<const KeyFramer*>(r->game.get()))
            {
                selected = id;
                if (world_path)
                {
                    // Source ON is ignored before BuildKeyList; initialize normally.
                    if (!game.step(1.0 / 60))
                        return false;
                    const auto* live = game.objects.get(selected);
                    if (!live || live->life != Life::Alive)
                        return false;
                    auto text = game.server.CreateString("ON");
                    auto message = game.server.StartMessageToObject(nullptr, live->handle, 1000);
                    const bool ok = text && message && game.server.WriteToMessageHString(message, text) == DE_OK &&
                                    game.server.EndMessage(message) == DE_OK;
                    game.server.FreeString(text);
                    return ok;
                }
                return true;
            }
        return false;
    }
    bool check_keyframe_smoke(GameSession& game, ObjectId selected, bool world_path)
    {
        const auto* r = game.objects.get(selected);
        const auto* f = r ? dynamic_cast<const KeyFramer*>(r->game.get()) : nullptr;
        if (game.world_path == "worlds/12_train_station.dat")
        {
            const auto* voice = f ? game.server.audio.get(f->sound_voice) : nullptr;
            if (!f || !f->initialized || f->targets.size() != 1 || !voice || !voice->clip || !voice->clip->frames() ||
                !f->sound_starts || f->sound_failures || voice->request.speaker != f->targets[0].object ||
                !game.server.IsObjectAlive(voice->owner) || voice->cursor <= 0 || !voice->request.loop ||
                voice->request.outer_radius != 6000 || voice->request.inner_radius != 600 ||
                !game.server.audio.has_output() || !game.server.audio.submitted_frames ||
                !game.server.audio.output_error.empty())
                return false;
            spdlog::info("Authored key audio: {} frames={}, cursor={}, submitted={}, attached=true", f->last_sound,
                         voice->clip->frames(), voice->cursor, game.server.audio.submitted_frames);
            return true;
        }
        if (!f || !f->initialized || f->keys.size() != (world_path ? 3 : 2) || f->targets.size() != 1 || !f->active ||
            !f->targets[0].moves || f->targets[0].result != MoverResult::Moved ||
            f->unresolved_messages != 0) // The authored Gideon recipient is now instantiated.
            return false;
        const auto* target = game.objects.get(f->targets[0].object);
        if (!target || (!world_path && game.client_shell.view.object != target->handle))
            return false;
        const auto& a = f->keys[0];
        const auto& b = f->keys[1];
        const auto expected = vector_add(f->targets[0].offset,
                                         vector_add(a.position, vector_scale(vector_subtract(b.position, a.position),
                                                                             (f->time - a.time) / (b.time - a.time))));
        auto delta = vector_subtract(expected, world_path ? target->state.m_Pos : game.client_shell.view.position);
        if (world_path)
        {
            if (!target->world_model || !target->world_model_base || target->mover_blocks || !target->mover_moves)
                return false;
            CollisionHit hit;
            const auto center = object_bounds_center(*target);
            if (sweep_object(*target, vector_add(center, {0, 300, 0}), vector_add(center, {0, -300, 0}), {}, hit) !=
                CollisionQuery::Hit)
                return false;
        }
        return vector_dot(delta, delta) < .0001f;
    }
} // namespace OpenBlood2
