#include "engine/world_model.h"
#include "game/original/ClientLightningSFX.h"
#include "game/original/DestructableBrush.h"
#include "game/original/DestructableModel.h"
#include "game/original/Door.h"
#include "game/original/PlayerObj.h"
#include "game/original/Projectile.h"
#include "game/original/SurfaceTypes.h"
#include "game/original/VolumeBrush.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    DRESULT ServerAdapter::WriteImmobilize(HMESSAGEWRITE h, float seconds)
    {
        trace_service("WriteImmobilize");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        if (!m->routed || m->id != immobilize_message || !IsObjectAlive(m->sender) || !IsObjectAlive(m->receiver) ||
            !m->bytes.empty() || m->damage || m->key || m->pickup || m->immobilize || !std::isfinite(seconds) ||
            seconds < 0 || !std::isfinite(GetTime() + seconds))
        {
            m->bad = true;
            return DE_INVALIDPARAMS;
        }
        m->immobilize = seconds;
        return DE_OK;
    }
    bool ServerAdapter::ReadImmobilize(HMESSAGEREAD h, float& seconds)
    {
        trace_service("ReadImmobilize");
        seconds = 0;
        auto it = messages_.find(h);
        if (it == messages_.end())
            return false;
        auto& m = it->second;
        if (!m.borrowed || m.bad || m.id != immobilize_message || !m.immobilize)
        {
            m.bad = true;
            return false;
        }
        seconds = *m.immobilize;
        return true;
    }
    namespace
    {
        // IntersectSegment's ray-independent object filter.
        bool segment_excludes(const Objects& objects, const ObjectRecord& record, HOBJECT ignore,
                              const ObjectRecord* owner, const CProjectile* projectile, bool fire_through,
                              SegmentObjects which, SegmentFilter filter)
        {
            if (record.life != Life::Alive || record.handle == ignore ||
                (which != SegmentObjects::All &&
                 !(record.state.m_Flags &
                   (which == SegmentObjects::SolidOrRayHit ? FLAG_SOLID | FLAG_RAYHIT : FLAG_SOLID))))
                return true;
            const auto* door = dynamic_cast<const Door*>(record.game.get());
            const auto* brush = dynamic_cast<const CDestructableBrush*>(record.game.get());
            // Original GlobalFilterFn, restricted to CanSee's two requested flags.
            // IGNORE_LIQUID excludes every VolumeBrush subclass, not just liquids.
            // Glass only passes when the object is a destructible brush. Baked
            // world polygons, doors, CanDamage and FireThrough do not define this filter.
            if ((filter == SegmentFilter::AIVisibility || filter == SegmentFilter::AIShot) &&
                (dynamic_cast<const VolumeBrush*>(record.game.get()) ||
                 (brush && brush->destructable && brush->GetSurfaceType() == SURFTYPE_GLASS)))
                return true;
            const auto* model = dynamic_cast<const CDestructableModel*>(record.game.get());
            if (filter == SegmentFilter::AIShot && model && model->destructable)
                return true;
            if (filter == SegmentFilter::IgnoreVolumes && dynamic_cast<const VolumeBrush*>(record.game.get()))
                return true;
            if (filter == SegmentFilter::IgnoreCharacters && dynamic_cast<const CBaseCharacter*>(record.game.get()))
                return true;
            if (filter == SegmentFilter::IgnoreAncientTentacles &&
                Objects::is_kind_of(record.type, objects.find_class("AncientOneTentacle")))
                return true;
            if (filter == SegmentFilter::IgnoreOwnerClass && Objects::is_kind_of(record.type, owner->type))
                return true;
            if (filter == SegmentFilter::ProjectileContact &&
                (record.type == owner->type || record.handle == projectile->m_hFiredFrom))
                return true;
            return fire_through && ((door && door->fire_through) || (brush && brush->fire_through));
        }
        // Replacement broad phase: a segment outside these bounds widened by a margin far
        // above every sweep tolerance cannot touch the geometry, so skipping it is a miss.
        bool segment_reaches(DVector from, DVector to, DVector min, DVector max)
        {
            double enter = 0, leave = 1;
            const auto axis = [&](double a, double b, double low, double high)
            {
                const double margin = 1 + 1e-4 * std::max(std::abs(low), std::abs(high));
                low -= margin;
                high += margin;
                const double delta = b - a;
                if (delta == 0)
                    return a >= low && a <= high;
                auto first = (low - a) / delta, last = (high - a) / delta;
                if (first > last)
                    std::swap(first, last);
                enter = std::max(enter, first);
                leave = std::min(leave, last);
                return enter <= leave;
            };
            return axis(from.x, to.x, min.x, max.x) && axis(from.y, to.y, min.y, max.y) &&
                   axis(from.z, to.z, min.z, max.z);
        }
    } // namespace
    void ServerAdapter::prepare_segment_batch(HOBJECT ignore, bool fire_through, SegmentObjects objects,
                                              SegmentFilter filter, SegmentBatch& batch)
    {
        batch = {ignore, fire_through, objects, filter};
        auto& blockers = batch.blockers;
        const auto* owner = ignore ? objects_.get(ignore) : nullptr;
        const auto* projectile = owner ? dynamic_cast<const CProjectile*>(owner->game.get()) : nullptr;
        // Arguments IntersectSegment refuses as Invalid never reach the filter below.
        if ((filter == SegmentFilter::IgnoreOwnerClass && !owner) ||
            (filter == SegmentFilter::ProjectileContact && !projectile))
            return;
        for (auto id : objects_.snapshot())
        {
            const auto* record = objects_.get(id);
            if (!record ||
                segment_excludes(objects_, *record, ignore, owner, projectile, fire_through, objects, filter))
                continue;
            SegmentBlocker blocker{id};
            const auto* shape = record->world_model ? record->world_model.get() : record->container().get();
            if (record->attachment)
                blocker.bounded = false; // Attachment poses refresh per query: always swept.
            else if (!shape || (record->state.m_Flags & FLAG_BOXPHYSICS))
            {
                // sweep_box: only finite, nonnegative bounds can produce a Miss rather than Invalid.
                const auto center = object_bounds_center(*record);
                const auto half = record->dimensions;
                blocker.bounded =
                    physics_vector(center) && physics_vector(half) && half.x >= 0 && half.y >= 0 && half.z >= 0;
                blocker.min = vector_subtract(center, half);
                blocker.max = vector_add(center, half);
            }
            else
            {
                blocker.shape = true;
                blocker.offset = vector_subtract(record->state.m_Pos, shape->origin);
                blocker.min = vector_add(shape->min, blocker.offset);
                blocker.max = vector_add(shape->max, blocker.offset);
                blocker.bounded =
                    physics_vector(blocker.offset) && physics_vector(blocker.min) && physics_vector(blocker.max);
            }
            blockers.push_back(blocker);
        }
    }
    SegmentResult ServerAdapter::IntersectSegment(DVector from, DVector to, HOBJECT ignore, SegmentHit& out,
                                                  bool fire_through, SegmentObjects objects, SegmentFilter filter,
                                                  SegmentStart start)
    {
        trace_service("IntersectSegment");
        auto* batch = segment_batch_;
        if (batch && (batch->ignore != ignore || batch->fire_through != fire_through || batch->objects != objects ||
                      batch->filter != filter))
            batch = nullptr;
        out = {};
        const auto* owner = ignore ? objects_.get(ignore) : nullptr;
        if ((filter != SegmentFilter::None && filter != SegmentFilter::AIVisibility &&
             filter != SegmentFilter::AIShot && filter != SegmentFilter::IgnoreCharacters &&
             filter != SegmentFilter::IgnoreVolumes && filter != SegmentFilter::IgnoreOwnerClass &&
             filter != SegmentFilter::IgnoreAncientTentacles && filter != SegmentFilter::ProjectileContact) ||
            (start != SegmentStart::IncludeInterior && start != SegmentStart::SkipInterior) ||
            (objects != SegmentObjects::SolidOnly && objects != SegmentObjects::SolidOrRayHit &&
             objects != SegmentObjects::All) ||
            (filter == SegmentFilter::IgnoreOwnerClass && !owner) || !physics_vector(from) || !physics_vector(to) ||
            (ignore && (!owner || owner->life != Life::Alive)) ||
            vector_dot(vector_subtract(to, from), vector_subtract(to, from)) < 1e-12f)
            return SegmentResult::Invalid;
        const auto* projectile = owner ? dynamic_cast<const CProjectile*>(owner->game.get()) : nullptr;
        if (filter == SegmentFilter::ProjectileContact && !projectile)
            return SegmentResult::Invalid;
        if (!collision || !collision_ready)
            return SegmentResult::Unavailable;
        if (!batch || !batch->attachments_settled)
        {
            bool changed = false;
            if (!objects_.refresh_attachments(batch ? &changed : nullptr))
                return SegmentResult::Invalid;
            if (batch && !changed)
                batch->attachments_settled = true;
        }
        CollisionHit nearest;
        const auto world_result = collision->sweep(from, to, {}, nearest);
        if (world_result == CollisionQuery::Invalid)
            return SegmentResult::Invalid;
        bool found = world_result == CollisionQuery::Hit;
        const auto consider = [&](const ObjectRecord& record)
        {
            CollisionHit hit;
            const auto query = sweep_object(record, from, to, {}, hit);
            if (query == CollisionQuery::Invalid)
                return false;
            // USE follows entry-face rays: an enclosing body/sensor must not
            // mask all interactions. Later LithTech exposes interior hits as an
            // opt-in flag; ordinary USE does not request it.
            if ((objects == SegmentObjects::All || start == SegmentStart::SkipInterior) && hit.start_solid)
                return true;
            if (query == CollisionQuery::Hit && (!found || hit.fraction < nearest.fraction))
            {
                nearest = hit;
                nearest.object = record.handle;
                found = true;
            }
            return true;
        };
        if (!batch)
            for (auto id : objects_.snapshot())
            {
                const auto* record = objects_.get(id);
                if (!record ||
                    segment_excludes(objects_, *record, ignore, owner, projectile, fire_through, objects, filter))
                    continue;
                if (!consider(*record))
                    return SegmentResult::Invalid;
            }
        else
            for (const auto& blocker : batch->blockers)
            {
                // A shape sweep refuses offset endpoints outside physics range; never skip that refusal.
                if (blocker.bounded &&
                    (!blocker.shape || (physics_vector(vector_subtract(from, blocker.offset)) &&
                                        physics_vector(vector_subtract(to, blocker.offset)))) &&
                    !segment_reaches(from, to, blocker.min, blocker.max))
                {
                    ++segment_blockers_skipped;
                    continue;
                }
                const auto* record = objects_.get(blocker.id);
                if (!record || record->life != Life::Alive)
                    continue;
                ++segment_blockers_swept;
                if (!consider(*record))
                    return SegmentResult::Invalid;
            }
        if (!found)
            return SegmentResult::Miss;
        out = {nearest.point, nearest.normal, nearest.object, nearest.polygon, nearest.fraction, nearest.texture_flags};
        return SegmentResult::Hit;
    }
    void ServerAdapter::RecordBrushDestruction(HOBJECT handle, DVector direction, float damage, DBYTE amount,
                                               unsigned started, unsigned failed)
    {
        trace_service("RecordBrushDestruction");
        auto* object = objects_.get(handle);
        if (!object || !physics_vector(direction) || !std::isfinite(damage))
            return;
        if (brush_destructions.size() == 32)
            brush_destructions.erase(brush_destructions.begin());
        brush_destructions.push_back(
            {object->state.m_Name, object->state.m_Pos, direction, damage, amount, started, failed});
    }
    DRESULT ServerAdapter::WriteDamage(HMESSAGEWRITE h, const Damage& value)
    {
        trace_service("WriteDamage");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        auto* attacker = objects_.get(value.attacker);
        const auto* sender = objects_.get(m->sender);
        const auto* projectile = sender ? dynamic_cast<const CProjectile*>(sender->game.get()) : nullptr;
        const auto* lightning = sender ? dynamic_cast<const CClientLightningSFX*>(sender->game.get()) : nullptr;
        const bool lightning_attribution = lightning && lightning->AllowsDamageAttribution(value.attacker) &&
                                           value.area_effect && value.type == DamageType::Normal &&
                                           (!value.attacker || (attacker && attacker->life == Life::Alive));
        const bool attribution =
            sender && sender->life == Life::Alive &&
            (lightning_attribution || (attacker && attacker->life == Life::Alive && m->sender == value.attacker) ||
             (projectile && projectile->configured && projectile->m_hFiredFrom == value.attacker &&
              (!value.attacker || (attacker && attacker->life == Life::Alive))));
        if (m->id != damage_message || !m->bytes.empty() || m->damage || m->key || m->pickup || m->immobilize ||
            !attribution || !physics_vector(value.position) || !physics_vector(value.direction) ||
            !std::isfinite(value.amount) || value.amount < 0 || value.amount > 1e6f ||
            (value.type != DamageType::Fire && value.type != DamageType::Normal && value.type != DamageType::Melee &&
             value.type != DamageType::Explode && value.type != DamageType::Suffocate &&
             value.type != DamageType::Death && value.type != DamageType::FreeFall && value.type != DamageType::Blind &&
             value.type != DamageType::Slow && value.type != DamageType::Electric && value.type != DamageType::Acid &&
             value.type != DamageType::DropWeapon && value.type != DamageType::Vision &&
             value.type != DamageType::Singularity && value.type != DamageType::Leech))
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
        if (!m.borrowed || m.bad || m.id != damage_message || !m.damage ||
            (m.damage->attacker && (!attacker || attacker->life != Life::Alive)))
        {
            m.bad = true;
            return false;
        }
        value = *m.damage;
        return true; // Owned payload can be read by each aggregate.
    }
} // namespace OpenBlood2
