#include "engine/world_model.h"
#include "game/original/Door.h"
#include "game/server_adapter.h"
#include <cmath>

namespace OpenBlood2
{
    SegmentResult ServerAdapter::IntersectSegment(DVector from, DVector to, HOBJECT ignore, SegmentHit& out,
                                                  bool fire_through)
    {
        trace_service("IntersectSegment");
        out = {};
        const auto* owner = ignore ? objects_.get(ignore) : nullptr;
        if (!physics_vector(from) || !physics_vector(to) || (ignore && (!owner || owner->life != Life::Alive)) ||
            vector_dot(vector_subtract(to, from), vector_subtract(to, from)) < 1e-12f)
            return SegmentResult::Invalid;
        if (!collision || !collision_ready)
            return SegmentResult::Unavailable;
        CollisionHit nearest;
        const auto world_result = collision->sweep(from, to, {}, nearest);
        if (world_result == CollisionQuery::Invalid)
            return SegmentResult::Invalid;
        bool found = world_result == CollisionQuery::Hit;
        for (auto id : objects_.snapshot())
        {
            const auto* record = objects_.get(id);
            if (!record || record->life != Life::Alive || record->handle == ignore ||
                !(record->state.m_Flags & FLAG_SOLID))
                continue; // Original IGNORE_NONSOLID.
            const auto* door = dynamic_cast<const Door*>(record->game.get());
            if (fire_through && door && door->fire_through)
                continue;
            CollisionHit hit;
            const auto query = sweep_object(*record, from, to, {}, hit);
            if (query == CollisionQuery::Invalid)
                return SegmentResult::Invalid;
            if (query == CollisionQuery::Hit && (!found || hit.fraction < nearest.fraction))
            {
                nearest = hit;
                nearest.object = record->handle;
                found = true;
            }
        }
        if (!found)
            return SegmentResult::Miss;
        out = {nearest.point, nearest.normal, nearest.object, nearest.polygon, nearest.fraction};
        return SegmentResult::Hit;
    }
    DRESULT ServerAdapter::WriteDamage(HMESSAGEWRITE h, const Damage& value)
    {
        trace_service("WriteDamage");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        auto* attacker = objects_.get(value.attacker);
        if (m->id != damage_message || !m->bytes.empty() || m->damage || m->key || !attacker ||
            attacker->life != Life::Alive || m->sender != value.attacker || !physics_vector(value.position) ||
            !physics_vector(value.direction) || !std::isfinite(value.amount) || value.amount < 0 ||
            value.amount > 1e6f ||
            (value.type != DamageType::Normal && value.type != DamageType::Melee &&
             value.type != DamageType::Suffocate && value.type != DamageType::Death &&
             value.type != DamageType::FreeFall && value.type != DamageType::Blind && value.type != DamageType::Slow &&
             value.type != DamageType::Acid))
        {
            m->bad = true;
            return DE_INVALIDPARAMS;
        }
        m->damage = value;
        return DE_OK;
    }
    bool ServerAdapter::ReadDamage(HMESSAGEREAD h, Damage& value)
    {
        trace_service("ReadDamage");
        value = {};
        auto it = messages_.find(h);
        if (it == messages_.end())
            return false;
        auto& m = it->second;
        auto* attacker = m.damage ? objects_.get(m.damage->attacker) : nullptr;
        if (!m.borrowed || m.bad || m.id != damage_message || !m.damage || !attacker || attacker->life != Life::Alive)
        {
            m.bad = true;
            return false;
        }
        value = *m.damage;
        return true; // Owned payload can be read by each aggregate.
    }
} // namespace OpenBlood2
