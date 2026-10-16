#include "character_motion_smoke.h"
namespace OpenBlood2
{
    bool prepare_character_motion_smoke(GameSession& g, ObjectId& id)
    {
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = {100000, 100000, 100000};
        state.m_Flags = FLAG_MODELKEYS; // No collision/gravity: isolated inspection probe, never an AI actor.
        SAFE_STRCPY(state.m_Name, "Synthetic motion probe (not AI)");
        SAFE_STRCPY(state.m_Filename, "models/characters/caleb.abc");
        const auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
        auto* r = g.objects.get(h);
        auto* c = r ? dynamic_cast<CBaseCharacter*>(r->game.get()) : nullptr;
        if (!c || !r->model || r->model->animations.empty() || r->model->animations[0].keys.empty())
            return false;
        // Only the in-memory probe receives synthetic markers; archive/cache remains immutable.
        auto model = std::make_shared<Model>(*r->model);
        for (auto& a : model->animations)
            for (auto& key : a.keys)
                key.text.clear();
        model->animations[0].keys[0].text =
            "[RotY:0.25][ForwVel:12][AddX:2][AddY:0][AddZ:0][DimsX:2][DimsY:3][DimsZ:4]";
        r->model = model;
        id = r->id;
        return c->m_Anim_Sound.SetAnimationIndexes(h) && c->m_Anim_Sound.GenerateHitSpheres(h);
    }
    bool check_character_motion_smoke(GameSession& g, ObjectId id)
    {
        const auto* r = g.objects.get(id);
        const auto* c = r ? dynamic_cast<const CBaseCharacter*>(r->game.get()) : nullptr;
        return c && c->motion_keys > 0 && c->motion_actions == c->motion_keys * 4 && c->motion_applied == 51 &&
               c->motion_failures == 0 && c->motion_error.empty() && r->dimensions.y == 3 &&
               r->state.m_Pos.x > 100000 && r->state.m_Pos.y == 100000;
    }
} // namespace OpenBlood2
