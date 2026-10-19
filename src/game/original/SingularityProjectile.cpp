// Original ObjectDLL/GameProjectiles.cpp CSingularityProjectile (Monolith).
#include "SingularityProjectile.h"
#include "engine/collision.h"
#include "game/weapon_messages.h"
#include <cmath>
using namespace OpenBlood2;
CSingularityProjectile::CSingularityProjectile() : CProjectile(OT_MODEL)
{
    // No projectile file: the original PostPropRead falls back to the grenade model/skin,
    // and the zeroed m_vInitScale keeps it invisible. ScaleObject refuses zero model scales
    // here, so the port keeps scale 1 and drops FLAG_VISIBLE instead (its 1-unit box and
    // RAYHIT flag are unchanged: dimensions do not follow the scale).
    filename = "Models\\Ammo\\Grenade.abc";
    skin = "Skins\\Ammo\\Grenade.dtx";
    damage_type = DamageType::Singularity;
    flags = FLAG_TOUCH_NOTIFY | FLAG_REMOVEIFOUTSIDE | FLAG_FORCECLIENTUPDATE | FLAG_SHADOW | FLAG_MODELGOURAUDSHADE |
            FLAG_RAYHIT | FLAG_POINTCOLLIDE | FLAG_NOSLIDING;
}
void CSingularityProjectile::Damage(HOBJECT target, float amount, DVector at)
{
    // DamageObject(m_hFiredFrom, this, target, ...): sent by the projectile, attributed to the shooter.
    auto* s = g_pServerDE;
    if (auto message = s->StartMessageToObject(this, target, damage_message))
    {
        if (s->WriteDamage(message, {m_vDir, at, amount, damage_type, m_hFiredFrom}) == DE_OK &&
            s->EndMessage(message) == DE_OK)
            ++damage_messages;
    }
}
bool CSingularityProjectile::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .1f);
    if (!configured)
        return Fail("Projectile never received Setup");
    if (armed)
    {
        if (s->GetTime() - arm_time > attract_time)
            return false;
        DVector position;
        if (s->GetObjectPos(m_hObject, &position) != DE_OK)
            return Fail("Singularity position unavailable");
        const auto character = s->GetClass("CBaseCharacter");
        // Suck stuff in: walk the original destructable list. Removal is deferred, so the
        // following link stays valid across damage callbacks.
        for (DLink* link = CDestructable::m_DestructableHead.m_pNext;
             link && link != &CDestructable::m_DestructableHead;)
        {
            auto* destructable = static_cast<CDestructable*>(link->m_pData);
            link = link->m_pNext;
            const HOBJECT target = destructable ? destructable->m_hObject : nullptr;
            if (!target)
                break;
            if (target == m_hObject || !s->IsObjectAlive(target) || s->GetObjectType(target) != OT_MODEL ||
                !(s->GetObjectUserFlags(target) & (1u << 5))) // USRFLG_SINGULARITY_ATTRACT
                continue;
            DVector at;
            if (s->GetObjectPos(target, &at) != DE_OK)
                continue;
            const auto delta = vector_subtract(at, position);
            const float distance2 = vector_dot(delta, delta);
            if (distance2 > attract_radius * attract_radius)
                continue;
            const bool is_character = s->IsKindOf(s->GetObjectClass(target), character);
            if (distance2 <= kill_radius * kill_radius)
            {
                DVector zero{};
                s->SetVelocity(target, &zero);
                auto center = position;
                s->MoveObject(target, &center);
                if (!is_character)
                {
                    // The source also sends SFX_OBJECTFX_ID / OBJFX_SINGULARITY_1 at the hole.
                    s->RemoveObject(target);
                    ++swallowed;
                    ++object_fx_unavailable;
                }
                else
                    Damage(target, m_fDamage * 2, position);
                continue;
            }
            // Halfway in: characters start taking damage.
            if (distance2 <= attract_radius * attract_radius / 2 && is_character)
                Damage(target, m_fDamage, position);
            if (!s->IsObjectAlive(target))
                continue;
            const float acceleration = attract_radius - distance2 / attract_radius;
            DVector velocity;
            if (s->GetVelocity(target, &velocity) != DE_OK)
                continue;
            const auto pull = vector_scale(vector_scale(delta, -1 / std::sqrt(distance2)), acceleration * .75f);
            velocity = vector_add(velocity, pull);
            s->SetVelocity(target, &velocity);
            ++pulls;
        }
        return !failed;
    }
    // Where does the beam stop: forward 3000 units, solid objects, liquid filter, one
    // recast from the shooter's hit point when it is the first object.
    DVector from;
    DRotation rotation;
    if (s->GetObjectPos(m_hObject, &from) != DE_OK || s->GetObjectRotation(m_hObject, &rotation) != DE_OK)
        return Fail("Singularity transform unavailable");
    const auto to = vector_add(from, vector_scale(rotate_vector({0, 0, 1}, rotation), 3000));
    SegmentHit hit;
    auto query = [&](DVector start, HOBJECT ignore)
    {
        return s->IntersectSegment(start, to, ignore, hit, false, SegmentObjects::SolidOnly,
                                   SegmentFilter::IgnoreVolumes);
    };
    ray = query(from, nullptr);
    if (ray == SegmentResult::Hit && m_hFiredFrom && hit.object == m_hFiredFrom)
    {
        skipped_shooter = true;
        ray = query(hit.point, m_hFiredFrom);
    }
    if (ray != SegmentResult::Hit && ray != SegmentResult::Miss)
        return Fail("Singularity ray unavailable/invalid");
    ray_end = ray == SegmentResult::Hit ? hit.point : to;
    ray_normal = ray == SegmentResult::Hit ? hit.normal : DVector{0, 1, 0};
    armed = true;
    arm_time = s->GetTime();
    // 25 units off the surface (world up on a miss), then the normalized normal.
    auto destination = vector_add(ray_end, vector_scale(ray_normal, 25));
    const auto length = std::sqrt(vector_dot(ray_normal, ray_normal));
    const auto normal = length > 0 ? vector_scale(ray_normal, 1 / length) : DVector{0, 1, 0};
    if (s->TeleportObject(m_hObject, &destination) != DE_OK)
        return Fail("Singularity teleport rejected");
    AddExplosion(destination, normal);
    DVector zero{};
    if (s->SetVelocity(m_hObject, &zero) != DE_OK || s->SetAcceleration(m_hObject, &zero) != DE_OK)
        return Fail("Singularity stop rejected");
    return !failed;
}
void CSingularityProjectile::AddExplosion(DVector position, DVector normal)
{
    auto* s = g_pServerDE;
    auto& r = effect;
    r = {};
    r.source = s->GetObjectName(m_hObject);
    r.position = position;
    r.normal = normal;
    r.type = 18; // EXP_SINGULARITY_PRIMARY
    r.model_event = s->PublishExplosionModels(m_hObject, r.type, position, normal, r.model_error);
    r.sprite_event = s->PublishExplosionSprites(m_hObject, r.type, position, normal, r.sprite_error);
    r.ring_event = s->PublishExplosionRings(m_hObject, r.type, position, normal, r.ring_error);
    r.fragment_event = s->PublishExplosionFragments(m_hObject, r.type, position, normal, r.fragment_error);
    r.flame_event = s->PublishExplosionFlames(m_hObject, r.type, position, normal, r.flame_error);
    r.light_event = s->PublishExplosionLights(m_hObject, r.type, position, normal, r.light_error);
    SoundRequest request;
    r.sound = request.resource = "Sounds\\Weapons\\Singularity\\impact.wav";
    request.inner_radius = 200; // PlaySoundFromPos: radius * 0.1.
    request.outer_radius = 2000;
    request.priority = 1; // SOUNDPRIORITY_MISC_MEDIUM.
    r.voice = s->PlayDetachedSound(m_hObject, request, position, r.sound_error);
    s->RecordSceneExplosion(r);
}
