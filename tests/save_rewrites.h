#pragma once
#include "engine/save_objects.h"
#include "save_ai_fixture.h"
#include <algorithm>
#include <string_view>
#include <vector>

// Every class lot's listed re-save rewrites (save_load_contracts): the fields an original
// PRECREATE_SAVEGAME, INITIALUPDATE_SAVEGAME or Load deliberately rewrites, cleared on both sides
// of a comparison. Whole-world comparisons hold every lot's classes, so they share this list.
namespace save_rewrites
{
    using namespace OpenBlood2;
    inline bool light_class(std::string_view name)
    {
        return name == "ClientLightFX" || name == "GlowingLight" || name == "FlickerLight" ||
               name.ends_with("WaveLightFX");
    }
    inline void apply(std::vector<SaveEngineRecord>& records)
    {
        for (auto& r : records)
        {
            if (save_ai::is_ai(r.class_name))
            {
                // Lot 11: AI_Mgr::Load adds GetTime() to the raw m_fLastUpdate. Lot 12: Naga's whomp
                // count, written as a DWord and read as a Float.
                save_ai::normalize_payload(r.class_name, r.payload);
            }
            else if (r.class_name == "CCorpse") // Lot 11: PostPropRead restores its constant flags.
                r.flags = FLAG_VISIBLE | FLAG_RAYHIT | FLAG_GRAVITY | FLAG_MODELKEYS | FLAG_REMOVEIFOUTSIDE;
            else if (r.class_name == "CameraObj")
            {
                // Lot 7: PostPropRead forces the flags, Load's SetActive the schedule and a timed
                // camera's full duration.
                r.update_remaining = 0;
                r.flags &= FLAG_FORCECLIENTUPDATE | FLAG_FULLPOSITIONRES | FLAG_GOTHRUWORLD;
                if (r.payload.size() >= 13)
                    std::fill_n(r.payload.begin() + 9, 4, 0); // m_fDeactivateTime - GetTime().
            }
            else if (r.class_name == "CPlayerObj")
            {
                // Lot 5: Load's flags, velocity, node reset, zoom and the blob's load type (the
                // player's bytes are checked field by field in save_player). The client attach
                // also resets a moving player's acceleration and the first update restarts its
                // clip (observed in lot 11).
                r.flags &= ~FLAG_SHADOW;
                r.velocity = {};
                r.hidden_nodes.clear();
                r.payload.clear();
                r.acceleration = {};
                r.animation = 0;
                r.animation_time = 0;
                r.animation_looping = r.animation_done = r.animation_first = false;
            }
            else if (r.class_name == "WorldModelDebris") // Lot 8: CDestructableBrush's PostPropRead.
                r.flags &= ~(FLAG_FULLPOSITIONRES | FLAG_MODELGOURAUDSHADE);
            else if (r.class_name.ends_with("PU") || r.class_name == "KeyPickup")
                r.flags |= FLAG_TOUCH_NOTIFY | FLAG_VISIBLE | FLAG_GRAVITY; // Lot 9: PickupObject's PostPropRead.
            // Lot 10: the ClientLightFX family's PostPropRead flags and schedule, and the explosion,
            // lightning and warp gate initial updates' reschedule (each keeps polling at its phase).
            if (light_class(r.class_name))
                r.flags = 0;
            if (light_class(r.class_name) || r.class_name == "CClientExplosionSFX" ||
                r.class_name == "CClientLightningSFX" || r.class_name == "CClientWarpGateSFX")
            {
                r.scheduled = false;
                r.update_remaining = 0;
            }
        }
    }
} // namespace save_rewrites
