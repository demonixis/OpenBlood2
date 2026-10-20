#include "LeechProjectile.h"
#include "engine/collision.h"
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
namespace
{
    bool turn(DRotation& rotation, DVector axis, float angle)
    {
        const float length = std::sqrt(vector_dot(axis, axis));
        if (!std::isfinite(length) || length < 1e-6f)
            return false;
        DRotation delta;
        delta.m_Vec = vector_scale(axis, std::sin(angle / 2) / length);
        delta.m_Spin = std::cos(angle / 2);
        rotation = compose_rotation(delta, rotation);
        return true;
    }
    void publish(HOBJECT owner, SceneExplosionReport& report, bool alt, DVector position, DVector normal)
    {
        auto* s = g_pServerDE;
        report = {};
        report.source = s->GetObjectName(owner);
        report.type = alt ? 17 : 16;
        report.position = position;
        report.normal = normal;
        if (alt)
        {
            report.model_event = s->PublishExplosionModels(owner, report.type, position, normal, report.model_error);
            report.ring_event = s->PublishExplosionRings(owner, report.type, position, normal, report.ring_error);
        }
        report.sprite_event = s->PublishExplosionSprites(owner, report.type, position, normal, report.sprite_error);
        report.light_event = s->PublishExplosionLights(owner, report.type, position, normal, report.light_error);
        s->RecordSceneExplosion(report); // Neither source explosion requests an impact sound.
    }
} // namespace
CLeechPrimeProjectile::CLeechPrimeProjectile()
{
    filename = "Sprites\\lleechproj.spr";
    skin = nullptr;
    damage_type = DamageType::Leech;
    g_pServerDE->RandomFloat(.25f, 1, color.y);
    g_pServerDE->RandomFloat(.25f, 2, trail_scale);
}
CLeechPrimeProjectile::~CLeechPrimeProjectile()
{
    if (flight_sound && g_pServerDE)
        g_pServerDE->KillSound(flight_sound);
}
bool CLeechPrimeProjectile::InitialUpdate()
{
    DRotation rotation;
    float roll = 0;
    int sign = 0;
    if (g_pServerDE->GetObjectRotation(m_hObject, &rotation) != DE_OK ||
        !g_pServerDE->RandomFloat(0, std::numbers::pi_v<float>, roll) ||
        !turn(rotation, rotate_vector(rotation, {0, 0, 1}), roll) ||
        g_pServerDE->SetObjectRotation(m_hObject, &rotation) != DE_OK || !g_pServerDE->RandomFloat(.05f, .1f, curve) ||
        !g_pServerDE->RandomInt(0, 1, sign) || !g_pServerDE->RandomFloat(5, 20, amplitude) ||
        !CProjectile::InitialUpdate())
        return false;
    curve *= std::numbers::pi_v<float> * (sign ? 1.f : -1.f);
    // Source Setup subsequently replaces the initial roll with its aligned firing rotation.
    if (!g_pServerDE->ScaleObject(m_hObject, {}))
        return false;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 3, trail_error, trail_scale);
    return trail && trail_error.empty();
}
bool CLeechPrimeProjectile::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Leech projectile never received Setup");
    if (!flight_sound)
    {
        SoundRequest sound;
        sound.resource = "Sounds\\Weapons\\lifeleech\\projectile.wav";
        sound.inner_radius = 75;
        sound.outer_radius = 750;
        sound.priority = 1;
        sound.space = SoundSpace::Spatial;
        sound.speaker = m_hObject;
        sound.loop = true;
        flight_sound = s->PlayObjectLoopSound(m_hObject, sound, sound_error);
        if (!flight_sound)
            return Fail(sound_error.c_str());
    }
    if (m_bExplode || m_damage.IsDead())
    {
        s->KillSound(flight_sound);
        flight_sound = 0;
        Explode();
        return false;
    }
    const float scale = s->GetTime() - m_fStartTime < .01f ? 0 : .2f;
    DRotation rotation;
    DVector velocity;
    if (!s->ScaleObject(m_hObject, {scale, scale, scale}) || !s->SetObjectColor(m_hObject, color, 1) ||
        s->GetObjectRotation(m_hObject, &rotation) != DE_OK || s->GetVelocity(m_hObject, &velocity) != DE_OK ||
        !turn(rotation, rotate_vector(rotation, {0, 0, 1}), curve) ||
        s->SetObjectRotation(m_hObject, &rotation) != DE_OK)
        return Fail("Leech curved projectile pose unavailable");
    velocity = vector_add(velocity, vector_scale(rotate_vector(rotation, {0, 1, 0}), amplitude));
    if (s->SetVelocity(m_hObject, &velocity) != DE_OK || s->GetObjectPos(m_hObject, &light_position) != DE_OK)
        return Fail("Leech velocity/light position unavailable");
    return true; // Original overrides base lifetime: no ten-second expiry.
}
void CLeechPrimeProjectile::AddExplosion(DVector at, DVector normal)
{
    publish(m_hObject, effect, false, at, normal);
    if (!effect.sprite_error.empty() || !effect.light_error.empty())
    {
        error = effect.sprite_error + effect.light_error;
        Fail(error.c_str());
    }
}
CLeechAltProjectile::CLeechAltProjectile() : CProjectile(OT_NORMAL)
{
    filename = skin = nullptr;
    damage_type = DamageType::Explode;
}
bool CLeechAltProjectile::Update()
{
    auto* s = g_pServerDE;
    if (!configured)
        return Fail("Leech alternate never received Setup");
    if (!s->IsObjectAlive(m_hFiredFrom))
        return false;
    DVector center;
    if (s->GetObjectPos(m_hFiredFrom, &center) != DE_OK)
        return Fail("Leech alternate owner pose unavailable");
    AddExplosion(center, {0, 1, 0});
    if (failed)
        return false;
    std::vector<HOBJECT> candidates;
    for (DLink* link = CDestructable::m_DestructableHead.m_pNext; link && link != &CDestructable::m_DestructableHead;
         link = link->m_pNext)
    {
        auto* damage = static_cast<CDestructable*>(link->m_pData);
        if (!damage || !damage->m_hObject)
            break;
        candidates.push_back(damage->m_hObject);
    }
    // Source reads uninitialized vUp for its first rotation. Define that one
    // axis as world up; subsequent axes retain the source's previous gravity value.
    DVector up{0, 1, 0};
    for (const auto target : candidates)
    {
        if (!s->IsObjectAlive(m_hObject) || !s->IsObjectAlive(m_hFiredFrom))
            return false;
        if (!s->IsObjectAlive(target) || target == m_hFiredFrom || s->GetObjectType(target) != OT_MODEL ||
            !(s->GetObjectUserFlags(target) & (1u << 5)))
            continue;
        DVector position, velocity;
        DRotation rotation;
        if (s->GetObjectPos(target, &position) != DE_OK)
            return Fail("Leech target position unavailable");
        const auto delta = vector_subtract(position, center);
        const float distance = std::sqrt(vector_dot(delta, delta));
        if (distance > 450)
            continue;
        float angle = 0;
        if (s->GetObjectRotation(target, &rotation) != DE_OK || s->GetVelocity(target, &velocity) != DE_OK ||
            !s->RandomFloat(-std::numbers::pi_v<float> * .2f, std::numbers::pi_v<float> * .2f, angle))
            return Fail("Leech target state unavailable");
        // A zero global force leaves the rotation unchanged; no NaN quaternion.
        if (vector_dot(up, up) > 1e-12f)
            turn(rotation, up, angle);
        if (s->SetObjectRotation(target, &rotation) != DE_OK || s->GetGlobalForce(&up) != DE_OK)
            return Fail("Leech target rotation/gravity unavailable");
        up = vector_scale(up, -.25f);
        const auto radial = distance > 1e-6f ? vector_scale(delta, (450 - distance) * 7.5f / distance) : DVector{};
        velocity = vector_add(velocity, vector_add(up, radial));
        if (s->SetVelocity(target, &velocity) != DE_OK)
            return Fail("Leech target push rejected");
        ++pushes;
        const auto message = s->StartMessageToObject(this, target, damage_message);
        if (!message)
            return Fail("Leech alternate damage message unavailable");
        const auto written = s->WriteDamage(message, {velocity, position, m_fDamage, damage_type, m_hFiredFrom});
        const auto delivered = s->EndMessage(message);
        if (written != DE_OK || delivered != DE_OK)
            return Fail("Leech alternate damage delivery failed");
        ++targets.delivered;
    }
    return false;
}
void CLeechAltProjectile::AddExplosion(DVector at, DVector normal)
{
    publish(m_hObject, effect, true, at, normal);
    if (!effect.model_error.empty() || !effect.sprite_error.empty() || !effect.ring_error.empty() ||
        !effect.light_error.empty())
    {
        error = effect.model_error + effect.sprite_error + effect.ring_error + effect.light_error;
        Fail(error.c_str());
    }
}
