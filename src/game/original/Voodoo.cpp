// Selected original ObjectDLL/GameWeapons.cpp CVoodooDoll solo branches.
#include "InventoryMgr.h"
#include "PlayerObj.h"
#include "Weapon.h"
#include "engine/collision.h"
#include <cmath>

using namespace OpenBlood2;
FireResult CVoodooDoll::FireVector()
{
    auto* server = BaseClass::GetServerDE();
    auto* player = server ? dynamic_cast<CPlayerObj*>(server->HandleToObject(m_hOwner)) : nullptr;
    voodoo_error.clear();
    targets.clear();
    deliveries = 0;
    const auto fail = [&](const char* reason)
    {
        voodoo_error = reason;
        return last_shot.result = FireResult::DeliveryFailed;
    };
    if (!player || !server->IsObjectAlive(m_hOwner))
        return fail("Voodoo requires a live player owner");
    const bool alternate = last_shot.request.alternate;
    hit_effect = alternate ? 5 : last_shot.request.voodoo_hit;
    constexpr DamageType types[]{DamageType::Normal,     DamageType::Normal, DamageType::Blind,
                                 DamageType::DropWeapon, DamageType::Slow,   DamageType::Vision};
    damage_type = types[hit_effect];
    // The alternate source draws damage again, from the server RNG.
    if (alternate && !server->RandomFloat(data.m_fMinAltDamage, data.m_fMaxAltDamage, last_shot.damage))
        return fail("Voodoo alternate damage draw failed");
    DVector origin;
    if (server->GetObjectPos(m_hOwner, &origin) != DE_OK)
        return fail("Voodoo owner position unavailable");
    const float cp = std::cos(player->m_fPitch);
    const DVector forward{std::sin(player->m_fYaw) * cp, -std::sin(player->m_fPitch), std::cos(player->m_fYaw) * cp};
    std::vector<HOBJECT> candidates;
    auto& head = CDestructable::m_DestructableHead;
    for (auto* link = head.m_pNext; link && link != &head; link = link->m_pNext)
    {
        const auto* damage = static_cast<CDestructable*>(link->m_pData);
        if (!damage || !damage->m_hObject)
            break; // Original null-handle terminator.
        candidates.push_back(damage->m_hObject);
    }
    const auto deliver = [&](HOBJECT target, DVector point, DVector direction, float amount, DamageType type)
    {
        auto* owner = server->HandleToObject(m_hOwner);
        if (!owner || !server->IsObjectAlive(m_hOwner))
            return false;
        auto message = server->StartMessageToObject(owner, target, damage_message);
        if (!message)
            return false;
        const auto written = server->WriteDamage(message, {direction, point, amount, type, m_hOwner});
        const auto sent = server->EndMessage(message);
        if (written != DE_OK || sent != DE_OK)
            return false;
        ++deliveries;
        last_shot.damage_delivered = true;
        return true;
    };
    const auto impact = [&](HOBJECT target, const SegmentHit& hit, DVector direction)
    {
        float amount = last_shot.damage * m_pInventoryMgr->damage_multiplier;
        if (hit_effect == 1)
            amount *= 2;
        if (!deliver(target, hit.point, direction, amount, damage_type))
            return false;
        targets.push_back(target);
        last_shot.hit = hit;
        last_shot.collision = SegmentResult::Hit;
        if (!server->IsObjectAlive(m_hOwner))
        {
            last_shot.interrupted = true;
            return true;
        }
        // Removal during damage is deferred; do not publish a new child on a pending target.
        if (server->IsObjectAlive(target))
        {
            if (!server->PublishObjectParticles(target, 12 + hit_effect, voodoo_error))
                return false;
            DVector zero{};
            if (hit_effect == 4 && server->SetVelocity(target, &zero) != DE_OK)
                return false;
        }
        return true;
    };
    float closest = 3000;
    HOBJECT nearest = nullptr;
    SegmentHit nearest_hit;
    DVector nearest_direction{}; // The source leaves this uninitialized on a primary miss.
    for (auto candidate : candidates)
    {
        if (!server->IsObjectAlive(m_hOwner))
        {
            last_shot.interrupted = true;
            break;
        }
        if (candidate == m_hOwner || !server->IsObjectAlive(candidate) ||
            !dynamic_cast<CBaseCharacter*>(server->HandleToObject(candidate)))
            continue;
        DVector point;
        if (server->GetObjectPos(candidate, &point) != DE_OK)
            return fail("Voodoo target position unavailable");
        auto direction = vector_subtract(point, origin);
        const float distance = std::sqrt(vector_dot(direction, direction));
        if (distance > 3000 || distance < 1e-6f)
            continue; // Avoid the source's zero-length normalization.
        direction = vector_scale(direction, 1 / distance);
        if (vector_dot(direction, forward) < (alternate ? .65f : .9f))
            continue;
        SegmentHit hit;
        const auto query = server->IntersectSegment(origin, vector_add(point, vector_scale(direction, 15)), m_hOwner,
                                                    hit, false, SegmentObjects::SolidOnly, SegmentFilter::IgnoreVolumes,
                                                    SegmentStart::SkipInterior);
        if (query == SegmentResult::Invalid || query == SegmentResult::Unavailable)
            return fail("Voodoo visibility query unavailable");
        if (query != SegmentResult::Hit || hit.object != candidate)
            continue;
        if (alternate)
        {
            if (!impact(candidate, hit, direction))
                return fail("Voodoo alternate damage/effect failed");
            if (last_shot.interrupted)
                break;
        }
        else if (distance < closest)
        {
            closest = distance;
            nearest = candidate;
            nearest_hit = hit;
            nearest_direction = vector_scale(direction, 15); // Primary source preserves the extended-ray vector.
        }
    }
    if (!last_shot.interrupted)
    {
        if (!alternate && nearest)
        {
            if (!impact(nearest, nearest_hit, nearest_direction))
                return fail("Voodoo primary damage/effect failed");
        }
        else if (targets.empty() && (alternate || !candidates.empty()))
        {
            const float amount = alternate ? 100 : last_shot.damage * m_pInventoryMgr->damage_multiplier / 3;
            if (!deliver(m_hOwner, origin, alternate ? DVector{0, -1, 0} : DVector{}, amount,
                         alternate ? DamageType::Explode : damage_type))
                return fail("Voodoo self damage failed");
        }
    }
    return last_shot.result = FireResult::Fired;
}
