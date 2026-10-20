// Selected original GameProjectiles.cpp and NagaCeilingDebris.cpp.
#include "CampaignProjectiles.h"
#include "AI_Mgr.h"
#include "LaserBeamFX.h"
#include "SurfaceTypes.h"
#include "engine/collision.h"
#include "radius_damage.h"
#include <algorithm>
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
void PublishCampaignExplosion(HOBJECT source, DVector p, DVector n, unsigned type, const char* sound, float radius)
{
    auto* s = g_pServerDE;
    SceneExplosionReport r;
    r.source = s->GetObjectName(source);
    r.position = p;
    r.normal = n;
    r.type = type;
    r.model_event = s->PublishExplosionModels(source, type, p, n, r.model_error);
    r.sprite_event = s->PublishExplosionSprites(source, type, p, n, r.sprite_error);
    r.ring_event = s->PublishExplosionRings(source, type, p, n, r.ring_error);
    r.fragment_event = s->PublishExplosionFragments(source, type, p, n, r.fragment_error);
    r.flame_event = s->PublishExplosionFlames(source, type, p, n, r.flame_error);
    r.light_event = s->PublishExplosionLights(source, type, p, n, r.light_error);
    if (sound)
    {
        SoundRequest request;
        r.sound = request.resource = sound;
        request.inner_radius = radius * .1f;
        request.outer_radius = radius;
        request.priority = 1;
        r.voice = s->PlayDetachedSound(source, request, p, r.sound_error);
    }
    s->RecordSceneExplosion(r);
}
bool PublishDivineShockball(HOBJECT source, DVector p, DVector n, SceneExplosionReport& r, const char* sound)
{
    auto* s = g_pServerDE;
    r = {};
    r.source = s->GetObjectName(source);
    r.position = p;
    r.normal = n;
    r.type = 22;
    r.model_event = s->PublishExplosionModels(source, 22, p, n, r.model_error);
    r.ring_event = s->PublishExplosionRings(source, 22, p, n, r.ring_error);
    r.light_event = s->PublishExplosionLights(source, 22, p, n, r.light_error);
    if (sound)
    {
        SoundRequest request;
        r.sound = request.resource = sound;
        request.inner_radius = 200;
        request.outer_radius = 2000;
        request.priority = 1;
        r.voice = s->PlayDetachedSound(source, request, p, r.sound_error);
    }
    s->RecordSceneExplosion(r);
    return r.model_event && r.ring_event && r.light_event && r.model_error.empty() && r.ring_error.empty() &&
           r.light_error.empty() && r.sound_error.empty();
}
CTimeBomb::CTimeBomb()
{
    skin = "Skins\\Ammo\\c4_time.dtx";
}
bool CTimeBomb::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured || s->GetObjectPos(m_hObject, &last_position) != DE_OK)
        return Fail("Time bomb setup/position unavailable");
    if (m_bExplode || m_damage.IsDead())
    {
        Explode();
        return false;
    }
    if (armed)
    {
        DVector zero{};
        if (s->SetVelocity(m_hObject, &zero) != DE_OK)
            return Fail("Time bomb velocity unavailable");
        if (s->GetTime() > m_fStartTime + 5)
            m_bExplode = true; // Explodes on the next original update.
    }
    else
    {
        pitch += pitch_velocity * s->GetFrameTime();
        yaw += yaw_velocity * s->GetFrameTime();
        auto rotation = rotation_from_euler({pitch, yaw, 0});
        if (s->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return Fail("Time bomb rotation rejected");
    }
    return true;
}
void CTimeBomb::AddExplosion(DVector p, DVector n)
{
    PublishCampaignExplosion(m_hObject, p, n, 1, "Sounds\\Weapons\\time\\explode.wav");
}
CNagaSpikeProjectile::CNagaSpikeProjectile() : CProjectile(OT_MODEL)
{
    flags |= FLAG_SHADOW | FLAG_MODELGOURAUDSHADE | FLAG_RAYHIT;
    filename = "Models\\Ammo\\Nagaspike.abc";
    skin = "Skins\\Powerups\\Nagaspike_pu.dtx";
    damage_type = DamageType::Normal;
}
void CNagaSpikeProjectile::AddExplosion(DVector p, DVector)
{
    PublishCampaignExplosion(m_hObject, p, {0, 1, 0}, 29, "Sounds\\Weapons\\Tesla\\altimpact.wav", 2000);
}
void CNagaDebrisProjectile::AddExplosion(DVector p, DVector n)
{
    PublishCampaignExplosion(m_hObject, p, n, 31, "Sounds\\Weapons\\time\\explode.wav", 2000);
    trail_error = "Naga debris WFX_SCREENSHAKE unavailable";
}
bool CNagaDebrisProjectile::Update()
{
    auto* s = g_pServerDE;
    if (!configured || !s->IsObjectAlive(m_hFiredFrom))
        return Fail("Naga debris shooter unavailable");
    int count;
    if (!s->RandomInt(3, 5, count))
        return Fail("Naga debris count RNG unavailable");
    auto* ai = dynamic_cast<AI_Mgr*>(s->HandleToObject(m_hFiredFrom));
    DVector target;
    if (s->GetObjectPos(ai ? ai->GetTarget() : m_hObject, &target) != DE_OK)
        return Fail("Naga debris target unavailable");
    DVector hand;
    DRotation rotation;
    for (auto node : {"l_hand", "r_hand"})
        if (s->GetModelNodeTransform(m_hFiredFrom, node, &hand, &rotation))
            AddExplosion(hand, {0, 1, 0});
    for (int i = 0; i < count; ++i)
    {
        auto p = target;
        float offset;
        int sign;
        if (!s->RandomFloat(.5f, 150, offset) || !s->RandomInt(0, 1, sign))
            return Fail("Naga debris offset RNG unavailable");
        p.x += sign ? offset : -offset;
        if (!s->RandomFloat(.5f, 150, offset) || !s->RandomInt(0, 1, sign))
            return Fail("Naga debris offset RNG unavailable");
        p.z += sign ? offset : -offset;
        auto ceiling = vector_add(p, {0, 1000, 0});
        SegmentHit hit;
        auto query = s->IntersectSegment(p, ceiling, nullptr, hit, false, SegmentObjects::SolidOnly,
                                         SegmentFilter::None, SegmentStart::SkipInterior);
        if (query == SegmentResult::Hit)
            ceiling = hit.point;
        else if (query != SegmentResult::Miss)
            return Fail("Naga ceiling ray unavailable");
        ObjectCreateStruct state;
        state.m_Pos = ceiling;
        state.m_Pos.y -= 50;
        auto* child = s->CreateObject(s->GetClass("NagaCeilingDebris"), &state);
        if (!child || !s->IsObjectAlive(child->m_hObject))
            return Fail("Naga ceiling debris creation failed");
        DVector angles;
        constexpr float pi = std::numbers::pi_v<float>;
        if (!s->RandomFloat(-pi, pi, angles.x) || !s->RandomFloat(-pi, pi, angles.y) ||
            !s->RandomFloat(-pi, pi, angles.z))
            return Fail("Naga debris rotation RNG unavailable");
        const auto pose = rotation_from_euler(angles);
        s->SetObjectRotation(child->m_hObject, &pose);
        ++children;
    }
    return false;
}
DDWORD NagaCeilingDebris::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    const auto result = BaseClass::EngineMessageFn(message, data, reason);
    auto fail = [&](const char* why)
    {
        error = why;
        s->RemoveObject(m_hObject);
        return DDWORD{0};
    };
    if (message == MID_PRECREATE)
    {
        if (!data || reason != 0)
            return 0;
        int variant;
        if (!s->RandomInt(6, 9, variant))
            return 0;
        auto& state = *static_cast<ObjectCreateStruct*>(data);
        const auto name = "gib" + std::to_string(variant);
        legacy_copy(state.m_Filename, ("models\\gibs\\stone\\" + name + ".abc").c_str());
        legacy_copy(state.m_SkinName, ("skins\\gibs\\stone\\" + name + ".dtx").c_str());
        state.m_ObjectType = OT_MODEL;
        state.m_Flags = FLAG_VISIBLE | FLAG_GRAVITY | FLAG_SOLID | FLAG_RAYHIT;
    }
    if (message == MID_INITIALUPDATE)
    {
        DVector scale;
        constexpr float limit = std::numbers::pi_v<float> / 20;
        if (!s->RandomFloat(4, 6, scale.x) || !s->RandomFloat(4, 6, scale.y) || !s->RandomFloat(4, 6, scale.z) ||
            !s->ScaleObject(m_hObject, scale) || !s->SetBlockingPriority(m_hObject, 100) ||
            !s->ConfigureCharacterBody(m_hObject, 1000, 0))
            return fail("Naga rock body unavailable");
        if (!s->RandomFloat(-limit, limit, angular_step.x) || !s->RandomFloat(-limit, limit, angular_step.y) ||
            !s->RandomFloat(-limit, limit, angular_step.z))
            return fail("Naga rock spin RNG unavailable");
        start_time = s->GetTime();
        s->SetNextUpdate(m_hObject, .01f);
    }
    if (message == MID_UPDATE)
    {
        // Preserve the source's reversed expiry subtraction (no ordinary ten-second expiry).
        if (start_time - s->GetTime() > 10)
        {
            s->RemoveObject(m_hObject);
            return result;
        }
        s->SetNextUpdate(m_hObject, .01f);
        DRotation rotation;
        if (s->GetObjectRotation(m_hObject, &rotation) != DE_OK)
            return fail("Naga rock pose unavailable");
        rotation = multiply_rotation(rotation, rotation_from_euler(angular_step));
        s->SetObjectRotation(m_hObject, &rotation);
        MovementState support;
        if (!s->GetMovementState(m_hObject, support))
            return fail("Naga rock support unavailable");
        if (support.standing)
        {
            DVector position;
            s->GetObjectPos(m_hObject, &position);
            PublishCampaignExplosion(m_hObject, position, {0, 1, 0}, 30, "Sounds\\Gibs\\Stone\\gib_impact4.wav", 750);
            DamageObjectsInRadius(m_hObject, this, position, 175, 50, DamageType::Normal);
            s->RemoveObject(m_hObject);
        }
    }
    return result;
}
CGroundStrikeProjectile::CGroundStrikeProjectile() : CProjectile(OT_NORMAL)
{
    filename = "Sprites\\napalmprime.spr";
    skin = nullptr;
    damage_type = DamageType::Normal;
    trail_error = "OBJFX_GROUNDFLAME_2 (0.5 SCALELIFETIME) and AddLight unavailable";
}
bool CGroundStrikeProjectile::Update()
{
    const float scale = g_pServerDE->GetTime() - m_fStartTime < .01f ? 0 : .25f;
    if (!g_pServerDE->ScaleObject(m_hObject, {scale, scale, scale}))
        return Fail("Ground strike scale unavailable");
    return CProjectile::Update();
}
void CShockwaveProjectile::AddExplosion(DVector p, DVector n)
{
    auto* ai = dynamic_cast<AI_Mgr*>(g_pServerDE->HandleToObject(m_hFiredFrom));
    if (ai && ai->npc_type == AI_Mgr::NPCType::Zealot && !ai->npc_divine)
        PublishCampaignExplosion(m_hObject, p, n, 19);
    else
    {
        SceneExplosionReport effect;
        if (!PublishDivineShockball(m_hObject, p, n, effect))
            Fail("Divine shockwave presentation failed");
    }
}
bool CShockwaveProjectile::Update()
{
    auto* s = g_pServerDE;
    if (!configured || !s->IsObjectAlive(m_hFiredFrom))
        return Fail("Zealot shockwave shooter unavailable");
    if (exploded)
        return true;
    exploded = true;
    DVector p, normal{0, -1000, 0};
    if (s->GetObjectPos(m_hFiredFrom, &p) != DE_OK)
        return Fail("Zealot shockwave position unavailable");
    SegmentHit hit;
    auto query = s->IntersectSegment(p, vector_add(p, normal), nullptr, hit, false, SegmentObjects::SolidOnly,
                                     SegmentFilter::None, SegmentStart::SkipInterior);
    if (query == SegmentResult::Hit)
    {
        p = hit.point;
        normal = hit.normal;
    }
    else if (query != SegmentResult::Miss)
        return Fail("Zealot shockwave ground ray unavailable");
    AddExplosion(p, normal);
    if (failed)
        return false;
    std::vector<HOBJECT> snapshot;
    auto& head = CDestructable::m_DestructableHead;
    for (auto* link = head.m_pNext; link && link != &head; link = link->m_pNext)
    {
        auto h = static_cast<CDestructable*>(link->m_pData)->m_hObject;
        if (!h)
            break;
        snapshot.push_back(h);
    }
    targets.candidates = static_cast<unsigned>(snapshot.size());
    for (auto h : snapshot)
    {
        if (!s->IsObjectAlive(m_hObject) || !s->IsObjectAlive(m_hFiredFrom))
        {
            targets.cancelled = true;
            break;
        }
        if (h == m_hFiredFrom || !s->IsObjectAlive(h) || !(s->GetObjectUserFlags(h) & (1u << 5)) ||
            s->GetObjectType(h) != OT_MODEL)
            continue;
        DVector position, velocity, gravity;
        s->GetObjectPos(h, &position);
        auto delta = vector_subtract(position, p);
        float distance = std::sqrt(vector_dot(delta, delta));
        if (distance > 450)
            continue;
        ++targets.in_radius;
        s->GetVelocity(h, &velocity);
        s->GetGlobalForce(&gravity);
        if (distance > 1e-6f)
            velocity = vector_add(velocity, vector_scale(delta, (450 - distance) * 7.5f / distance));
        velocity = vector_add(velocity, vector_scale(gravity, -.25f));
        // Source SetVelocity is commented out: impulse is only the damage direction.
        auto message = s->StartMessageToObject(this, h, damage_message);
        if (!message)
        {
            targets.cancelled = true;
            break;
        }
        auto written = s->WriteDamage(message, {velocity, position, m_fDamage, DamageType::Explode, m_hFiredFrom});
        auto sent = s->EndMessage(message);
        if (written != DE_OK || sent != DE_OK)
        {
            targets.cancelled = true;
            break;
        }
        ++targets.delivered;
    }
    if (targets.cancelled)
        error = "Zealot shockwave damage interrupted";
    return false;
}
void CDeathRayProjectile::AddExplosion(DVector p, DVector n)
{
    PublishCampaignExplosion(m_hObject, p, n, 25, "Sounds\\Weapons\\laser\\impact.wav", 750);
}
bool CDeathRayProjectile::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("DeathRay not configured");
    if (s->GetTime() - last_bounce <= .1f)
        return true;
    DVector from;
    if (s->GetObjectPos(m_hObject, &from) != DE_OK)
        return Fail("DeathRay position unavailable");
    const auto to = vector_add(from, vector_scale(m_vDir, 3000));
    SegmentHit hit;
    auto query = s->IntersectSegment(from, to, first ? m_hFiredFrom : nullptr, hit, false, SegmentObjects::SolidOnly,
                                     SegmentFilter::IgnoreVolumes, SegmentStart::SkipInterior);
    first = false;
    if (query != SegmentResult::Hit && query != SegmentResult::Miss)
        return Fail("DeathRay ray unavailable");
    s->PublishLaserBeam(m_hObject, from, query == SegmentResult::Hit ? hit.point : to, beam_error, LASER_GREEN_SMALL);
    if (query == SegmentResult::Miss)
        return false;
    AddExplosion(hit.point, hit.normal);
    const bool character = dynamic_cast<CBaseCharacter*>(s->HandleToObject(hit.object)) != nullptr;
    if (hit.object && s->IsObjectAlive(hit.object))
    {
        auto message = s->StartMessageToObject(this, hit.object, damage_message);
        if (!message)
            return Fail("DeathRay damage message unavailable");
        auto written = s->WriteDamage(
            message, {vector_scale(m_vDir, 3000), hit.point, m_fDamage, DamageType::Electric, m_hFiredFrom});
        auto sent = s->EndMessage(message);
        if (written != DE_OK || sent != DE_OK)
            return Fail("DeathRay damage delivery failed");
        ++targets.delivered;
    }
    if (character || hit.texture_flags == SURFTYPE_SKY || !s->IsObjectAlive(m_hObject))
        return false;
    const auto delta = vector_subtract(hit.point, from);
    bounce_distance += std::sqrt(vector_dot(delta, delta));
    if (bounce_distance > 1500)
        return false;
    // Bound zero-distance reflection cycles instead of leaving an immortal beam.
    if (++reflections > 64)
        return Fail("DeathRay reflection bound exceeded");
    m_vDir = vector_subtract(m_vDir, vector_scale(hit.normal, 2 * vector_dot(m_vDir, hit.normal)));
    auto position = hit.point;
    if (s->TeleportObject(m_hObject, &position) != DE_OK)
        return Fail("DeathRay reflection position rejected");
    last_bounce = s->GetTime();
    return true;
}
namespace
{
    bool fragment(CProjectile& parent, const char* name, DVector position, DVector direction, float speed, float damage,
                  int radius)
    {
        auto* s = g_pServerDE;
        const auto length = std::sqrt(vector_dot(direction, direction));
        if (length < 1e-6f || !s->IsObjectAlive(parent.m_hFiredFrom))
            return false;
        ObjectCreateStruct state;
        state.m_Pos = position;
        auto* child = dynamic_cast<CProjectile*>(s->CreateObject(s->GetClass(name), &state));
        return child && child->Setup(vector_scale(direction, 1 / length), parent.m_nWeaponType, damage, speed, radius,
                                     parent.m_hFiredFrom);
    }
    DVector radial(DVector normal, float angle)
    {
        DRotation rotation;
        g_pServerDE->AlignRotation(rotation, normal);
        return rotate_vector(rotation, {std::cos(angle), std::sin(angle), 0});
    }
} // namespace
void CHowitzerAltShell::AddExplosion(DVector p, DVector n)
{
    PublishCampaignExplosion(m_hObject, p, n, 10, "Sounds\\Weapons\\c4\\explosion_1.wav");
    float angle;
    constexpr float pi = std::numbers::pi_v<float>;
    auto* s = g_pServerDE;
    if (!s->RandomFloat(-pi, pi, angle))
    {
        error = "Howitzer fragment angle RNG unavailable";
        return;
    }
    // Source redraws its loop bound each iteration, and divides damage twice.
    for (int i = 0; i < 10; ++i)
    {
        int count;
        float spread;
        if (!s->RandomInt(5, 10, count))
        {
            error = "Howitzer fragment count RNG unavailable";
            return;
        }
        if (i >= count)
            break;
        angle += pi / 2;
        if (!s->RandomFloat(.25f, 4, spread))
        {
            error = "Howitzer fragment spread RNG unavailable";
            return;
        }
        auto direction = vector_add(n, vector_scale(radial(n, angle), spread));
        // Source passes a length-30 direction and speed 30 to Setup: actual speed 900.
        if (!fragment(*this, "CHowitzerAltFrag", p, direction, 900, m_fDamage / 4, m_nRadius / 2))
        {
            error = "Howitzer fragment creation failed";
            return;
        }
    }
}
CHowitzerAltFrag::CHowitzerAltFrag()
{
    filename = "Sprites\\howitzerball.spr";
    skin = nullptr;
    m_damage.m_bGodMode = true;
    trail_error = "Howitzer AddLight and OBJFX_SMOKETRAIL_1 unavailable";
}
bool CHowitzerAltFrag::InitialUpdate()
{
    return CProjectile::InitialUpdate() && g_pServerDE->ScaleObject(m_hObject, {.15f, .15f, 0});
}
bool CHowitzerAltFrag::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Howitzer fragment not configured");
    if (m_bExplode || m_damage.IsDead())
    {
        Explode();
        return false;
    }
    DVector velocity, gravity;
    if (s->GetVelocity(m_hObject, &velocity) != DE_OK || s->GetGlobalForce(&gravity) != DE_OK)
        return Fail("Howitzer fragment gravity unavailable");
    velocity = vector_add(velocity, vector_scale(gravity, .02f));
    return s->SetVelocity(m_hObject, &velocity) == DE_OK || Fail("Howitzer fragment velocity rejected");
}
void CHowitzerAltFrag::AddExplosion(DVector p, DVector n)
{
    PublishCampaignExplosion(m_hObject, p, contacted ? contact.normal : n, 11);
}
void CNapalmAltProjectile::AddExplosion(DVector p, DVector n)
{
    auto* s = g_pServerDE;
    if (flight_sound)
    {
        s->KillSound(flight_sound);
        flight_sound = 0;
    }
    PublishCampaignExplosion(m_hObject, p, n, 5, "Sounds\\Weapons\\napalm\\altimpact.wav", 2000);
    constexpr float pi = std::numbers::pi_v<float>;
    DVector incoming;
    if (s->GetVelocity(m_hObject, &incoming) != DE_OK)
    {
        error = "Napalm impact velocity unavailable";
        return;
    }
    // Source AlignRotation(normal, incoming velocity) orients the radial fan around the impact.
    auto cross = [](DVector a, DVector b)
    { return DVector{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; };
    auto right = cross(incoming, n);
    const float length = std::sqrt(vector_dot(right, right));
    right = length > 1e-6f ? vector_scale(right, 1 / length) : radial(n, 0);
    const auto up = cross(n, right);
    for (unsigned i = 0; i < 8; ++i)
    {
        float angle, spread;
        if (!s->RandomFloat(pi * .15f, pi * .85f, angle) || !s->RandomFloat(35 * .75f, 35 * 3, spread))
        {
            error = "Napalm fragment RNG unavailable";
            return;
        }
        const auto direction = vector_add(
            vector_scale(n, 35),
            vector_scale(vector_add(vector_scale(right, std::cos(angle)), vector_scale(up, std::sin(angle))), spread));
        // Original keeps the parent's radius, and passes a length-35 direction twice.
        if (!fragment(*this, "CNapalmFireball", p, direction, 1225, m_fDamage / 3, m_nRadius))
        {
            error = "Napalm fireball creation failed";
            return;
        }
    }
}
CNapalmFireball::CNapalmFireball()
{
    filename = "Sprites\\napalmprime.spr";
    skin = nullptr;
    flags |= FLAG_GRAVITY;
    m_damage.m_bGodMode = true;
    g_pServerDE->RandomFloat(1.5f, 2.5f, m_fLifeTime);
    trail_error = "Napalm fireball AddLight unavailable";
}
bool CNapalmFireball::InitialUpdate()
{
    return CProjectile::InitialUpdate() && g_pServerDE->ScaleObject(m_hObject, {});
}
bool CNapalmFireball::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Napalm fireball not configured");
    s->ScaleObject(m_hObject, {.25f, .25f, .25f});
    if (bounces >= 3)
        return false;
    if (m_bExplode)
    {
        Explode(true);
        m_bExplode = false;
    }
    return true; // Source ignores its randomized lifetime.
}
void CNapalmFireball::HandleContact(const SegmentHit& hit)
{
    if (hit.object && (!(g_pServerDE->GetObjectFlags(hit.object) & FLAG_SOLID) ||
                       g_pServerDE->GetObjectClass(hit.object) == g_pServerDE->GetObjectClass(m_hObject)))
        return;
    contact = hit;
    contacted = true;
    m_hHitObject = hit.object;
    m_bExplode = true;
    ++bounces;
    DVector velocity;
    g_pServerDE->GetVelocity(m_hObject, &velocity);
    velocity = vector_scale(velocity, .35f);
    g_pServerDE->SetVelocity(m_hObject, &velocity);
}
bool CNapalmFireball::AdvanceFlight(double seconds)
{
    if (!configured || m_bExplode || failed || seconds == 0)
        return !failed;
    if (!std::isfinite(seconds) || seconds < 0 || seconds > .5)
        return Fail("Napalm fireball flight bound exceeded");
    auto* s = g_pServerDE;
    const auto steps = std::max(1u, static_cast<unsigned>(std::ceil(seconds * 120)));
    const auto dt = float(seconds / steps);
    for (unsigned i = 0; i < steps && !m_bExplode; ++i)
    {
        DVector p, v, g;
        if (s->GetObjectPos(m_hObject, &p) != DE_OK || s->GetVelocity(m_hObject, &v) != DE_OK ||
            s->GetGlobalForce(&g) != DE_OK)
            return Fail("Napalm fireball motion unavailable");
        auto to = vector_add(p, vector_add(vector_scale(v, dt), vector_scale(g, .5f * dt * dt)));
        v = vector_add(v, vector_scale(g, dt));
        SegmentHit hit;
        auto query = s->IntersectSegment(p, to, m_hObject, hit, false, SegmentObjects::SolidOnly,
                                         SegmentFilter::ProjectileContact);
        if (query != SegmentResult::Hit && query != SegmentResult::Miss)
            return Fail("Napalm fireball sweep unavailable");
        if (query == SegmentResult::Hit)
        {
            to = vector_add(hit.point, vector_scale(hit.normal, .001f));
            auto into = vector_dot(v, hit.normal);
            if (into < 0)
                v = vector_subtract(v, vector_scale(hit.normal, into));
        }
        if (s->TeleportObject(m_hObject, &to) != DE_OK || s->SetVelocity(m_hObject, &v) != DE_OK)
            return Fail("Napalm fireball motion rejected");
        ++flight_steps;
        if (query == SegmentResult::Hit)
            HandleContact(hit);
    }
    return !failed;
}
void CNapalmFireball::AddExplosion(DVector p, DVector n)
{
    PublishCampaignExplosion(m_hObject, p, n, 6);
}
// Original skull: seek the nearest moving character head within 200 units.
CSkullProjectile::CSkullProjectile() : CProjectile(OT_MODEL)
{
    filename = "Models\\Powerups\\skull.abc";
    skin = "Skins\\Powerups\\skull.dtx";
    damage_type = DamageType::Normal;
}
CSkullProjectile::~CSkullProjectile()
{
    if (loop && g_pServerDE)
        g_pServerDE->KillSound(loop);
}
bool CSkullProjectile::InitialUpdate()
{
    if (!CProjectile::InitialUpdate() || !g_pServerDE->ScaleObject(m_hObject, {.5f, .5f, .5f}))
        return false;
    g_pServerDE->PublishObjectParticles(m_hObject, 3, trail_error);
    return true;
}
bool CSkullProjectile::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Skull setup unavailable");
    if (!loop)
    {
        SoundRequest request;
        request.resource = "Sounds\\Enemies\\deathshroud\\flyskull.wav";
        request.inner_radius = 75;
        request.outer_radius = 750;
        request.speaker = m_hObject;
        request.priority = 1;
        request.reverb_requested = false;
        request.loop = true;
        loop = s->PlayObjectLoopSound(m_hObject, request, sound_error);
        DVector color;
        float alpha;
        if (!s->GetObjectColor(m_hObject, color, alpha) || !s->SetObjectColor(m_hObject, color, .6f))
            return Fail("Skull color unavailable");
    }
    if (m_bExplode || m_damage.IsDead())
    {
        if (loop)
        {
            s->KillSound(loop);
            loop = 0;
        }
        Explode();
        return false;
    }
    DVector position, target{}, velocity;
    DRotation rotation;
    if (s->GetObjectPos(m_hObject, &position) != DE_OK || s->GetObjectRotation(m_hObject, &rotation) != DE_OK ||
        s->GetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("Skull transform unavailable");
    tracked = nullptr;
    float nearest = 200 * 200;
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
        if (s->GetVelocity(h, &v) != DE_OK || (v.x == 0 && v.y == 0 && v.z == 0) ||
            !s->GetModelNodeTransform(h, "head", &p, &r))
            continue;
        const auto delta = vector_subtract(p, position);
        const float d = vector_dot(delta, delta);
        if (d <= nearest)
        {
            tracked = h;
            target = p;
            nearest = d;
        }
    }
    if (tracked && nearest > 1e-12f)
    {
        DRotation desired, blended;
        if (s->AlignRotation(desired, vector_subtract(target, position)) != DE_OK ||
            s->InterpolateRotation(blended, rotation, desired, 1 - std::sqrt(nearest) / 200) != DE_OK ||
            s->SetObjectRotation(m_hObject, &blended) != DE_OK)
            return Fail("Skull steering unavailable");
        rotation = blended;
    }
    velocity = vector_scale(rotate_vector(rotation, {0, 0, 1}), std::sqrt(vector_dot(velocity, velocity)));
    if (s->SetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("Skull velocity unavailable");
    return true; // Source override has no base lifetime expiration.
}
void CSkullProjectile::HandleContact(const SegmentHit& hit)
{
    auto* s = g_pServerDE;
    if (!configured || m_hHitObject || m_bExplode || failed || !s->IsObjectAlive(m_hObject) ||
        hit.object == m_hFiredFrom)
        return;
    if (hit.object && (!s->IsObjectAlive(hit.object) || !(s->GetObjectFlags(hit.object) & FLAG_SOLID)))
        return;
    if (!hit.object && hit.polygon == UINT32_MAX)
        return;
    contacted = true;
    contact = hit;
    if (hit.object && dynamic_cast<CBaseCharacter*>(s->HandleToObject(hit.object)))
    {
        DVector zero{}, p;
        DRotation r;
        s->SetVelocity(m_hObject, &zero);
        m_hHitObject = hit.object;
        if (s->GetModelNodeTransform(hit.object, "head", &p, &r))
        {
            const auto forward = vector_scale(rotate_vector(r, {0, 0, 1}), 7.5f);
            p = vector_add(p, forward);
            s->TeleportObject(m_hObject, &p);
            DRotation facing;
            if (s->AlignRotation(facing, vector_scale(forward, -1)) == DE_OK)
                s->SetObjectRotation(m_hObject, &facing);
            const auto message = s->StartMessageToObject(this, hit.object, damage_message);
            if (!message)
            {
                Fail("Skull contact damage allocation failed");
                return;
            }
            const auto written = s->WriteDamage(message, {forward, p, m_fDamage, damage_type, m_hFiredFrom});
            const auto delivered = s->EndMessage(message);
            if (written != DE_OK || delivered != DE_OK)
            {
                Fail("Skull contact damage failed");
                return;
            }
            ++contact_damage;
        }
    }
    m_bExplode = true;
    if (hit.object && s->IsObjectAlive(hit.object) && s->CreateInterObjectLink(m_hObject, hit.object) != DE_OK)
        Fail("Skull contact link failed");
}
void CSkullProjectile::AddExplosion(DVector p, DVector n)
{
    PublishCampaignExplosion(m_hObject, p, n, 14, "Sounds\\Weapons\\howitzer\\impact.wav", 750);
}
