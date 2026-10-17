#include "game/original/Grenade.h"
#include "game/original/Projectile.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool ServerAdapter::AdvanceGrenade(HOBJECT h, double seconds)
    {
        trace_service("AdvanceGrenade");
        auto* record = objects_.get(h);
        auto* grenade = record && record->life == Life::Alive ? dynamic_cast<CGrenade*>(record->game.get()) : nullptr;
        if (!grenade || !grenade->configured || moving_ || objects_.busy() || !collision || !collision_ready ||
            !std::isfinite(seconds) || seconds <= 0 || seconds > .5 || record->attachment ||
            record->state.m_ObjectType != OT_MODEL || !record->model || !physics_vector(record->velocity) ||
            !physics_vector(record->acceleration) || !physics_vector(record->state.m_Pos) ||
            !physics_vector(record->dimensions))
            return false;
        moving_ = true;
        struct Reset
        {
            bool& flag;
            ~Reset()
            {
                flag = false;
            }
        } reset{moving_};
        const auto ids = objects_.snapshot();
        const auto steps = static_cast<unsigned>(std::max(1.0, std::ceil(seconds * 120)));
        const float dt = static_cast<float>(seconds / steps);
        for (unsigned step = 0; step < steps && record->life == Life::Alive && !grenade->m_bExplode; ++step)
        {
            ContainerPhysics physics{
                h,    record->velocity, record->acceleration, dt, (record->state.m_Flags & FLAG_GRAVITY) != 0, false,
                false};
            if (!affect_containers(h, physics))
                return false;
            if (record->life != Life::Alive)
                break;
            auto acceleration = physics.acceleration;
            if (physics.gravity)
                acceleration = vector_add(acceleration, gravity);
            // Fresh support, excluding the shooter just like the travelled sweep.
            CollisionHit support;
            auto down = record->state.m_Pos;
            down.y -= .02f;
            auto supported =
                sweep_scene(h, record->state.m_Pos, down, record->dimensions, support, ids, grenade->m_hFiredFrom);
            if (supported == CollisionQuery::Invalid)
                return false;
            bool resting = physics.gravity && supported == CollisionQuery::Hit && !support.start_solid &&
                           support.normal.y >= .5f && vector_dot(physics.velocity, support.normal) <= .01f;
            DVector displacement, velocity = physics.velocity;
            if (resting)
            {
                const auto into = vector_dot(acceleration, support.normal);
                if (into < 0)
                    acceleration = vector_subtract(acceleration, vector_scale(support.normal, into));
                // Source friction coefficient 18; use the existing body's exponential
                // ground-friction policy. This is replacement physics, not retail parity.
                const float decay = std::exp(-18.f * dt);
                const auto terminal = vector_scale(acceleration, 1.f / 18);
                displacement = vector_add(vector_scale(terminal, dt),
                                          vector_scale(vector_subtract(velocity, terminal), (1 - decay) / 18));
                velocity = vector_add(terminal, vector_scale(vector_subtract(velocity, terminal), decay));
                if (vector_dot(velocity, velocity) < .1f && vector_dot(acceleration, acceleration) < .1f)
                    velocity = {};
            }
            else
            {
                displacement = vector_add(vector_scale(velocity, dt), vector_scale(acceleration, .5f * dt * dt));
                velocity = vector_add(velocity, vector_scale(acceleration, dt));
            }
            const auto destination = vector_add(record->state.m_Pos, displacement);
            CollisionHit hit;
            const auto query =
                sweep_scene(h, record->state.m_Pos, destination, record->dimensions, hit, ids, grenade->m_hFiredFrom);
            if (query == CollisionQuery::Invalid || (query == CollisionQuery::Hit && hit.start_solid) ||
                !physics_vector(destination) || !physics_vector(velocity))
                return false;
            record->grounded = resting;
            record->contact_normal = resting ? support.normal : DVector{};
            record->contact_object = resting ? support.object : nullptr;
            record->contact_polygon = resting ? support.polygon : UINT32_MAX;
            if (query == CollisionQuery::Hit)
            {
                // Commit one box segment per bounded substep, then run the original
                // callback on post-projection velocity. No fabricated bounce impulse.
                record->state.m_Pos = vector_add(hit.point, vector_scale(hit.normal, .001f));
                const auto into = vector_dot(velocity, hit.normal);
                if (into < 0)
                    velocity = vector_subtract(velocity, vector_scale(hit.normal, into));
                record->contact_normal = hit.normal;
                record->contact_object = hit.object;
                record->contact_polygon = hit.polygon;
                record->grounded = hit.normal.y >= .5f;
                record->velocity = velocity;
                SegmentHit contact{hit.point, hit.normal, hit.object, hit.polygon, hit.fraction, hit.texture_flags};
                grenade->contact = contact;
                grenade->contacted = true;
                ++grenade->contacts;
                grenade->HandleContact(contact);
                auto* target = objects_.get(hit.object);
                if (record->life == Life::Alive && target && target->life == Life::Alive &&
                    (target->state.m_Flags & FLAG_TOUCH_NOTIFY))
                    objects_.engine_message(target->handle, MID_TOUCHNOTIFY, h);
                if (grenade->failed)
                    return false;
            }
            else
            {
                record->state.m_Pos = destination;
                record->velocity = velocity;
            }
            ++grenade->flight_steps;
        }
        return true;
    }
    void ServerAdapter::RecordProjectile(const ProjectileReport& report)
    {
        if (projectile_reports.size() == 64)
            projectile_reports.erase(projectile_reports.begin());
        projectile_reports.push_back(report);
    }
    bool ServerAdapter::advance_projectiles(double seconds)
    {
        if (objects_.busy() || !std::isfinite(seconds) || seconds < 0 || seconds > 60)
            return false;
        bool ok = true;
        for (const auto id : objects_.snapshot())
        {
            auto* record = objects_.get(id);
            auto* projectile = record && record->life == Life::Alive && record->activity == OBJSTATE_ACTIVE
                                   ? dynamic_cast<CProjectile*>(record->game.get())
                                   : nullptr;
            if (projectile)
            {
                const bool was_contact = projectile->contacted;
                ok = projectile->AdvanceFlight(seconds) && ok;
                const auto* target = objects_.get(projectile->m_hHitObject);
                if (!dynamic_cast<CGrenade*>(projectile) && !was_contact && projectile->contacted &&
                    record->life == Life::Alive && target && target->life == Life::Alive &&
                    (target->state.m_Flags & FLAG_TOUCH_NOTIFY))
                    objects_.engine_message(target->handle, MID_TOUCHNOTIFY, record->handle);
            }
        }
        return ok;
    }
} // namespace OpenBlood2
