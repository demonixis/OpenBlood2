#pragma once
#include "client/effects.h"
#include "engine/model_view.h"
#include "engine/polygrid_view.h"
#include "engine/sprite_view.h"
namespace OpenBlood2
{
    bool append_effect_sprites(const ClientEffects&, SpriteView&, DRotation camera, bool before_muzzle,
                               const Resources&, std::string&);
    bool append_effect_models(const ClientEffects&, ModelView&, bool casings, const Resources&, std::string&);
    bool append_effect_polygrids(const ClientEffects&, PolyGridView&, const Resources&, std::string&);
    SceneEffects gather_scene_effects(const ClientEffects&, DVector eye, DRotation view);
} // namespace OpenBlood2
