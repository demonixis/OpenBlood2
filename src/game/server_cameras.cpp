#include "game/original/CameraObj.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    DDWORD ServerAdapter::GetObjectUserFlags(HOBJECT h)
    {
        trace_service("GetObjectUserFlags");
        const auto* r = objects_.get(h);
        return r ? r->user_flags : 0;
    }
    bool ServerAdapter::PublishCameraEffect(HOBJECT h, const CameraSettings& settings)
    {
        trace_service("PublishCameraEffect");
        if (!IsObjectAlive(h) || !dynamic_cast<CameraObj*>(objects_.get(h)->game.get()) || settings.type > 2 ||
            camera_effects.size() >= 4096 ||
            std::any_of(camera_effects.begin(), camera_effects.end(), [h](const auto& c) { return c.object == h; }))
            return objects_.fail(ObjectError::Unsupported,
                                 "Invalid/duplicate camera descriptor or camera limit exceeded");
        camera_effects.push_back({h, settings, {}, {}});
        return true;
    }
    void ServerAdapter::RemoveCameraEffect(HOBJECT h)
    {
        trace_service("RemoveCameraEffect");
        std::erase_if(camera_effects, [h](const auto& c) { return c.object == h; });
    }
    CameraView ServerAdapter::GetCameraView()
    {
        trace_service("GetCameraView");
        // Original client chooses the first visible camera FX. Stable creation
        // order replaces the legacy SFX slot array; no visibility culling.
        for (const auto& effect : camera_effects)
            if (const auto* r = objects_.get(effect.object); r && r->life == Life::Alive && (r->user_flags & 1))
                return {effect.object, effect.settings, r->state.m_Pos, r->state.m_Rotation};
        return {};
    }
    DRESULT ServerAdapter::AlignRotation(DRotation& rotation, DVector forward)
    {
        trace_service("AlignRotation (world Y up)");
        // Shared original +Y-up convention, with deterministic vertical fallback.
        return align_rotation(forward, rotation) ? DE_OK : DE_INVALIDPARAMS;
    }
} // namespace OpenBlood2
