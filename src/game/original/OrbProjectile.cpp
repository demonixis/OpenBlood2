#include "OrbProjectile.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
COrbProjectile::COrbProjectile() : CProjectile(OT_MODEL)
{
    filename = "Models\\Ammo\\orb.abc";
    skin = "Skins\\Powerups\\orb_pu.dtx";
    damage_type = DamageType::Normal;
}
COrbProjectile::~COrbProjectile()
{
    if (g_pServerDE && loop)
        g_pServerDE->KillSound(loop);
}
bool COrbProjectile::InitialUpdate()
{
    if (!CProjectile::InitialUpdate() || !g_pServerDE->ScaleObject(m_hObject, {.5f, .5f, .5f}))
        return false;
    if (!alternate)
        trail = g_pServerDE->PublishObjectParticles(m_hObject, 33, trail_error);
    return alternate || (trail && trail_error.empty());
}
bool COrbProjectile::SetCamera(bool on)
{
    auto* s = g_pServerDE;
    auto* p = s->IsObjectAlive(m_hFiredFrom) ? dynamic_cast<CPlayerObj*>(s->HandleToObject(m_hFiredFrom)) : nullptr;
    if (!p || !p->GetClient())
        return true;
    if (s->SendToClient(p->GetClient(), OrbView{m_hObject, on}) != DE_OK)
    {
        camera_error = "Orb camera message rejected";
        return Fail(camera_error.c_str());
    }
    camera_started = on;
    return true;
}
bool COrbProjectile::StartCamera()
{
    return !alternate || SetCamera(true); // Typed equivalent of the original precreate client-ID SFX.
}
bool COrbProjectile::Steer()
{
    auto* s = g_pServerDE;
    DVector position, velocity;
    DRotation rotation;
    if (s->GetObjectPos(m_hObject, &position) != DE_OK || s->GetObjectRotation(m_hObject, &rotation) != DE_OK ||
        s->GetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("Orb motion pose unavailable");
    if (alternate)
    {
        auto* p = s->IsObjectAlive(m_hFiredFrom) ? dynamic_cast<CPlayerObj*>(s->HandleToObject(m_hFiredFrom)) : nullptr;
        if (p)
        {
            if (p->m_bDead)
            {
                m_bExplode = true;
                return true;
            }
            // Source leaves rRot uninitialized for a player that has never sent PLAYERUPDATE.
            // Retain the current projectile orientation until the first checked update instead.
            if (p->rotation_received)
            {
                rotation = rotation_from_euler({p->m_fPitch, p->m_fYaw, 0});
                if (s->SetObjectRotation(m_hObject, &rotation) != DE_OK)
                    return Fail("Orb player steering failed");
            }
        }
    }
    else
    {
        tracked = nullptr;
        DVector target{};
        float nearest = 175 * 175;
        auto& head = CDestructable::m_DestructableHead;
        for (auto* link = head.m_pNext; link && link != &head; link = link->m_pNext)
        {
            const auto h = static_cast<CDestructable*>(link->m_pData)->m_hObject;
            if (!h)
                break;
            if (h == m_hFiredFrom || !s->IsObjectAlive(h) || !dynamic_cast<CBaseCharacter*>(s->HandleToObject(h)))
                continue;
            DVector v, p;
            DRotation r;
            if (s->GetVelocity(h, &v) != DE_OK)
                return Fail("Orb target velocity unavailable");
            // Original compares signed components, not speed or absolute components.
            if ((v.x < .1f && v.y < .1f && v.z < .1f) || !s->GetModelNodeTransform(h, "head", &p, &r))
                continue;
            const auto delta = vector_subtract(p, position);
            const float square = vector_dot(delta, delta);
            if (square <= nearest)
            {
                tracked = h;
                target = p;
                nearest = square;
            }
        }
        if (tracked && nearest > 1e-12f)
        {
            DRotation desired, blended;
            if (s->AlignRotation(desired, vector_subtract(target, position)) != DE_OK ||
                s->InterpolateRotation(blended, rotation, desired, 1 - std::sqrt(nearest) / 175) != DE_OK ||
                s->SetObjectRotation(m_hObject, &blended) != DE_OK)
                return Fail("Orb steering failed");
            rotation = blended;
        }
    }
    velocity = vector_scale(rotate_vector(rotation, {0, 0, 1}), std::sqrt(vector_dot(velocity, velocity)));
    return s->SetVelocity(m_hObject, &velocity) == DE_OK || Fail("Orb velocity rejected");
}
bool COrbProjectile::FollowHead()
{
    auto* s = g_pServerDE;
    if (!m_hHitObject || !s->IsObjectAlive(m_hHitObject) || s->GetTime() - hit_time > 5.1f)
    {
        m_bExplode = true;
        return true;
    }
    DVector position;
    DRotation rotation;
    if (s->GetModelNodeTransform(m_hHitObject, "head", &position, &rotation))
    {
        const auto forward = vector_scale(rotate_vector(rotation, {0, 0, 1}), 7.5f);
        position = vector_add(position, forward);
        DRotation turn;
        turn.m_Vec = rotate_vector(rotation, {0, 1, 0});
        turn.m_Spin = 0;
        rotation = compose_rotation(turn, rotation);
        if (s->TeleportObject(m_hObject, &position) != DE_OK || s->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return Fail("Attached orb head pose rejected");
        if (s->GetTime() - last_damage_time > 1)
        {
            last_damage_time = s->GetTime();
            const auto target = m_hHitObject;
            const auto message = s->StartMessageToObject(this, target, damage_message);
            if (!message)
                return Fail("Orb damage allocation failed");
            const auto written = s->WriteDamage(message, {forward, position, m_fDamage, damage_type, m_hFiredFrom});
            const auto delivered = s->EndMessage(message);
            if (written != DE_OK || delivered != DE_OK)
                return Fail("Orb attached damage failed");
            ++damage_ticks;
            if (!s->IsObjectAlive(m_hObject))
                return false;
            blood = {};
            blood.m_vDestPos = position;
            blood.m_eSurfaceType = SURFTYPE_FLESH;
            blood.blood_requested = true; // AddBloodImpact explicitly supplies WFX_BLOODSPURT, no random admission.
            if (!blood.CreateBloodSpurt(*s, m_hObject, forward, m_fDamage, cosmetic_random_) ||
                !blood.CreateSparks(*s, m_hObject, forward, m_fDamage, false, cosmetic_random_))
            {
                error = blood.blood_error + blood.spark_error;
                return Fail(error.c_str());
            }
        }
    } // Original missing-head branch stays attached without periodic damage.
    if (s->IsObjectAlive(m_hHitObject))
        if (auto* p = dynamic_cast<CPlayerObj*>(s->HandleToObject(m_hHitObject)); p && p->m_bDead)
            m_bExplode = true;
    return true;
}
bool COrbProjectile::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Orb setup unavailable");
    if (!loop)
    {
        SoundRequest request;
        request.resource = attached    ? "Sounds\\Weapons\\orb\\loopdrilling.wav"
                           : alternate ? "Sounds\\Weapons\\orb\\altloop.wav"
                                       : "Sounds\\Weapons\\orb\\loop.wav";
        request.inner_radius = 75;
        request.outer_radius = 750;
        request.priority = 1;
        request.loop = true;
        request.speaker = m_hObject;
        request.reverb_requested = false;
        loop = s->PlayObjectLoopSound(m_hObject, request, sound_error);
        if (!loop)
            return Fail(sound_error.c_str());
    }
    if (alternate && create_camera)
    {
        if (auto* p =
                s->IsObjectAlive(m_hFiredFrom) ? dynamic_cast<CPlayerObj*>(s->HandleToObject(m_hFiredFrom)) : nullptr)
            if (!p->SetOrbObject(m_hObject))
                return Fail("Orb listener link rejected");
        create_camera = false;
    }
    if (m_bExplode || m_damage.IsDead())
    {
        s->KillSound(loop);
        loop = 0;
        if (alternate && !SetCamera(false))
            return false;
        Explode();
        return false;
    }
    return attached ? FollowHead() : Steer(); // No base lifetime timeout in either original override.
}
void COrbProjectile::HandleContact(const SegmentHit& hit)
{
    auto* s = g_pServerDE;
    if (!configured || m_hHitObject || failed || !s->IsObjectAlive(m_hObject) || hit.object == m_hFiredFrom)
        return;
    if (hit.object && (!s->IsObjectAlive(hit.object) || !(s->GetObjectFlags(hit.object) & FLAG_SOLID)))
        return;
    if (!hit.object && hit.polygon == UINT32_MAX)
        return;
    if (alternate && !SetCamera(false))
        return;
    contacted = true;
    contact = hit;
    if (!attached)
    {
        bool attach = hit.object && dynamic_cast<CBaseCharacter*>(s->HandleToObject(hit.object));
        if (attach && alternate)
        {
            DVector head, pos;
            DRotation r;
            attach = s->GetModelNodeTransform(hit.object, "head", &head, &r) &&
                     s->GetObjectPos(m_hObject, &pos) == DE_OK &&
                     vector_dot(vector_subtract(head, pos), vector_subtract(head, pos)) <= 45 * 45;
        }
        if (attach)
        {
            DVector zero{};
            if (s->SetVelocity(m_hObject, &zero) != DE_OK)
            {
                Fail("Orb attachment velocity rejected");
                return;
            }
            m_hHitObject = hit.object;
            hit_time = s->GetTime();
            attached = true;
            if (loop)
            {
                s->KillSound(loop);
                loop = 0;
            }
            if (alternate)
                if (auto* p = s->IsObjectAlive(m_hFiredFrom)
                                  ? dynamic_cast<CPlayerObj*>(s->HandleToObject(m_hFiredFrom))
                                  : nullptr)
                    if (!p->SetOrbObject(nullptr))
                    {
                        Fail("Orb listener release failed");
                        return;
                    }
        }
        else
            m_bExplode = true;
    }
    if (hit.object && s->CreateInterObjectLink(m_hObject, hit.object) != DE_OK)
        Fail("Orb hit link rejected");
}
void COrbProjectile::AddExplosion(DVector p, DVector n)
{
    auto* s = g_pServerDE;
    effect = {};
    effect.source = s->GetObjectName(m_hObject);
    effect.type = 21;
    effect.position = p;
    effect.normal = n;
    effect.fragment_event = s->PublishExplosionFragments(m_hObject, 21, p, n, effect.fragment_error);
    SoundRequest request;
    effect.sound = request.resource = "Sounds\\Weapons\\orb\\explode.wav";
    request.inner_radius = 75;
    request.outer_radius = 750;
    request.priority = 1;
    effect.voice = s->PlayDetachedSound(m_hObject, request, p, effect.sound_error);
    s->RecordSceneExplosion(effect);
    if (!effect.fragment_error.empty() || !effect.sound_error.empty())
    {
        error = effect.fragment_error + effect.sound_error;
        Fail(error.c_str());
    }
}
