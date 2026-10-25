// Selected original ObjectDLL/GameProjectiles.cpp: CFlareProjectile, Alt and Burst.
#include "FlareProjectile.h"
#include "Corpse.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
namespace
{
    bool choose_color(DVector& color)
    {
        int index = 0;
        if (!g_pServerDE->RandomInt(0, 3, index))
            return false;
        constexpr DVector colors[]{{1, 0, 0}, {1, .25f, 0}, {1, 1, 0}, {1, 1, 1}};
        color = colors[index];
        return true;
    }
    void explosion(HOBJECT owner, DDWORD type, DVector position, DVector normal, float radius)
    {
        SceneExplosionReport r;
        r.source = g_pServerDE->GetObjectName(owner);
        r.position = position;
        r.normal = normal;
        r.type = type;
        r.sprite_event = g_pServerDE->PublishExplosionSprites(owner, type, position, normal, r.sprite_error);
        if (type == 13)
            r.ring_event = g_pServerDE->PublishExplosionRings(owner, type, position, normal, r.ring_error);
        r.light_event = g_pServerDE->PublishExplosionLights(owner, type, position, normal, r.light_error);
        SoundRequest request;
        r.sound = request.resource = "Sounds\\Weapons\\flare\\impact.wav";
        request.inner_radius = radius / 10;
        request.outer_radius = radius;
        request.priority = 1;
        r.voice = g_pServerDE->PlayDetachedSound(owner, request, position, r.sound_error);
        g_pServerDE->RecordSceneExplosion(r);
    }
} // namespace
CFlareProjectile::CFlareProjectile()
{
    filename = "Sprites\\lensflare32.spr";
    skin = nullptr;
    damage_type = DamageType::Fire;
    color_ready = choose_color(color);
    track_start = g_pServerDE->GetTime();
    light_error = "Owner light (radius 200) illumination unavailable";
}
CFlareProjectile::~CFlareProjectile()
{
    StopSound();
}
void CFlareProjectile::StopSound()
{
    if (sound)
        g_pServerDE->KillSound(sound);
    sound = 0;
}
bool CFlareProjectile::InitialUpdate()
{
    if (!color_ready || !CProjectile::InitialUpdate() || !g_pServerDE->ScaleObject(m_hObject, {}))
        return false;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 6, trail_error);
    return true;
}
bool CFlareProjectile::AdvanceFlight(double seconds)
{
    // Source tracking writes position and zero velocity in Update. Never perform
    // a second travel segment between the contact and that update.
    return tracking ? !failed : CProjectile::AdvanceFlight(seconds);
}
void CFlareProjectile::HandleContact(const SegmentHit& hit)
{
    if (!configured || contacted || failed || !g_pServerDE->IsObjectAlive(m_hObject))
        return;
    if (hit.object)
    {
        if (hit.object == m_hFiredFrom || !g_pServerDE->IsObjectAlive(hit.object) ||
            !(g_pServerDE->GetObjectFlags(hit.object) & FLAG_SOLID) ||
            g_pServerDE->GetObjectClass(hit.object) == g_pServerDE->GetObjectClass(m_hObject))
            return;
    }
    else if (hit.polygon == UINT32_MAX)
        return;
    StopSound();
    contact = hit;
    contacted = true;
    m_hHitObject = hit.object;
    auto* object = g_pServerDE->HandleToObject(hit.object);
    if (dynamic_cast<CBaseCharacter*>(object) || dynamic_cast<CCorpse*>(object))
    {
        DVector target, position;
        if (g_pServerDE->GetObjectPos(hit.object, &target) != DE_OK ||
            g_pServerDE->GetObjectPos(m_hObject, &position) != DE_OK)
        {
            Fail("Flare contact pose unavailable");
            return;
        }
        offset = vector_subtract(position, target);
        tracking = true;
        m_fLifeTime = 5;
        track_start = g_pServerDE->GetTime();
    }
    else
        m_bExplode = true; // Unlike base HandleTouch, primary flares have no SKY exclusion.
    g_pServerDE->SetObjectFlags(m_hObject, g_pServerDE->GetObjectFlags(m_hObject) & ~FLAG_TOUCH_NOTIFY);
    if (m_hHitObject && g_pServerDE->CreateInterObjectLink(m_hObject, m_hHitObject) != DE_OK)
        Fail("Flare target link failed");
}
bool CFlareProjectile::Update()
{
    if (!configured)
        return Fail("Flare never received Setup");
    if (!sound)
    {
        SoundRequest request;
        request.resource = "Sounds\\Weapons\\flare\\projectile.wav";
        request.speaker = m_hObject;
        request.loop = true;
        request.reverb_requested = false;
        request.inner_radius = 75;
        request.outer_radius = 750;
        request.priority = 1;
        sound = g_pServerDE->PlayObjectLoopSound(m_hObject, request, sound_error);
    }
    const auto now = g_pServerDE->GetTime();
    const float scale = now - track_start < .01f ? 0 : .25f;
    if (!g_pServerDE->ScaleObject(m_hObject, {scale, scale, scale}) ||
        !g_pServerDE->SetObjectColor(m_hObject, color, 1))
        return Fail("Flare presentation state rejected");
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
    if (m_bExplode || m_damage.IsDead())
    {
        StopSound();
        if (tracking)
        {
            DVector position;
            if (g_pServerDE->GetObjectPos(m_hObject, &position) != DE_OK)
                return Fail("Tracking flare position unavailable");
            AddExplosion(position, {0, 1, 0}); // Fizzle only: no second damage/explosion.
            exploded = true;
        }
        else
            Explode();
        return false;
    }
    if (m_fLifeTime > 0 && now > track_start + m_fLifeTime)
    {
        StopSound();
        return false;
    }
    if (!m_hHitObject)
    {
        if (tracking)
            StopSound();
        return !tracking; // Target unlink removes without fizzle.
    }
    if (auto* character = dynamic_cast<CBaseCharacter*>(g_pServerDE->HandleToObject(m_hHitObject));
        character && character->m_damage.IsDead())
        m_bExplode = true; // Source still follows/ticks this frame; fizzles on its next update.
    if (tracking)
    {
        DVector position, zero{};
        if (g_pServerDE->GetObjectPos(m_hHitObject, &position) != DE_OK)
            return Fail("Tracked flare target unavailable");
        position = vector_add(position, offset); // World offset, deliberately no target rotation.
        if (g_pServerDE->SetVelocity(m_hObject, &zero) != DE_OK ||
            g_pServerDE->TeleportObject(m_hObject, &position) != DE_OK)
            return Fail("Tracked flare movement rejected");
        if (now > last_damage_time + 1)
        {
            last_damage_time = now;
            const auto message = g_pServerDE->StartMessageToObject(this, m_hHitObject, damage_message);
            if (!message)
                return Fail("Flare damage message unavailable");
            const auto written =
                g_pServerDE->WriteDamage(message, {zero, position, m_fDamage, DamageType::Fire, m_hFiredFrom});
            const auto delivered = g_pServerDE->EndMessage(message);
            if (written != DE_OK || delivered != DE_OK)
                return Fail("Flare damage delivery failed");
            ++ticks;
            ++targets.delivered;
            // Deferred destruction keeps this callback alive; no endpoint access after delivery.
        }
    }
    return true;
}
void CFlareProjectile::AddExplosion(DVector position, DVector normal)
{
    explosion(m_hObject, 13, position, normal, 750);
}
CFlareAltProjectile::CFlareAltProjectile()
{
    filename = "Sprites\\lensflare32.spr";
    skin = nullptr;
    color_ready = choose_color(color);
    trail_error = "Owner light (radius 200) unavailable; source alternate trail is disabled";
}
bool CFlareAltProjectile::InitialUpdate()
{
    return color_ready && CProjectile::InitialUpdate() && g_pServerDE->ScaleObject(m_hObject, {});
}
bool CFlareAltProjectile::Update()
{
    if (!configured)
        return Fail("Alternate flare never received Setup");
    if (get_velocity)
    {
        if (g_pServerDE->GetVelocity(m_hObject, &cached_velocity) != DE_OK)
            return Fail("Flare burst velocity unavailable");
        get_velocity = false;
    }
    const auto elapsed = g_pServerDE->GetTime() - scale_start; // Source shadow m_fStartTime.
    if (elapsed > .1f)
    {
        const auto speed = std::sqrt(vector_dot(cached_velocity, cached_velocity));
        DRotation rotation;
        DVector position;
        if (!std::isfinite(speed) || speed < 1e-6f || g_pServerDE->AlignRotation(rotation, cached_velocity) != DE_OK ||
            g_pServerDE->GetObjectPos(m_hObject, &position) != DE_OK || !g_pServerDE->IsObjectAlive(m_hFiredFrom))
            return Fail("Flare burst requires cached velocity and a live shooter");
        const auto right = rotate_vector(rotation, {1, 0, 0});
        const auto up = rotate_vector(rotation, {0, 1, 0});
        const auto forward = vector_scale(cached_velocity, 1 / speed);
        constexpr float offsets[8][2]{{.135f, 0},    {-.135f, 0},   {0, -.135f},    {0, .135f},
                                      {.06f, -.06f}, {-.06f, .06f}, {-.06f, -.06f}, {.06f, .06f}};
        for (const auto& offset : offsets)
        {
            auto dir = vector_add(forward, vector_add(vector_scale(right, offset[0]), vector_scale(up, offset[1])));
            dir = vector_scale(dir, 1 / std::sqrt(vector_dot(dir, dir)));
            ObjectCreateStruct state;
            state.m_Pos = position;
            SAFE_STRCPY(state.m_Name, "Flare burst fragment");
            auto* child = dynamic_cast<CFlareBurstProjectile*>(
                g_pServerDE->CreateObject(g_pServerDE->GetClass("CFlareBurstProjectile"), &state));
            if (!child || !child->Setup(dir, m_nWeaponType, m_fDamage * .5f, speed, m_nRadius, m_hFiredFrom) ||
                !child->SetColor(color))
            {
                if (child)
                    g_pServerDE->RemoveObject(child->m_hObject);
                return Fail("Flare fragment creation/setup failed (earlier children remain independent)");
            }
            ++children;
        }
        if (!AddImpact(position)) // Source emits FRAG before changing to BURST.
            return false;
        explosion_type = 14;
        CProjectile::Update(); // A pending contact/death may now explode with BURST.
        return false;
    }
    const float scale = elapsed < .01f ? 0 : .5f;
    if (!g_pServerDE->ScaleObject(m_hObject, {scale, scale, scale}) ||
        !g_pServerDE->SetObjectColor(m_hObject, color, 1))
        return Fail("Alternate flare presentation state rejected");
    return CProjectile::Update();
}
void CFlareAltProjectile::AddExplosion(DVector position, DVector normal)
{
    explosion(m_hObject, explosion_type, position, normal, 500);
}
CFlareBurstProjectile::CFlareBurstProjectile()
{
    filename = "Sprites\\lensflare32.spr";
    skin = nullptr;
    m_damage.m_bGodMode = true;
    light_error = "Owner light (radius 75) illumination unavailable";
}
bool CFlareBurstProjectile::InitialUpdate()
{
    if (!CProjectile::InitialUpdate() || !g_pServerDE->ScaleObject(m_hObject, {}))
        return false;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 8, trail_error);
    return true;
}
bool CFlareBurstProjectile::SetColor(DVector value)
{
    if (!physics_vector(value) || value.x < 0 || value.y < 0 || value.z < 0 || value.x > 1 || value.y > 1 ||
        value.z > 1)
        return Fail("Invalid flare fragment color");
    color = value;
    color_ready = true;
    return true;
}
bool CFlareBurstProjectile::Update()
{
    if (!color_ready || !g_pServerDE->ScaleObject(m_hObject, {.25f, .25f, .25f}) ||
        !g_pServerDE->SetObjectColor(m_hObject, color, 1))
        return Fail("Flare fragment color/presentation unavailable");
    return CProjectile::Update();
}
void CFlareBurstProjectile::AddExplosion(DVector position, DVector normal)
{
    explosion(m_hObject, 15, position, normal, 750);
}
