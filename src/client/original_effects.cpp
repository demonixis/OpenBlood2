#include "client/original_effects.h"
#include <algorithm>
namespace OpenBlood2
{
    bool OriginalEffects::advance_lights(float time, const ClientWorld& replica)
    {
        std::erase_if(light_effects, [&](const auto& light) { return !replica.get(light.source); });
        for (auto& light : light_effects)
        {
            const auto& owner = *replica.get(light.source);
            if (!light.Update(time, owner.pose.position, (owner.user_flags & 1u) != 0))
                return false;
        }
        return true;
    }
    void OriginalEffects::reset_world()
    {
        explosion_sprites.clear();
        sparks.clear();
        lightning.clear();
        warp_gates.clear();
        blood_spurts.clear();
        blood_splats.clear();
        weapon_sprites.clear();
        shotgun_effects.clear();
        explosion_models.clear();
        laser_beams.clear();
        object_particles.clear();
        particle_streams.clear();
        polygrids.clear();
        explosion_lights.clear();
        explosion_rings.clear();
        explosion_flames.clear();
        explosion_fragments.clear();
        flesh_gibs.clear();
        destruction_debris.clear();
        light_effects.clear();
        detached_lines.clear();
        next_line_id = 1;
    }
} // namespace OpenBlood2
