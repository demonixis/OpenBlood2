#include "game/server_adapter.h"
#include <cmath>
namespace OpenBlood2
{
    bool ServerAdapter::ConfigureCharacterBody(HOBJECT h, float mass, float friction)
    {
        trace_service("ConfigureCharacterBody");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || r->world_body || r->state.m_ObjectType != OT_MODEL ||
            !std::isfinite(mass) || mass <= 0 || mass > 1e6f || !std::isfinite(friction) || friction < 0 ||
            friction > 20005)
            return false;
        r->character_body = true;
        r->body_properties = {mass, friction, false};
        // Original SetMass sets MIN_FORCE=0: this integrator drops no nonzero force.
        return true;
    }
    bool ServerAdapter::ScaleObject(HOBJECT h, DVector scale)
    {
        trace_service("ScaleObject");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment ||
            (r->state.m_ObjectType != OT_MODEL && r->state.m_ObjectType != OT_SPRITE) || !physics_vector(scale) ||
            scale.x < 0 || scale.y < 0 || scale.z < 0 ||
            (r->state.m_ObjectType == OT_MODEL && (scale.x == 0 || scale.y == 0 || scale.z == 0)))
            return false;
        r->state.m_Scale = scale;
        return true;
    }
    bool ServerAdapter::SetDeactivationTime(HOBJECT h, float seconds)
    {
        trace_service("SetDeactivationTime");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || !std::isfinite(seconds) || seconds < 0 || seconds > 1e6f)
            return false;
        r->deactivation_period = seconds;
        r->deactivate_at = objects_.time() + seconds;
        r->auto_deactivate = seconds > 0;
        if (r->activity == OBJSTATE_AUTODEACTIVATE_NOW)
            r->activity = OBJSTATE_ACTIVE;
        return true;
    }
    bool ServerAdapter::PingObjects(HOBJECT observer)
    {
        trace_service("PingObjects");
        auto* source = objects_.get(observer);
        if (!source || source->life != Life::Alive || !collision || !collision_ready)
            return false;
        const auto from = source->state.m_Pos;
        // Replacement visibility selection: checked center rays, independent of render/UI.
        // Retail PVS-based activation is not reproduced by the DAT reader yet.
        std::vector<ObjectId> visible;
        for (auto id : objects_.snapshot())
        {
            auto* r = objects_.get(id);
            if (!r || r->life != Life::Alive || !r->auto_deactivate || r->handle == observer ||
                (r->activity != OBJSTATE_ACTIVE && r->activity != OBJSTATE_AUTODEACTIVATE_NOW))
                continue;
            SegmentHit hit;
            auto result = IntersectSegment(from, r->state.m_Pos, observer, hit);
            if (result == SegmentResult::Invalid || result == SegmentResult::Unavailable)
                return false; // No partial refresh on failed queries.
            if (result == SegmentResult::Miss || (result == SegmentResult::Hit && hit.object == r->handle))
                visible.push_back(id);
        }
        for (auto id : visible)
        {
            auto* r = objects_.get(id); // No callbacks above; retain checked identities anyway.
            if (!r || r->life != Life::Alive)
                continue;
            r->activity = OBJSTATE_ACTIVE;
            r->deactivate_at = objects_.time() + r->deactivation_period;
            ++r->activation_pings;
        }
        return true;
    }
} // namespace OpenBlood2
