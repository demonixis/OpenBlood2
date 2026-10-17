#include "Grenade.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <numbers>
using namespace OpenBlood2;
CGrenade::CGrenade() : CProjectile(OT_MODEL)
{
    m_fLifeTime = 3;
    flags = FLAG_VISIBLE | FLAG_TOUCH_NOTIFY | FLAG_REMOVEIFOUTSIDE | FLAG_FORCECLIENTUPDATE | FLAG_SHADOW |
            FLAG_MODELGOURAUDSHADE | FLAG_RAYHIT | FLAG_GRAVITY;
}
CGrenade::~CGrenade()
{
    StopSound();
}
void CGrenade::StopSound()
{
    if (sound)
        g_pServerDE->KillSound(sound);
    sound = 0;
}
bool CGrenade::InitialUpdate()
{
    if (!CProjectile::InitialUpdate())
        return false;
    // The existing type-1 fragment trail is not a general object-bound trail.
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 1, trail_error);
    return true;
}
bool CGrenade::AdvanceFlight(double seconds)
{
    if (!configured || m_bExplode || failed || seconds == 0)
        return !failed;
    if (!g_pServerDE->AdvanceModelProjectile(m_hObject, seconds))
        return failed ? false : Fail("Grenade box flight unavailable/invalid (including embedded starts)");
    return !failed;
}
bool CGrenade::Update()
{
    if (!sound)
    {
        SoundRequest request;
        request.resource = "Sounds\\Weapons\\assault\\projectile.wav";
        request.outer_radius = 600;
        request.inner_radius = 60;
        request.priority = 1;
        request.speaker = m_hObject;
        request.space = SoundSpace::Spatial;
        request.loop = true;
        sound = g_pServerDE->PlayObjectLoopSound(m_hObject, request, sound_error);
    }
    if (g_pServerDE->GetObjectPos(m_hObject, &last_position) != DE_OK)
        return Fail("Grenade position unavailable");
    const bool alive = CProjectile::Update();
    if (failed || !g_pServerDE->IsObjectAlive(m_hObject))
        return false;
    DVector velocity;
    if (g_pServerDE->GetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("Grenade velocity unavailable");
    bool rotate = false;
    float next_pitch = pitch;
    if (vector_dot(velocity, velocity) < 25)
    {
        pitch_velocity = yaw_velocity = 0;
        next_pitch = 0; // Original keeps the accumulated pitch field unchanged.
        rotate = true;
    }
    else if (pitch_velocity != 0 || yaw_velocity != 0)
    {
        pitch += pitch_velocity * g_pServerDE->GetFrameTime();
        yaw += yaw_velocity * g_pServerDE->GetFrameTime();
        next_pitch = pitch;
        rotate = true;
    }
    if (rotate)
    {
        const auto rotation = rotation_from_euler({next_pitch, yaw, 0});
        if (g_pServerDE->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return Fail("Grenade rotation rejected");
    }
    // Preserve source precedence: (!alive && gravity) || dead. Base contact/death
    // already calls Explode; the source intentionally reaches a second sequential
    // explosion pass here. Reentrant calls are still guarded by the base.
    if ((!alive && (flags & FLAG_GRAVITY)) || m_damage.IsDead())
    {
        StopSound();
        Explode(true);
    }
    return alive;
}
void CGrenade::HandleContact(const SegmentHit& hit)
{
    if (!configured || failed || m_bExplode || !g_pServerDE->IsObjectAlive(m_hObject))
        return;
    if (hit.object && (!g_pServerDE->IsObjectAlive(hit.object) || hit.object == m_hFiredFrom))
        return;
    auto* character = dynamic_cast<CBaseCharacter*>(g_pServerDE->HandleToObject(hit.object));
    if (bounces_left == 5 && character)
    {
        if (!character->m_damage.IsDead())
            CProjectile::HandleContact(hit);
        return;
    }
    if ((hit.object && !(g_pServerDE->GetObjectFlags(hit.object) & FLAG_SOLID)) ||
        (!hit.object && hit.polygon == UINT32_MAX) || bounces_left <= 0)
        return;
    DVector velocity;
    if (g_pServerDE->GetVelocity(m_hObject, &velocity) != DE_OK)
    {
        Fail("Grenade bounce velocity unavailable");
        return;
    }
    // Source consumes the velocity AFTER engine contact resolution. The replacement
    // solver projects inward motion; do not invent elastic restitution here.
    if (hit.polygon != UINT32_MAX)
    {
        velocity = vector_subtract(velocity, vector_scale(hit.normal, vector_dot(velocity, hit.normal) * .3f));
        constexpr float circle = 2 * std::numbers::pi_v<float>;
        if (!g_pServerDE->RandomFloat(-circle, circle, pitch_velocity) ||
            !g_pServerDE->RandomFloat(-circle, circle, yaw_velocity))
        {
            Fail("Grenade spin randomness unavailable");
            return;
        }
        velocity = vector_scale(velocity, .5f);
    }
    else
        velocity = {};
    SoundRequest request;
    request.resource = "Sounds\\Weapons\\assault\\altbounce.wav";
    request.outer_radius = 750;
    request.inner_radius = 75;
    request.priority = 1;
    request.speaker = m_hObject;
    request.space = SoundSpace::Spatial;
    if (g_pServerDE->PlayCharacterSound(m_hObject, request, false, sound_error))
        ++bounce_sounds;
    if (g_pServerDE->SetVelocity(m_hObject, &velocity) != DE_OK)
    {
        Fail("Grenade bounce velocity rejected");
        return;
    }
    --bounces_left;
}
void CGrenade::AddExplosion(DVector position, DVector)
{
    // Original CProjectile::AddExplosion always uses world up, not the hit normal.
    ++explosion_passes;
    SceneExplosionReport r;
    r.source = g_pServerDE->GetObjectName(m_hObject);
    r.position = position;
    r.normal = {0, 1, 0};
    r.type = 3;
    r.damage = m_fDamage;
    r.radius = static_cast<float>(m_nRadius);
    r.model_event = g_pServerDE->PublishExplosionModels(m_hObject, r.type, position, r.normal, r.model_error);
    r.sprite_event = g_pServerDE->PublishExplosionSprites(m_hObject, r.type, position, r.normal, r.sprite_error);
    r.ring_event = g_pServerDE->PublishExplosionRings(m_hObject, r.type, position, r.normal, r.ring_error);
    r.fragment_event = g_pServerDE->PublishExplosionFragments(m_hObject, r.type, position, r.normal, r.fragment_error);
    r.flame_event = g_pServerDE->PublishExplosionFlames(m_hObject, r.type, position, r.normal, r.flame_error);
    r.light_event = g_pServerDE->PublishExplosionLights(m_hObject, r.type, position, r.normal, r.light_error);
    SoundRequest request;
    r.sound = request.resource = "Sounds\\Weapons\\c4\\explosion_1.wav";
    request.inner_radius = 100;
    request.outer_radius = 1000;
    request.priority = 1;
    r.voice = g_pServerDE->PlayDetachedSound(m_hObject, request, position, r.sound_error);
    g_pServerDE->RecordSceneExplosion(r);
}
