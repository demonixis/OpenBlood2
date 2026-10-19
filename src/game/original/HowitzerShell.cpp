// Original ObjectDLL/GameProjectiles.cpp CHowitzerShell (1162-1283). Used here by
// the Fanatic detonation; the player Howitzer weapon is not integrated.
#include "Projectile.h"
#include "engine/collision.h"
#include "radius_damage.h"
using namespace OpenBlood2;

bool CHowitzerShell::Update()
{
    if (!configured)
        return Fail("Projectile never received Setup");
    auto* s = g_pServerDE;
    // A Fanatic removes itself one update after creating its shell. The source
    // handle stays usable until the end of that frame; here a shooter pending
    // removal is unlinked now, so the shot is attributed to no one as after MID_LINKBROKEN.
    if (m_hFiredFrom && !s->IsObjectAlive(m_hFiredFrom))
    {
        s->BreakInterObjectLink(m_hObject, m_hFiredFrom);
        m_hFiredFrom = nullptr;
        ++orphaned;
    }
    DRotation rotation;
    if (s->GetObjectRotation(m_hObject, &rotation) != DE_OK || s->GetObjectPos(m_hObject, &ray_from) != DE_OK)
        return Fail("Howitzer shell pose unavailable");
    const auto distance = vector_scale(rotate_vector({0, 0, 1}, rotation), 3000);
    ray_to = vector_add(ray_from, distance);
    // INTERSECT_OBJECTS | IGNORE_NONSOLID with the original LiquidFilterFn.
    const auto cast = [&](DVector from, HOBJECT ignore)
    {
        return s->IntersectSegment(from, ray_to, ignore, ray_hit, false, SegmentObjects::SolidOnly,
                                   SegmentFilter::IgnoreVolumes);
    };
    ray_result = cast(ray_from, nullptr);
    // "Don't shoot the sap who fired this": recast once from the shooter hit.
    if (ray_result == SegmentResult::Hit && m_hFiredFrom && ray_hit.object == m_hFiredFrom)
    {
        skipped_shooter = true;
        ray_result = cast(ray_hit.point, m_hFiredFrom);
    }
    if (ray_result != SegmentResult::Hit && ray_result != SegmentResult::Miss)
        return Fail("Howitzer shell ray unavailable/invalid");
    if (ray_result == SegmentResult::Hit)
    {
        auto point = ray_hit.point;
        if (s->TeleportObject(m_hObject, &point) != DE_OK)
            return Fail("Howitzer shell teleport rejected");
        contact = ray_hit;
        contacted = exploded = true;
        m_hHitObject = ray_hit.object; // Impact normal query only; no inter-object link.
        if (!AddImpact(point))
            return false;
        const HOBJECT responsible = m_hFiredFrom;
        if (m_nRadius)
            targets = DamageObjectsInRadius(responsible, this, point, float(m_nRadius), m_fDamage, damage_type);
        if (targets.cancelled)
            error = "Projectile damage traversal interrupted or unavailable";
        // A character shooter also damages the object it hit directly (no world HOBJECT here).
        const auto shooter = responsible ? s->GetObjectClass(responsible) : nullptr;
        if (!alternate && shooter && s->IsKindOf(shooter, s->GetClass("CBaseCharacter")) && ray_hit.object &&
            s->IsObjectAlive(ray_hit.object) && s->IsObjectAlive(m_hObject))
        {
            if (const auto message = s->StartMessageToObject(this, ray_hit.object, damage_message))
            {
                direct_hit = s->WriteDamage(message, {distance, point, m_fDamage, damage_type, responsible}) == DE_OK;
                direct_hit = s->EndMessage(message) == DE_OK && direct_hit;
            }
        }
        m_hHitObject = nullptr;
    }
    return false; // The source returns DFALSE after its single update: remove the shell.
}

void CHowitzerShell::AddExplosion(DVector position, DVector normal)
{
    // EXP_HOWITZER_PRIMARY (9). Components without a ported recipe report errors.
    auto& r = effect;
    r = {};
    r.source = g_pServerDE->GetObjectName(m_hObject);
    r.position = position;
    r.normal = normal;
    r.type = 9;
    r.damage = m_fDamage;
    r.radius = float(m_nRadius);
    r.model_event = g_pServerDE->PublishExplosionModels(m_hObject, 9, position, normal, r.model_error);
    r.sprite_event = g_pServerDE->PublishExplosionSprites(m_hObject, 9, position, normal, r.sprite_error);
    r.ring_event = g_pServerDE->PublishExplosionRings(m_hObject, 9, position, normal, r.ring_error);
    r.fragment_event = g_pServerDE->PublishExplosionFragments(m_hObject, 9, position, normal, r.fragment_error);
    r.flame_event = g_pServerDE->PublishExplosionFlames(m_hObject, 9, position, normal, r.flame_error);
    r.light_event = g_pServerDE->PublishExplosionLights(m_hObject, 9, position, normal, r.light_error);
    SoundRequest request;
    r.sound = request.resource = "Sounds\\Weapons\\c4\\explosion_1.wav";
    request.inner_radius = 100; // PlaySoundFromPos: radius * 0.1.
    request.outer_radius = 1000;
    request.priority = 1; // SOUNDPRIORITY_MISC_MEDIUM.
    r.voice = g_pServerDE->PlayDetachedSound(m_hObject, request, position, r.sound_error);
    g_pServerDE->RecordSceneExplosion(r);
}
