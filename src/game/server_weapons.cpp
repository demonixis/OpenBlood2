#include "engine/world_model.h"
#include "game/original/DestructableBrush.h"
#include "game/original/Door.h"
#include "game/original/PlayerObj.h"
#include "game/original/SurfaceTypes.h"
#include "game/original/VolumeBrush.h"
#include "game/server_adapter.h"
#include <cmath>

namespace OpenBlood2
{
    SegmentResult ServerAdapter::IntersectSegment(DVector from, DVector to, HOBJECT ignore, SegmentHit& out,
                                                  bool fire_through, SegmentObjects objects, SegmentFilter filter)
    {
        trace_service("IntersectSegment");
        out = {};
        const auto* owner = ignore ? objects_.get(ignore) : nullptr;
        if ((filter != SegmentFilter::None && filter != SegmentFilter::AIVisibility &&
             filter != SegmentFilter::IgnoreCharacters) ||
            (objects != SegmentObjects::SolidOnly && objects != SegmentObjects::SolidOrRayHit &&
             objects != SegmentObjects::All) ||
            !physics_vector(from) || !physics_vector(to) || (ignore && (!owner || owner->life != Life::Alive)) ||
            vector_dot(vector_subtract(to, from), vector_subtract(to, from)) < 1e-12f)
            return SegmentResult::Invalid;
        if (!collision || !collision_ready)
            return SegmentResult::Unavailable;
        if (!objects_.refresh_attachments())
            return SegmentResult::Invalid;
        CollisionHit nearest;
        const auto world_result = collision->sweep(from, to, {}, nearest);
        if (world_result == CollisionQuery::Invalid)
            return SegmentResult::Invalid;
        bool found = world_result == CollisionQuery::Hit;
        for (auto id : objects_.snapshot())
        {
            const auto* record = objects_.get(id);
            if (!record || record->life != Life::Alive || record->handle == ignore ||
                (objects != SegmentObjects::All &&
                 !(record->state.m_Flags &
                   (objects == SegmentObjects::SolidOrRayHit ? FLAG_SOLID | FLAG_RAYHIT : FLAG_SOLID))))
                continue;
            const auto* door = dynamic_cast<const Door*>(record->game.get());
            const auto* brush = dynamic_cast<const CDestructableBrush*>(record->game.get());
            // Original GlobalFilterFn, restricted to CanSee's two requested flags.
            // IGNORE_LIQUID excludes every VolumeBrush subclass, not just liquids.
            // Glass only passes when the object is a destructible brush. Baked
            // world polygons, doors, CanDamage and FireThrough do not define this filter.
            if (filter == SegmentFilter::AIVisibility &&
                (dynamic_cast<const VolumeBrush*>(record->game.get()) ||
                 (brush && brush->destructable && brush->GetSurfaceType() == SURFTYPE_GLASS)))
                continue;
            if (filter == SegmentFilter::IgnoreCharacters && dynamic_cast<const CBaseCharacter*>(record->game.get()))
                continue;
            if (fire_through && ((door && door->fire_through) || (brush && brush->fire_through)))
                continue;
            CollisionHit hit;
            const auto query = sweep_object(*record, from, to, {}, hit);
            if (query == CollisionQuery::Invalid)
                return SegmentResult::Invalid;
            // USE follows entry-face rays: an enclosing body/sensor must not
            // mask all interactions. Later LithTech exposes interior hits as an
            // opt-in flag; ordinary USE does not request it.
            if (objects == SegmentObjects::All && hit.start_solid)
                continue;
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
    void ServerAdapter::RecordBrushDestruction(HOBJECT handle, DVector direction, float damage, DBYTE amount)
    {
        trace_service("RecordBrushDestruction");
        auto* object = objects_.get(handle);
        if (!object || object->life != Life::Alive || !physics_vector(direction) || !std::isfinite(damage))
            return;
        if (brush_destructions.size() == 32)
            brush_destructions.erase(brush_destructions.begin());
        brush_destructions.push_back({object->state.m_Name, object->state.m_Pos, direction, damage, amount});
    }
    DRESULT ServerAdapter::WriteDamage(HMESSAGEWRITE h, const Damage& value)
    {
        trace_service("WriteDamage");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        auto* attacker = objects_.get(value.attacker);
        if (m->id != damage_message || !m->bytes.empty() || m->damage || m->key || m->pickup || !attacker ||
            attacker->life != Life::Alive || m->sender != value.attacker || !physics_vector(value.position) ||
            !physics_vector(value.direction) || !std::isfinite(value.amount) || value.amount < 0 ||
            value.amount > 1e6f ||
            (value.type != DamageType::Fire && value.type != DamageType::Normal && value.type != DamageType::Melee &&
             value.type != DamageType::Explode && value.type != DamageType::Suffocate &&
             value.type != DamageType::Death && value.type != DamageType::FreeFall && value.type != DamageType::Blind &&
             value.type != DamageType::Slow && value.type != DamageType::Acid))
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
