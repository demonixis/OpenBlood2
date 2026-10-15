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
        if (!physics_vector(forward) || vector_dot(forward, forward) < 1e-12f)
            return DE_INVALIDPARAMS;
        // Source CameraObj always supplies world Y up. Vertical forward has a
        // deterministic zero yaw. Coincident targets retain their old rotation.
        const float horizontal = std::hypot(forward.x, forward.z);
        rotation = rotation_from_euler(
            {std::atan2(-forward.y, horizontal), horizontal > 1e-6f ? std::atan2(forward.x, forward.z) : 0, 0});
        return DE_OK;
    }
} // namespace OpenBlood2
