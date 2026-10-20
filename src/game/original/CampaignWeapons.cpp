// Selected original GameWeapons.cpp; primary shared paths remain CWeapon.
#include "CampaignWeapons.h"
#include "InventoryMgr.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
FireResult CGideonSpear::FireVector()
{
    auto* s = g_pServerDE;
    immobilize_target = nullptr;
    immobilize_error.clear();
    if (!s || !firing || !s->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    SegmentHit hit;
    const auto query = s->IntersectSegment(last_shot.request.position, last_shot.end, nullptr, hit, false,
                                           SegmentObjects::SolidOnly, SegmentFilter::None, SegmentStart::SkipInterior);
    if (query != SegmentResult::Hit && query != SegmentResult::Miss)
    {
        immobilize_error = "Spear preliminary ray unavailable";
        return last_shot.result = FireResult::Unavailable;
    }
    if (query == SegmentResult::Hit && dynamic_cast<CPlayerObj*>(s->HandleToObject(hit.object)))
    {
        immobilize_target = hit.object;
        auto message = s->StartMessageToObject(s->HandleToObject(m_hOwner), hit.object, immobilize_message);
        const auto written = message ? s->WriteImmobilize(message, 4.f) : DE_ERROR;
        const auto delivered = message ? s->EndMessage(message) : DE_ERROR;
        if (written != DE_OK || delivered != DE_OK)
        {
            immobilize_error = "Spear immobilization message failed";
            return last_shot.result = FireResult::DeliveryFailed;
        }
        ++immobilize_deliveries; // Transport count; an already imprisoned player ignores the request.
    }
    return CWeapon::FireVector(); // Original independent ordinary damage ray follows the prepass.
}
FireResult CZealotSpell::FireVector()
{
    auto* s = g_pServerDE;
    if (!firing || !s->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    if (data.m_nType == WEAP_ZEALOT_HEAL)
    {
        const auto event = s->PublishObjectParticles(m_hOwner, 18, effect_error);
        // Health changes only on the original actor's animation completion.
        return last_shot.result = event && effect_error.empty() ? FireResult::Fired : FireResult::Unavailable;
    }
    SegmentHit hit;
    const auto query = s->IntersectSegment(last_shot.request.position, last_shot.end, nullptr, hit, false,
                                           SegmentObjects::SolidOnly, SegmentFilter::None, SegmentStart::SkipInterior);
    if (query != SegmentResult::Hit && query != SegmentResult::Miss)
        return last_shot.result = FireResult::Unavailable;
    if (query == SegmentResult::Hit && dynamic_cast<CBaseCharacter*>(s->HandleToObject(hit.object)))
        s->PublishObjectParticles(hit.object, 19, effect_error);
    // Original shield only emits particles, never armor or invulnerability.
    return last_shot.result = FireResult::Fired;
}
void CDeathRay::BeginFire(bool alternate)
{
    if (!alternate)
        return;
    area_deliveries = 0;
    area_error.clear();
    auto* s = g_pServerDE;
    auto* player = s->IsObjectAlive(m_hOwner) ? dynamic_cast<CPlayerObj*>(s->HandleToObject(m_hOwner)) : nullptr;
    if (!player || !m_pInventoryMgr)
    {
        area_error = "DeathRay FOV requires its original player owner";
        return;
    }
    DVector from;
    if (s->GetObjectPos(m_hOwner, &from) != DE_OK)
    {
        area_error = "DeathRay owner pose unavailable";
        return;
    }
    const auto forward = rotate_vector(rotation_from_euler({player->m_fPitch, player->m_fYaw, 0}), {0, 0, 1});
    effect_error = "OBJFX_DEATHRAY_RING_1/2 rotating emitter unavailable";
    std::vector<HOBJECT> snapshot;
    auto& head = CDestructable::m_DestructableHead;
    for (auto* link = head.m_pNext; link && link != &head; link = link->m_pNext)
    {
        const auto h = static_cast<CDestructable*>(link->m_pData)->m_hObject;
        if (!h)
            break;
        snapshot.push_back(h);
    }
    // Source prologue precedes CWeapon::Fire's next damage/ammo draw: retain the previous shot's damage.
    const float damage = last_shot.damage * m_pInventoryMgr->damage_multiplier;
    for (auto h : snapshot)
    {
        if (!s->IsObjectAlive(m_hOwner))
        {
            area_error = "DeathRay owner removed during FOV damage";
            break;
        }
        if (h == m_hOwner || !s->IsObjectAlive(h))
            continue;
        DVector p;
        if (s->GetObjectPos(h, &p) != DE_OK)
            continue;
        auto direction = vector_subtract(p, from);
        const auto distance = std::sqrt(vector_dot(direction, direction));
        if (distance > 250 || distance < 1e-6f)
            continue;
        direction = vector_scale(direction, 1 / distance);
        if (vector_dot(direction, forward) < .9f)
            continue;
        SegmentHit hit;
        const auto query =
            s->IntersectSegment(from, vector_add(p, vector_scale(direction, 15)), nullptr, hit, false,
                                SegmentObjects::SolidOnly, SegmentFilter::IgnoreVolumes, SegmentStart::SkipInterior);
        if (query == SegmentResult::Invalid || query == SegmentResult::Unavailable)
        {
            area_error = "DeathRay FOV ray failed";
            break;
        }
        if (query != SegmentResult::Hit || hit.object != h)
            continue;
        damage_type = DamageType::Electric;
        auto message = s->StartMessageToObject(s->HandleToObject(m_hOwner), h, damage_message);
        if (!message)
        {
            area_error = "DeathRay FOV message failed";
            break;
        }
        const auto written = s->WriteDamage(message, {direction, hit.point, damage, damage_type, m_hOwner});
        const auto delivered = s->EndMessage(message);
        if (written != DE_OK || delivered != DE_OK)
        {
            area_error = "DeathRay FOV delivery failed";
            break;
        }
        ++area_deliveries;
        if (s->IsObjectAlive(h))
            s->PublishObjectParticles(h, 39, effect_error);
    }
}

FireResult CDrudgeLightning::FireVector()
{
    auto* s = g_pServerDE;
    SegmentHit hit;
    const auto q = s->IntersectSegment(last_shot.request.position, last_shot.end, nullptr, hit, false,
                                       SegmentObjects::SolidOnly, SegmentFilter::None, SegmentStart::SkipInterior);
    if (q == SegmentResult::Hit || q == SegmentResult::Miss)
    {
        lightning_end = q == SegmentResult::Hit ? hit.point : last_shot.end;
        effect_error = "SFX_LIGHTNING boxed/thin/high-segment renderer unavailable";
    }
    else
        effect_error = "Drudge lightning visual ray unavailable";
    return CWeapon::FireVector(); // The source's separate damage ray keeps the ordinary filters.
}
