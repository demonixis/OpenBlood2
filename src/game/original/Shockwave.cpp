// Selected original GameProjectiles.cpp:5055-5165. See WIND_WEAPON_CONTRACTS.md.
#include "Projectile.h"
#include "engine/collision.h"
#include <cmath>
#include <vector>
using namespace OpenBlood2;
void CBehemothShockwaveProjectile::AddExplosion(DVector position, DVector normal)
{
    auto& report = effect;
    report = {};
    report.source = g_pServerDE->GetObjectName(m_hObject);
    report.position = position;
    report.normal = normal;
    report.type = 20; // EXP_SHOCKBALL_LARGE: original high-detail model and one evaluated light.
    report.damage = m_fDamage;
    report.radius = 450;
    report.model_event = g_pServerDE->PublishExplosionModels(m_hObject, 20, position, normal, report.model_error);
    report.light_event = g_pServerDE->PublishExplosionLights(m_hObject, 20, position, normal, report.light_error);
    SoundRequest request;
    report.sound = request.resource = "Sounds\\Weapons\\time\\explode.wav";
    request.inner_radius = 100;
    request.outer_radius = 1000;
    request.priority = 1;
    report.voice = g_pServerDE->PlayDetachedSound(m_hObject, request, position, report.sound_error);
    // FX/audio precede damage, independently of their success. The owned report
    // is recorded after traversal so inspection contains the real delivery counts.
}
bool CBehemothShockwaveProjectile::Update()
{
    if (exploded)
        return true; // A nested update cannot finish/remove the outer callback.
    if (failed)
        return false;
    if (!configured || !g_pServerDE->IsObjectAlive(m_hFiredFrom))
        return Fail("Shockwave requires a configured live shooter at its first update");
    // Latch before callbacks: a damage receiver must not re-enter this one-shot.
    exploded = true;
    DVector position, normal{0, -1000, 0};
    if (g_pServerDE->GetObjectPos(m_hFiredFrom, &position) != DE_OK)
        return Fail("Shockwave shooter position unavailable");
    ground_from = position;
    // Original iq.m_To += normal reads uninitialized storage. Define the intended
    // downward segment explicitly. Skip enclosing bodies, as for existing entry-face rays.
    ground_to = vector_add(position, normal);
    ground_result =
        g_pServerDE->IntersectSegment(ground_from, ground_to, nullptr, ground_hit, false, SegmentObjects::SolidOnly,
                                      SegmentFilter::None, SegmentStart::SkipInterior);
    if (ground_result != SegmentResult::Hit && ground_result != SegmentResult::Miss)
        return Fail("Shockwave ground query unavailable/invalid");
    if (ground_result == SegmentResult::Hit)
    {
        position = ground_hit.point;
        normal = ground_hit.normal;
    }
    blast_position = position;
    blast_normal = normal;
    AddExplosion(position, normal);
    // Preserve original destructible-list order and center-distance cutoff. This
    // deliberately is NOT DamageObjectsInRadius: no falloff, dimensions or LOS.
    std::vector<HOBJECT> snapshot;
    snapshot.reserve(CDestructable::m_dwNumDestructables);
    auto& head = CDestructable::m_DestructableHead;
    for (auto* link = head.m_pNext; link && link != &head; link = link->m_pNext)
    {
        auto target = static_cast<CDestructable*>(link->m_pData)->m_hObject;
        if (!target)
            break; // Original null-object terminates traversal.
        snapshot.push_back(target);
    }
    targets.candidates = static_cast<unsigned>(snapshot.size());
    const auto shooter = m_hFiredFrom;
    for (auto target : snapshot)
    {
        if (!g_pServerDE->IsObjectAlive(m_hObject) || !g_pServerDE->IsObjectAlive(shooter))
        {
            targets.cancelled = true;
            break;
        }
        if (target == shooter || !g_pServerDE->IsObjectAlive(target))
            continue;
        DVector point;
        if (g_pServerDE->GetObjectPos(target, &point) != DE_OK)
        {
            targets.cancelled = true;
            break;
        }
        const auto direction = vector_subtract(point, position);
        if (std::sqrt(vector_dot(direction, direction)) > 450.f)
            continue;
        ++targets.in_radius;
        auto message = g_pServerDE->StartMessageToObject(this, target, damage_message);
        if (!message)
        {
            targets.cancelled = true;
            break;
        }
        const auto written =
            g_pServerDE->WriteDamage(message, {direction, point, m_fDamage, DamageType::Explode, shooter});
        const auto delivered = g_pServerDE->EndMessage(message);
        if (written != DE_OK || delivered != DE_OK)
        {
            targets.cancelled = true;
            break;
        }
        ++targets.delivered;
    }
    if (targets.cancelled)
        error = "Shockwave damage traversal interrupted or unavailable";
    effect.targets = targets;
    g_pServerDE->RecordSceneExplosion(effect);
    return false; // Original Update returns DFALSE: parent removes after the callback.
}
