#include "ai_model_key_smoke.h"
#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CivilianAI.h"
#include <cmath>
#include <limits>
namespace OpenBlood2
{
    namespace
    {
        // Explicit original script-state driver, not the complete AI_Update.
        class AIFollowProbe : public AI_Mgr
        {
          public:
            PathMgr* diagnostic_paths = nullptr; // Owner is the enclosing GameSession.

          private:
            DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
            {
                if (m == MID_UPDATE)
                {
                    const bool ok = diagnostic_paths && AI_STATE_Script(*diagnostic_paths);
                    GetServerDE()->SetNextUpdate(m_hObject, ok && m_nState == StateScript ? .001f : 0);
                }
                return AI_Mgr::EngineMessageFn(m, d, f);
            }
        };
        class AIFireProbe : public AI_Mgr
        {
          public:
            HOBJECT diagnostic_target = nullptr;

          private:
            DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT value) override
            {
                if (message == MID_UPDATE)
                {
                    GetServerDE()->SetNextUpdate(m_hObject, .001f);
                    if (!MC_Fire_Stand())
                        GetServerDE()->SetNextUpdate(m_hObject, 0);
                }
                return AI_Mgr::EngineMessageFn(message, data, value);
            }
        };
        class AIFireTarget : public BaseClass
        {
          public:
            CDestructable damage;
            AIFireTarget()
            {
                AddAggregate(&damage);
            }

          private:
            DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT value) override
            {
                if (message == MID_INITIALUPDATE)
                {
                    damage.Init(m_hObject);
                    damage.m_fHitPoints = damage.m_fMaxHitPoints = 10000;
                }
                return BaseClass::EngineMessageFn(message, data, value);
            }
        };
        // Explicit state-action driver for this smoke only. It does not implement
        // the missing AI_Update (deactivation, retention, navigation, death, etc.).
        class AIStateProbe : public CivilianAI
        {
            DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT value) override
            {
                if (message == MID_UPDATE)
                {
                    GetServerDE()->SetNextUpdate(m_hObject, .001f);
                    if (!DispatchState())
                        GetServerDE()->SetNextUpdate(m_hObject, 0);
                }
                return AI_Mgr::EngineMessageFn(message, data, value);
            }
        };
    } // namespace
    bool prepare_ai_follow_smoke(GameSession& g, ObjectId& id)
    {
        static const ClassInfo type{"AIFollowProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AIFollowProbe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return false;
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = {2000, 10000, 0};
        // No model-key audio driver; this probe isolates script movement and actual model clocks.
        SAFE_STRCPY(state.m_Name, "AI script probe (no full AI_Update)");
        SAFE_STRCPY(state.m_Filename, "models/enemies/m_cultist1.abc");
        auto h = g.objects.create(&type, state);
        auto* r = g.objects.get(h);
        auto* ai = r ? dynamic_cast<AIFollowProbe*>(r->game.get()) : nullptr;
        auto table = std::make_shared<CAnim_Sound>();
        if (!ai || !r->model || !table->SetAnimationIndexes(h) || !ai->InitStatics(table) ||
            !ai->SetAnimation(table->m_nAnim_RUN[TYPE_MELEE]) || !ai->m_MoveObj.Init(h))
            return false;
        ObjectCreateStruct target;
        target.m_Pos = vector_add(r->state.m_Pos, {0, 0, 20});
        SAFE_STRCPY(target.m_Name, "AI script movement destination");
        if (!g.objects.create(g.objects.find_class("BaseClass"), target))
            return false;
        ai->m_fRunSpeed = 60;
        ai->diagnostic_paths = &g.server_shell.paths;
        if (!ai->PrepareScript(std::string("SCRIPT; SETMOVEMENT RUN; MOVETOOBJECT \"") + target.m_Name +
                               "\"; WAIT 0; SETSTATE INACTIVE"))
            return false;
        id = r->id;
        g.server.SetNextUpdate(h, .001f);
        return true;
    }
    bool check_ai_follow_smoke(GameSession& g, ObjectId id)
    {
        const auto* r = g.objects.get(id);
        const auto* a = r ? dynamic_cast<const AIFollowProbe*>(r->game.get()) : nullptr;
        return a && r->model && a->m_nState == AI_Mgr::StateInactive && a->script_selections == 4 &&
               a->script_failures == 0 && a->script_outcome == AI_Mgr::ScriptOutcome::StateChanged &&
               a->m_bUpdateScriptCmd && a->script_motion_outcome == AI_Mgr::MotionOutcome::Completed &&
               a->script_motion_failures == 0 && a->path_arrivals == 1 && a->velocity_commands > 0 &&
               a->m_MoveObj.calculations == 1 && a->m_MoveObj.samples > 4 && r->state.m_Pos.z >= 20 &&
               g.server.GetModelAnimation(a->m_hObject) < r->model->animations.size();
    }
    bool prepare_ai_fire_smoke(GameSession& g, ObjectId& id)
    {
        static const ClassInfo type{"AIFireProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AIFireProbe>(); },
                                    {}};
        static const ClassInfo target_type{"AIFireTarget",
                                           g.objects.find_class("BaseClass"),
                                           []() -> std::unique_ptr<BaseClass>
                                           { return std::make_unique<AIFireTarget>(); },
                                           {}};
        if ((!g.objects.find_class(type.name) && !g.objects.register_class(type)) ||
            (!g.objects.find_class(target_type.name) && !g.objects.register_class(target_type)))
            return false;
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = {1000, 10000, 0};
        state.m_Flags = FLAG_MODELKEYS;
        SAFE_STRCPY(state.m_Name, "AI weapon probe (no full AI_Update)");
        SAFE_STRCPY(state.m_Filename, "models/enemies/m_cultist1.abc");
        auto h = g.objects.create(&type, state);
        auto* r = g.objects.get(h);
        auto* ai = r ? dynamic_cast<AIFireProbe*>(r->game.get()) : nullptr;
        if (!ai || !r->model || !ai->m_InventoryMgr.Init(h, nullptr) ||
            ai->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA, 0) != InventoryResult::Ok)
            return false;
        ai->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 100);
        auto table = std::make_shared<CAnim_Sound>();
        if (!table->SetAnimationIndexes(h) || !table->SetSoundRoot("sounds/enemies/m_cultist") ||
            !ai->InitStatics(table))
            return false;
        ObjectCreateStruct target;
        target.m_Pos = {1000, 10000, 300};
        target.m_Flags = FLAG_SOLID;
        SAFE_STRCPY(target.m_Name, "AI weapon damage receiver (diagnostic)");
        const auto victim = g.objects.create(&target_type, target);
        DVector dimensions{60, 100, 30};
        if (!victim || g.server.SetObjectDims(victim, &dimensions) != DE_OK || !ai->SetNewTarget(victim) ||
            !ai->RefreshTargetPosition())
            return false;
        // Original cache/path service, exercised outside UI. This isolated diagnostic
        // placement is not a claim of authored NPC navigation or path following.
        if (!ai->m_MoveObj.Init(h) || !ai->m_MoveObj.CalculatePath({1100, 10000, 0}))
            return false;
        ai->m_fHearingDist = 1000;
        if (!ai->HandleTrigger(victim, "SOUND_TRIGGER 2"))
            return false;
        ai->diagnostic_target = victim;
        g.server.SetNextUpdate(h, .001f);
        id = r->id;
        return true;
    }
    bool check_ai_fire_smoke(GameSession& g, ObjectId id)
    {
        const auto* r = g.objects.get(id);
        const auto* ai = r ? dynamic_cast<const AIFireProbe*>(r->game.get()) : nullptr;
        const auto* target =
            ai ? dynamic_cast<const AIFireTarget*>(g.server.HandleToObject(ai->diagnostic_target)) : nullptr;
        return ai && target && ai->m_MoveObj.sampled && ai->m_MoveObj.samples > 2 &&
               ai->m_MoveObj.outcome == CMovement::Outcome::Direct && ai->m_MoveObj.path().size() == 1 &&
               ai->m_MoveObj.failures == 0 && ai->hearing_reactions == 1 && ai->trigger_failures == 0 &&
               ai->trigger_outcome == AI_Mgr::TriggerOutcome::Heard && ai->fire_shots > 2 &&
               ai->animation_finishes > 0 && ai->fire_failures == 0 && ai->state_failures == 0 &&
               ai->key_failures == 0 && ai->audio_starts > 0 && target->damage.m_fHitPoints < 10000 &&
               ai->m_InventoryMgr.GetAmmoCount(AMMO_BULLET) < 100;
    }
    bool prepare_ai_state_smoke(GameSession& g, ObjectId& id)
    {
        static const ClassInfo type{"AIStateProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AIStateProbe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return false;
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        // Isolated diagnostic placement, not an authored NPC or a navigable actor.
        state.m_Pos = {0, 10000, 0};
        state.m_Flags = FLAG_MODELKEYS;
        SAFE_STRCPY(state.m_Name, "Civilian action probe (no full AI_Update)");
        SAFE_STRCPY(state.m_Filename, "models/enemies/f_civilian1.abc");
        auto h = g.objects.create(&type, state);
        auto* r = g.objects.get(h);
        auto* ai = r ? dynamic_cast<AIStateProbe*>(r->game.get()) : nullptr;
        if (!ai || !r->model)
            return false;
        auto table = std::make_shared<CAnim_Sound>();
        if (!table->SetAnimationIndexes(h) || !table->SetSoundRoot("sounds/enemies/f_civ") || !ai->InitStatics(table))
            return false;
        ai->m_szAIState = "SPECIAL4";
        if (!ai->ComputeState(0))
            return false;
        g.server.SetNextUpdate(h, .001f);
        id = r->id;
        return true;
    }
    bool check_ai_state_smoke(GameSession& g, ObjectId id)
    {
        const auto* r = g.objects.get(id);
        const auto* ai = r ? dynamic_cast<const AIStateProbe*>(r->game.get()) : nullptr;
        return ai && ai->state_dispatches > 1 && ai->animation_starts > 1 && ai->animation_finishes > 0 &&
               ai->state_changes > 1 && ai->state_failures == 0 && ai->m_nState == AI_Mgr::StateSpecial4;
    }
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
        state.m_Pos.x -= 50;
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
        return check_ai_target_smoke(g, id) && check_ai_sight_smoke(g, id);
    }
    bool check_ai_target_smoke(GameSession& g, ObjectId id)
    {
        const auto* r = g.objects.get(id);
        auto* ai = r ? dynamic_cast<AI_Mgr*>(r->game.get()) : nullptr;
        const auto* player = g.server_shell.Player(g.local_client);
        const auto* body = player ? g.objects.get(player->m_hObject) : nullptr;
        if (!ai || !body || !ai->InsertMe())
            return false;
        ai->m_fSeeingDist = 1500;
        ai->m_fSensingDist = 200;
        const auto stimulus = ai->ComputeStimuli();
        if ((stimulus != 1 && stimulus != 4) || ai->GetTarget() != body->handle || !ai->RefreshTargetPosition() ||
            !ai->MC_FaceTarget())
            return false;
        const auto delta = vector_subtract(body->state.m_Pos, r->state.m_Pos);
        const auto forward = rotate_vector({0, 0, 1}, r->state.m_Rotation);
        const auto distance = std::hypot(delta.x, delta.z);
        return distance > 0 && std::abs(forward.x - delta.x / distance) < .001f &&
               std::abs(forward.z - delta.z / distance) < .001f && ai->GetTarget() == body->handle &&
               g.objects.link_count(r->handle) == 1 && ai->target_position_valid && ai->target_failures == 0;
    }
    bool check_ai_sight_smoke(GameSession& g, ObjectId id)
    {
        const auto* r = g.objects.get(id);
        auto* ai = r ? dynamic_cast<AI_Mgr*>(r->game.get()) : nullptr;
        const auto* player = g.server_shell.Player(g.local_client);
        if (!ai || !player)
            return false;
        ai->CanSee(player->m_hObject);
        const auto& s = ai->sight;
        return s.origin_head && s.rays >= 1 && s.rays <= 2 && s.target == player->m_hObject &&
               s.outcome != AISightOutcome::Invalid && s.outcome != AISightOutcome::Unavailable;
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
