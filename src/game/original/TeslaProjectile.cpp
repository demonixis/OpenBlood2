// Selected original ObjectDLL/GameProjectiles.cpp CTeslaProjectile, CTeslaBoltProjectile
// and CTeslaBallProjectile. See tesla_contracts.md.
#include "TeslaProjectile.h"
#include "LightningFX.h"
#include "engine/collision.h"
#include "radius_damage.h"
#include <cmath>
using namespace OpenBlood2;
namespace
{
    constexpr float attract_min = 100, attract_max = 400, attract_time = 2.5f, attract_delay = .5f;
    constexpr float bolt_damage = 100, bolt_radius = 100;
    void publish(HOBJECT owner, SceneExplosionReport& r, DDWORD type, DVector position, DVector normal,
                 const char* sound, float radius)
    {
        auto* s = g_pServerDE;
        r = {};
        r.source = s->GetObjectName(owner);
        r.position = position;
        r.normal = normal;
        r.type = type;
        r.model_event = s->PublishExplosionModels(owner, type, position, normal, r.model_error);
        r.sprite_event = s->PublishExplosionSprites(owner, type, position, normal, r.sprite_error);
        if (type == 7)
            r.ring_event = s->PublishExplosionRings(owner, type, position, normal, r.ring_error);
        else
            r.flame_event = s->PublishExplosionFlames(owner, type, position, normal, r.flame_error);
        r.light_event = s->PublishExplosionLights(owner, type, position, normal, r.light_error);
        SoundRequest request;
        r.sound = request.resource = sound;
        request.inner_radius = radius * .1f; // PlaySoundFromPos.
        request.outer_radius = radius;
        request.priority = 1; // SOUNDPRIORITY_MISC_MEDIUM.
        r.voice = s->PlayDetachedSound(owner, request, position, r.sound_error);
        s->RecordSceneExplosion(r);
    }
    // INTERSECT_OBJECTS | IGNORE_NONSOLID with LiquidFilterFn, 2000 units forward; recast once
    // from the shooter's hit point when the shooter is the first object.
    bool cast(HOBJECT self, HOBJECT shooter, TeslaRay& ray)
    {
        auto* s = g_pServerDE;
        DRotation rotation;
        if (s->GetObjectRotation(self, &rotation) != DE_OK || s->GetObjectPos(self, &ray.from) != DE_OK)
            return false;
        ray.to = vector_add(ray.from, vector_scale(rotate_vector({0, 0, 1}, rotation), 2000));
        auto query = [&](DVector from, HOBJECT ignore)
        {
            return s->IntersectSegment(from, ray.to, ignore, ray.hit, false, SegmentObjects::SolidOnly,
                                       SegmentFilter::IgnoreVolumes);
        };
        ray.result = query(ray.from, nullptr);
        if (ray.result == SegmentResult::Hit && shooter && ray.hit.object == shooter)
        {
            ray.skipped_shooter = true;
            ray.result = query(ray.hit.point, shooter);
        }
        return ray.result == SegmentResult::Hit || ray.result == SegmentResult::Miss;
    }
} // namespace
CTeslaProjectile::~CTeslaProjectile()
{
    if (flight_sound && g_pServerDE)
        g_pServerDE->KillSound(flight_sound);
}
bool CTeslaProjectile::Update()
{
    // Hidden (scale zero) for its first 0.01 s, then 0.175.
    const float scale = g_pServerDE->GetTime() - scale_start < .01f ? 0 : .175f;
    if (!flight_sound)
    {
        // PlaySoundFromObject(.., 600, MISC_MEDIUM, loop, handle): attached and retained.
        SoundRequest request;
        request.resource = "Sounds\\Weapons\\tesla\\projectile.wav";
        request.speaker = m_hObject;
        request.space = SoundSpace::Spatial;
        request.outer_radius = 600;
        request.inner_radius = 60;
        request.priority = 1;
        request.reverb_requested = true;
        request.loop = true;
        flight_sound = g_pServerDE->PlayObjectLoopSound(m_hObject, request, sound_error);
    }
    if (!g_pServerDE->ScaleObject(m_hObject, {scale, scale, scale}))
        return Fail("Tesla projectile scale rejected");
    return CProjectile::Update();
}
void CTeslaProjectile::AddExplosion(DVector position, DVector normal)
{
    if (flight_sound)
    {
        g_pServerDE->KillSound(flight_sound);
        flight_sound = 0;
    }
    publish(m_hObject, effect, 7 /* EXP_TESLA_PRIMARY */, position, normal, "Sounds\\Weapons\\Tesla\\impact.wav", 750);
}
bool CTeslaBoltProjectile::Update()
{
    if (!configured)
        return Fail("Projectile never received Setup");
    if (!cast(m_hObject, m_hFiredFrom, ray))
        return Fail("Tesla bolt ray unavailable/invalid");
    if (ray.result == SegmentResult::Hit)
    {
        auto point = ray.hit.point;
        if (g_pServerDE->TeleportObject(m_hObject, &point) != DE_OK)
            return Fail("Tesla bolt teleport rejected");
        contact = ray.hit;
        contacted = exploded = true;
        m_hHitObject = ray.hit.object; // Impact normal query only; no inter-object link.
        if (!AddImpact(point))
            return false;
        if (m_nRadius)
            targets = DamageObjectsInRadius(m_hFiredFrom, this, point, float(m_nRadius), m_fDamage, damage_type);
        if (targets.cancelled)
            error = "Projectile damage traversal interrupted or unavailable";
        m_hHitObject = nullptr;
    }
    return false; // The source returns DFALSE after its single update: remove the bolt.
}
bool CTeslaBallProjectile::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .1f);
    if (!configured)
        return Fail("Projectile never received Setup");
    if (armed)
    {
        if (s->GetTime() - arm_time > attract_time)
            return false;
        if (s->GetTime() - last_fire_time > attract_delay)
            FireBolt();
        return !failed;
    }
    if (!cast(m_hObject, m_hFiredFrom, ray))
        return Fail("Tesla ball ray unavailable/invalid");
    auto destination = ray.result == SegmentResult::Hit ? ray.hit.point : ray.to;
    armed = true;
    arm_time = s->GetTime();
    if (s->TeleportObject(m_hObject, &destination) != DE_OK)
        return Fail("Tesla ball teleport rejected");
    if (ray.result == SegmentResult::Hit)
    {
        contact = ray.hit;
        m_hHitObject = ray.hit.object;
    }
    contacted = exploded = true;
    if (!AddImpact(destination))
        return false;
    if (m_nRadius)
        targets = DamageObjectsInRadius(m_hFiredFrom, this, destination, float(m_nRadius), m_fDamage, damage_type);
    if (targets.cancelled)
        error = "Projectile damage traversal interrupted or unavailable";
    m_hHitObject = nullptr;
    DVector zero{};
    if (s->IsObjectAlive(m_hObject) &&
        (s->SetVelocity(m_hObject, &zero) != DE_OK || s->SetAcceleration(m_hObject, &zero) != DE_OK))
        return Fail("Tesla ball stop rejected");
    return !failed;
}
void CTeslaBallProjectile::FireBolt()
{
    auto* s = g_pServerDE;
    attract_radius = attract_min + (attract_max - attract_min) * ((s->GetTime() - arm_time) / attract_time);
    DVector position;
    if (s->GetObjectPos(m_hObject, &position) != DE_OK)
        return;
    // Walk the original destructable list; the first close character other than the
    // previous target receives the bolt.
    for (DLink* link = CDestructable::m_DestructableHead.m_pNext; link && link != &CDestructable::m_DestructableHead;)
    {
        auto* destructable = static_cast<CDestructable*>(link->m_pData);
        link = link->m_pNext;
        const HOBJECT target = destructable ? destructable->m_hObject : nullptr;
        if (!target)
            break;
        DVector at;
        if (!s->IsObjectAlive(target) || !s->IsKindOf(s->GetObjectClass(target), s->GetClass("CBaseCharacter")) ||
            s->GetObjectPos(target, &at) != DE_OK)
            continue;
        const auto d = vector_subtract(at, position);
        if (vector_dot(d, d) > attract_radius * attract_radius || target == last_target)
            continue;
        lightning = s->PublishLightning(m_hObject, {position, at, 2, 1, 2}, lightning_error);
        if (lightning)
            ++lightning_count;
        last_bolt = DamageObjectsInRadius(m_hFiredFrom, this, at, bolt_radius, bolt_damage, DamageType::Electric);
        int variant = 0;
        if (s->RandomInt(0, 2, variant))
        {
            SoundRequest request;
            request.resource = "Sounds\\Weapons\\Tesla\\althit" + std::to_string(variant + 1) + ".wav";
            request.inner_radius = 150;
            request.outer_radius = 1500;
            request.priority = 1;
            s->PlayDetachedSound(m_hObject, request, position, sound_error);
        }
        last_fire_time = s->GetTime();
        last_target = target;
        ++bolts;
        break;
    }
}
void CTeslaBallProjectile::AddExplosion(DVector position, DVector normal)
{
    publish(m_hObject, effect, 8 /* EXP_TESLA_ALT */, position, normal, "Sounds\\Weapons\\Tesla\\altimpact.wav", 2000);
}
bool CShikariLoogieProjectile::Update()
{
    // Hidden (scale zero) for its first 0.01 s, then 0.25. An OT_NORMAL object has no
    // visual, so the scale is state only.
    const float scale = g_pServerDE->GetTime() - scale_start < .01f ? 0 : .25f;
    if (!g_pServerDE->ScaleObject(m_hObject, {scale, scale, scale}))
        return Fail("Loogie scale rejected");
    return CProjectile::Update();
}
void CShikariLoogieProjectile::AddExplosion(DVector, DVector)
{
    // m_bExplosion is false in the source, so AddImpact never calls this override (its
    // EXP_FLAME_SMALL placeholder and impact.wav are unreachable). Nothing to publish.
}

bool CShikariLoogieProjectile::InitialUpdate()
{
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 31, trail_error, .25f);
    return CProjectile::InitialUpdate();
}
