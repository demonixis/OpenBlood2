#include "ai_model_key_smoke.h"
#include "game/original/AI_Mgr.h"
#include <limits>
namespace OpenBlood2
{
    bool prepare_ai_model_key_smoke(GameSession& g, ObjectId& id)
    {
        // Diagnostic registration only: never satisfies a required world AI class.
        static const ClassInfo type{"AIModelKeyProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AI_Mgr>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return false;
        const auto* player = g.server_shell.Player(g.local_client);
        const auto* body = player ? g.objects.get(player->m_hObject) : nullptr;
        if (!body)
            return false;
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = body->state.m_Pos;
        state.m_Flags = FLAG_MODELKEYS;
        SAFE_STRCPY(state.m_Name, "AI model-key probe (no AI states)");
        SAFE_STRCPY(state.m_Filename, "models/enemies/m_cultist1.abc");
        const auto h = g.objects.create(&type, state);
        auto* r = g.objects.get(h);
        auto* ai = r ? dynamic_cast<AI_Mgr*>(r->game.get()) : nullptr;
        if (!ai || !r->model)
            return false;
        auto table = std::make_shared<CAnim_Sound>();
        if (!table->SetSoundRoot("sounds/enemies/m_cultist") || !table->SetAnimationIndexes(h) ||
            !table->GenerateHitSpheres(h) || !ai->InitStatics(table))
            return false;
        // Choose the earliest real retained-voice key. No marker/asset edits or direct dispatch.
        std::uint32_t earliest = std::numeric_limits<std::uint32_t>::max();
        unsigned selected = 0;
        for (unsigned clip = 0; clip < r->model->animations.size(); ++clip)
            for (const auto& key : r->model->animations[clip].keys)
                if (key.time < earliest && key.text.find("[play_sound:") != std::string::npos &&
                    key.text.find("[sound_voice:1]") != std::string::npos)
                {
                    earliest = key.time;
                    selected = clip;
                }
        if (earliest > 500)
            return false;
        g.server.SetModelAnimation(h, selected);
        if (r->animation.animation != selected)
            return false;
        r->animation.looping = false;
        id = r->id;
        return true;
    }
    bool check_ai_model_key_smoke(GameSession& g, ObjectId id, std::uint64_t minimum_keys)
    {
        const auto* r = g.objects.get(id);
        const auto* ai = r ? dynamic_cast<const AI_Mgr*>(r->game.get()) : nullptr;
        const auto* table = ai ? ai->AnimSound() : nullptr;
        const auto* voice = table ? g.server.audio.get(table->m_hVoxSound) : nullptr;
        std::string resource;
        return voice && resource_path(voice->request.resource, resource) && ai && ai->model_keys >= minimum_keys &&
               ai->audio_starts > 0 && ai->key_failures == 0 && ai->camera_suppressed == 0 && voice &&
               voice->cursor > 0 && voice->owner == r->handle && resource.starts_with("sounds/enemies/m_cultist/") &&
               g.server.audio.submitted_frames > 0 && g.server.audio.output_error.empty();
    }
} // namespace OpenBlood2
