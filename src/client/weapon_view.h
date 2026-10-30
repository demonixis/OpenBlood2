#pragma once
#include "engine/model_store.h"
#include "engine/sprite.h"
namespace OpenBlood2
{
    // Owned client visual state. Placement reuses the original CViewWeapon math;
    // no inventory, game object or server callback is needed by presentation.
    struct ClientWeaponView
    {
        bool AdjustAngleAndSetPos(float pitch, float yaw, DVector eye, NodePose&) const;
        DVector FlashOffset(bool original_projection) const;
        DBYTE type = 0;
        bool left = false;
        std::string model_path, skin_path;
        std::shared_ptr<const Model> model;
        AnimationTrack animation;
        std::array<std::shared_ptr<const Sprite>, 2> flashes;
        std::array<SpriteTrack, 2> flash_tracks;
        unsigned flash_visible = 0;
    };
} // namespace OpenBlood2
