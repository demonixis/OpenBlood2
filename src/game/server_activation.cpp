#include "engine/collision.h"
#include "game/server_adapter.h"
#include <chrono>
#include <cmath>
namespace OpenBlood2
{
    bool ServerAdapter::ConfigureCharacterBody(HOBJECT h, float mass, float friction)
    {
        trace_service("ConfigureCharacterBody");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || r->world_body || r->model_body ||
            r->state.m_ObjectType != OT_MODEL || !std::isfinite(mass) || mass <= 0 || mass > 1e6f ||
            !std::isfinite(friction) || friction < 0 || friction > 20005)
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
            (r->state.m_ObjectType != OT_MODEL && r->state.m_ObjectType != OT_SPRITE &&
             r->state.m_ObjectType != OT_NORMAL) ||
            !physics_vector(scale) || scale.x < 0 || scale.y < 0 || scale.z < 0 ||
            (r->state.m_ObjectType == OT_MODEL && (scale.x == 0 || scale.y == 0 || scale.z == 0)))
            return false;
        r->state.m_Scale = scale;
        return true;
    }
    bool ServerAdapter::SetDeactivationTime(HOBJECT h, float seconds)
    {
        if (!trace_service("SetDeactivationTime"))
            return false;
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
        if (!trace_service("PingObjects"))
            return false;
        struct Cost
        {
            ServerAdapter& server;
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            ~Cost()
            {
                server.activation_ping_ns += std::uint64_t(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin)
                        .count());
                ++server.activation_ping_calls;
            }
        } cost{*this};
        auto* source = objects_.get(observer);
        if (!source || source->life != Life::Alive || !collision || !collision_ready)
            return false;
        // Retail PingObjects walks the observer leaf's BSP visibility set: no range or field-of-view
        // limit, and objects never occlude. Replacement: up to four primary-world segments between
        // the observer's and the candidate's centres and box tops, the first clear one wins. World
        // models (doors, props, train brushes) and characters never block activation.
        const auto from = source->state.m_Pos;
        const DVector from_top{from.x, from.y + source->dimensions.y, from.z};
        std::vector<ObjectId> visible;
        for (auto id : objects_.snapshot())
        {
            auto* r = objects_.get(id);
            if (!r || r->life != Life::Alive || !r->auto_deactivate || r->handle == observer ||
                (r->activity != OBJSTATE_ACTIVE && r->activity != OBJSTATE_AUTODEACTIVATE_NOW))
                continue;
            const auto to = r->state.m_Pos;
            const DVector to_top{to.x, to.y + r->dimensions.y, to.z};
            auto& probe = r->activation_probe;
            ++probe.checks;
            probe.time = objects_.time();
            probe.observer = source->id;
            probe.from = from;
            probe.to = to;
            probe.hit_object = {};
            probe.refreshed = false;
            const auto d = vector_subtract(to, from);
            auto result = vector_dot(d, d) < 1e-12f ? CollisionQuery::Invalid : CollisionQuery::Hit;
            CollisionHit first;
            const std::pair<DVector, DVector> rays[]{{from, to}, {from_top, to}, {from, to_top}, {from_top, to_top}};
            for (unsigned i = 0; i < 4 && result == CollisionQuery::Hit; ++i)
            {
                CollisionHit hit;
                result = collision->sweep(rays[i].first, rays[i].second, {}, hit);
                if (!i)
                    first = hit;
            }
            probe.polygon = first.polygon;
            probe.fraction = first.fraction;
            probe.visibility = result == CollisionQuery::Invalid ? ActivationVisibility::Invalid
                               : result == CollisionQuery::Miss  ? ActivationVisibility::Visible
                                                                 : ActivationVisibility::Blocked;
            if (result == CollisionQuery::Invalid)
                return false; // No partial refresh on failed queries.
            if (probe.visibility == ActivationVisibility::Visible)
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
            r->activation_probe.refreshed = true;
        }
        return true;
    }
} // namespace OpenBlood2
