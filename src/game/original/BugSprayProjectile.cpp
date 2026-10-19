// Original GameProjectiles.cpp BugSpray primary/alternate, using bounded point flight.
#include "BugSprayProjectile.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
CBugSprayProjectile::CBugSprayProjectile(bool alternate) : alternate_(alternate)
{
    filename = "Sprites\\napalmprime.spr";
    skin = nullptr;
    flags |= FLAG_GRAVITY;
    damage_type = alternate ? DamageType::Fire : DamageType::Explode;
}
bool CBugSprayProjectile::InitialUpdate()
{
    if (!CProjectile::InitialUpdate() || !g_pServerDE->ScaleObject(m_hObject, {}))
        return false;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, alternate_ ? 32 : 31, trail_error);
    return trail && trail_error.empty();
}
bool CBugSprayProjectile::AdvanceFlight(double seconds)
{
    if (seconds == 0 || m_bExplode || failed || !configured)
        return !failed;
    if (!std::isfinite(seconds) || seconds < 0 || seconds > .5)
        return Fail("BugSpray flight exceeds bounded catch-up");
    DVector gravity, pos, velocity;
    if (g_pServerDE->GetGlobalForce(&gravity) != DE_OK || g_pServerDE->GetObjectPos(m_hObject, &pos) != DE_OK ||
        g_pServerDE->GetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("BugSpray motion unavailable");
    const auto steps = static_cast<unsigned>(std::max(1., std::ceil(seconds * 120)));
    const float dt = float(seconds / steps);
    for (unsigned i = 0; i < steps && !m_bExplode && g_pServerDE->IsObjectAlive(m_hObject); ++i)
    {
        const auto to = vector_add(pos, vector_add(vector_scale(velocity, dt), vector_scale(gravity, .5f * dt * dt)));
        velocity = vector_add(velocity, vector_scale(gravity, dt));
        SegmentHit hit;
        const auto result = g_pServerDE->IntersectSegment(pos, to, m_hObject, hit, false, SegmentObjects::SolidOnly,
                                                          SegmentFilter::ProjectileContact);
        if (result != SegmentResult::Hit && result != SegmentResult::Miss)
            return Fail("BugSpray point sweep unavailable");
        pos = result == SegmentResult::Hit ? hit.point : to;
        if (g_pServerDE->TeleportObject(m_hObject, &pos) != DE_OK ||
            g_pServerDE->SetVelocity(m_hObject, &velocity) != DE_OK)
            return Fail("BugSpray flight commit rejected");
        ++flight_steps;
        if (result == SegmentResult::Hit)
            HandleContact(hit);
    }
    return !failed;
}
void CBugSprayProjectile::AddExplosion(DVector position, DVector normal)
{
    auto* s = g_pServerDE;
    auto& r = effect;
    r = {};
    r.source = s->GetObjectName(m_hObject);
    r.position = position;
    r.normal = normal;
    r.type = alternate_ ? 27 : 26;
    // These recipes contain only smoke (primary), or two rings and a light (alternate).
    if (alternate_)
    {
        r.ring_event = s->PublishExplosionRings(m_hObject, r.type, position, normal, r.ring_error);
        r.light_event = s->PublishExplosionLights(m_hObject, r.type, position, normal, r.light_error);
    }
    else
        r.flame_event = s->PublishExplosionFlames(m_hObject, r.type, position, normal, r.flame_error);
    SoundRequest request;
    r.sound = request.resource =
        alternate_ ? "Sounds\\Weapons\\Bugbuster\\altimpact.wav" : "Sounds\\Weapons\\Bugbuster\\impact.wav";
    request.outer_radius = 750;
    request.inner_radius = 75;
    request.priority = 1;
    r.voice = s->PlayDetachedSound(m_hObject, request, position, r.sound_error);
    s->RecordSceneExplosion(r);
}
