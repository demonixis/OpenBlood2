// Original ObjectDLL/Projectile.cpp and GameProjectiles.cpp, selected energy path including the Zealot divine impact.
#include "Projectile.h"
#include "AI_Mgr.h"
#include "FlareProjectile.h"
#include "Grenade.h"
#include "ProximityBomb.h"
#include "RemoteBomb.h"
#include "SurfaceTypes.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include "radius_damage.h"
#include <cmath>
using namespace OpenBlood2;
CProjectile::CProjectile(DBYTE type) : BaseClass(type)
{
    AddAggregate(&m_damage);
    if (g_pServerDE)
        scale_start = g_pServerDE->GetTime();
}
bool CProjectile::Fail(const char* text)
{
    error = text;
    failed = true;
    Finish("failed");
    return false;
}
void CProjectile::Finish(const char* outcome)
{
    DVector position;
    g_pServerDE->GetObjectPos(m_hObject, &position);
    ProjectileReport report;
    report.source = g_pServerDE->GetObjectName(m_hObject);
    report.outcome = outcome;
    report.error = error;
    report.position = position;
    report.time = g_pServerDE->GetTime();
    report.delivered = targets.delivered;
    report.cancelled = targets.cancelled;
    report.candidates = targets.candidates;
    report.in_radius = targets.in_radius;
    if (const auto* wave = dynamic_cast<const CBehemothShockwaveProjectile*>(this))
    {
        report.shockwave = true;
        report.query_from = wave->ground_from;
        report.query_to = wave->ground_to;
        report.query = wave->ground_result;
        report.blast_position = wave->blast_position;
        report.blast_normal = wave->blast_normal;
    }
    if (const auto* remote = dynamic_cast<const CRemoteBomb*>(this))
    {
        report.remote = true;
        report.armed = remote->armed;
        report.falling = remote->falling;
        report.contacts = remote->contacts;
    }
    if (const auto* bomb = dynamic_cast<const CProximityBomb*>(this))
    {
        report.proximity = m_nWeaponType == WEAP_PROXIMITYBOMB;
        report.time_bomb = m_nWeaponType == WEAP_TIMEBOMB;
        report.armed = bomb->armed;
        report.hit_character = bomb->hit_character;
        report.contacts = bomb->contacts;
        report.scans = bomb->scans;
        report.audio_error = bomb->sound_error;
    }
    if (const auto* grenade = dynamic_cast<const CGrenade*>(this))
    {
        report.grenade = true;
        report.bounces_left = grenade->bounces_left;
        report.contacts = grenade->contacts;
        report.explosions = grenade->explosion_passes;
        report.delivered = grenade->damage_deliveries;
    }
    if (const auto* flare = dynamic_cast<const CFlareProjectile*>(this))
    {
        report.flare = true;
        report.tracked = flare->tracking;
        report.burn_ticks = flare->ticks;
    }
    if (const auto* flare = dynamic_cast<const CFlareAltProjectile*>(this))
    {
        report.flare = true;
        report.burst_children = flare->children;
    }
    g_pServerDE->RecordProjectile(report);
    g_pServerDE->RemoveObject(m_hObject); // Destruction stays deferred through all callbacks.
}
DDWORD CProjectile::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        // Runtime creation or a saved-game restore; authored projectiles are not claimed.
        if (!data || (reason != PRECREATE_NORMAL && reason != PRECREATE_SAVEGAME))
            return 0;
        const auto result = BaseClass::EngineMessageFn(message, data, reason);
        if (reason == PRECREATE_SAVEGAME)
            return result; // Original skips PostPropRead: the engine record carries file and update.
        auto& state = *static_cast<ObjectCreateStruct*>(data);
        legacy_copy(state.m_Filename, filename ? filename : "");
        legacy_copy(state.m_SkinName, skin ? skin : "");
        state.m_NextUpdate = .001f;
        return result;
    }
    if (message == MID_INITIALUPDATE)
    {
        if (reason != INITIALUPDATE_SAVEGAME && !InitialUpdate())
            Fail("Projectile initialization failed");
    }
    else if (message == MID_SAVEOBJECT)
    {
        if (data)
            SaveObject(*static_cast<HMESSAGEWRITE*>(data));
    }
    else if (message == MID_LOADOBJECT)
    {
        if (data)
            LoadObject(*static_cast<HMESSAGEREAD*>(data));
    }
    else if (message == MID_UPDATE && g_pServerDE->IsObjectAlive(m_hObject) && !Update() && !failed)
        Finish(exploded ? "exploded" : "expired");
    else if (message == MID_LINKBROKEN)
    {
        if (m_hHitObject == data)
            m_hHitObject = nullptr;
        else if (m_hFiredFrom == data)
            m_hFiredFrom = nullptr; // Original else-if order.
    }
    else if (message == MID_TOUCHNOTIFY && data)
    {
        SegmentHit hit;
        hit.object = static_cast<HOBJECT>(data);
        g_pServerDE->GetObjectPos(m_hObject, &hit.point);
        HandleContact(hit);
    }
    return BaseClass::EngineMessageFn(message, data, reason);
}
bool CProjectile::InitialUpdate()
{
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
    m_fStartTime = g_pServerDE->GetTime();
    DVector dimensions{1, 1, 1};
    if (g_pServerDE->SetObjectDims(m_hObject, &dimensions) != DE_OK ||
        !g_pServerDE->ScaleObject(m_hObject, {1, 1, 1}) || !m_damage.Init(m_hObject))
        return false;
    m_damage.SetHitPoints(5);
    m_damage.apply_damage_physics = false;
    g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | 8u | (1u << 5));
    return true;
}
bool CProjectile::Setup(DVector direction, DBYTE type, float damage, float speed, int radius, HOBJECT owner)
{
    if (configured || !g_pServerDE->IsObjectAlive(m_hObject) || !g_pServerDE->IsObjectAlive(owner) ||
        !physics_vector(direction) || std::abs(vector_dot(direction, direction) - 1) > .001f ||
        !std::isfinite(damage) || damage < 0 || damage > 1e6f || !std::isfinite(speed) || speed < 0 || speed > 1e6f ||
        radius < 0 || radius > 1000000)
        return Fail("Invalid projectile setup");
    if (auto* energy = dynamic_cast<CEnergyBlastProjectile*>(this))
        if (auto* ai = dynamic_cast<AI_Mgr*>(g_pServerDE->HandleToObject(owner));
            ai && ai->npc_type == AI_Mgr::NPCType::Zealot)
            energy->divine = ai->npc_divine;
    m_vDir = direction;
    m_fDamage = damage;
    m_fVelocity = speed;
    m_nRadius = radius;
    m_hFiredFrom = owner;
    m_nWeaponType = type;
    if (g_pServerDE->CreateInterObjectLink(m_hObject, owner) != DE_OK)
        return Fail("Projectile shooter link failed");
    DRotation rotation;
    if (g_pServerDE->AlignRotation(rotation, direction) != DE_OK)
        return Fail("Projectile rotation failed");
    auto velocity = vector_scale(direction, speed);
    if (g_pServerDE->SetObjectRotation(m_hObject, &rotation) != DE_OK ||
        g_pServerDE->SetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("Projectile transform/velocity failed");
    g_pServerDE->SetObjectFlags(m_hObject, flags); // Concrete classes retain their original gravity policy.
    configured = true;
    return true;
}
bool CProjectile::Update()
{
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Projectile never received Setup");
    if (m_bExplode || m_damage.IsDead())
    {
        Explode();
        return false;
    }
    return m_fLifeTime <= 0 || g_pServerDE->GetTime() <= m_fStartTime + m_fLifeTime;
}
void CProjectile::HandleContact(const SegmentHit& hit)
{
    if (!configured || m_bExplode || failed || !g_pServerDE->IsObjectAlive(m_hObject))
        return;
    if (hit.object)
    {
        if (hit.object == m_hFiredFrom || !g_pServerDE->IsObjectAlive(hit.object) ||
            !(g_pServerDE->GetObjectFlags(hit.object) & FLAG_SOLID) ||
            g_pServerDE->GetObjectClass(hit.object) == g_pServerDE->GetObjectClass(m_hObject))
            return;
    }
    else if (hit.polygon == UINT32_MAX)
        return; // No invented world HOBJECT.
    contact = hit;
    contacted = true;
    if (!hit.object && hit.texture_flags == SURFTYPE_SKY)
    {
        Finish("sky");
        return;
    }
    m_bExplode = true;
    m_hHitObject = hit.object;
    if (m_hHitObject && g_pServerDE->CreateInterObjectLink(m_hObject, m_hHitObject) != DE_OK)
        Fail("Projectile hit link failed");
}
bool CProjectile::AdvanceFlight(double seconds)
{
    if (seconds == 0 || m_bExplode || failed || !configured)
        return !failed;
    if (!std::isfinite(seconds) || seconds < 0 || seconds > .5)
        return Fail("Projectile flight exceeds 0.5-second catch-up bound");
    if (g_pServerDE->GetObjectFlags(m_hObject) & FLAG_GRAVITY)
        return Fail("Energy projectile gravity is unsupported");
    DVector from, velocity, acceleration;
    if (g_pServerDE->GetObjectPos(m_hObject, &from) != DE_OK ||
        g_pServerDE->GetVelocity(m_hObject, &velocity) != DE_OK ||
        g_pServerDE->GetAcceleration(m_hObject, &acceleration) != DE_OK || vector_dot(acceleration, acceleration) != 0)
        return Fail("Projectile motion state unavailable");
    ++flight_steps;
    if (vector_dot(velocity, velocity) < 1e-12f)
        return true;
    const auto to = vector_add(from, vector_scale(velocity, float(seconds)));
    SegmentHit hit;
    const auto result = g_pServerDE->IntersectSegment(from, to, m_hObject, hit, false, SegmentObjects::SolidOnly,
                                                      SegmentFilter::ProjectileContact);
    if (result != SegmentResult::Hit && result != SegmentResult::Miss)
        return Fail("Projectile point sweep unavailable/invalid");
    auto destination = result == SegmentResult::Hit ? hit.point : to;
    if (g_pServerDE->TeleportObject(m_hObject, &destination) != DE_OK)
        return Fail("Projectile movement rejected");
    if (result == SegmentResult::Hit)
    {
        // Replacement point collision: one travelled segment, no speculative contacts,
        // no sliding. Source explosion remains deferred to the next original update.
        const auto into = vector_dot(velocity, hit.normal);
        if (into < 0)
            velocity = vector_subtract(velocity, vector_scale(hit.normal, into));
        if (g_pServerDE->SetVelocity(m_hObject, &velocity) != DE_OK)
            return Fail("Projectile contact velocity rejected");
        HandleContact(hit);
    }
    return !failed;
}
void CProjectile::Explode(bool repeat)
{
    if ((exploded && !repeat) || exploding_ || failed || !g_pServerDE->IsObjectAlive(m_hObject))
        return;
    exploding_ = true;
    struct Reset
    {
        bool& value;
        ~Reset()
        {
            value = false;
        }
    } reset{exploding_};
    exploded = true;
    DVector position;
    g_pServerDE->GetObjectPos(m_hObject, &position);
    if (!AddImpact(position))
        return;
    if (m_nRadius)
        targets = DamageObjectsInRadius(m_hFiredFrom, this, position, float(m_nRadius), m_fDamage, damage_type);
    else if (m_hHitObject && g_pServerDE->IsObjectAlive(m_hHitObject))
    {
        const auto message = g_pServerDE->StartMessageToObject(this, m_hHitObject, damage_message);
        if (message)
        {
            const auto written =
                g_pServerDE->WriteDamage(message, {m_vDir, position, m_fDamage, damage_type, m_hFiredFrom});
            const auto delivered = g_pServerDE->EndMessage(message);
            targets.delivered = written == DE_OK && delivered == DE_OK;
            targets.cancelled = !targets.delivered;
        }
        else
            targets.cancelled = true;
    }
    if (auto* grenade = dynamic_cast<CGrenade*>(this))
        grenade->damage_deliveries += targets.delivered;
    if (targets.cancelled)
        error = "Projectile damage traversal interrupted or unavailable";
    if (m_hHitObject)
    {
        g_pServerDE->BreakInterObjectLink(m_hObject, m_hHitObject);
        m_hHitObject = nullptr;
    }
}
bool CProjectile::AddImpact(DVector position)
{
    // AddImpact mutates the stored source direction: normalized *2, then *10.
    m_vDir = vector_scale(m_vDir, 1 / std::sqrt(vector_dot(m_vDir, m_vDir)));
    const auto from = vector_subtract(position, vector_scale(m_vDir, 2));
    m_vDir = vector_scale(m_vDir, 20);
    SegmentHit impact;
    auto normal = vector_scale(m_vDir, -1), point = position;
    if (m_hHitObject || (!dynamic_cast<CGrenade*>(this) && !dynamic_cast<CProximityBomb*>(this) &&
                         !dynamic_cast<CRemoteBomb*>(this) && !contact.object && contact.polygon != UINT32_MAX))
    {
        const auto query = g_pServerDE->IntersectSegment(from, vector_add(position, m_vDir), nullptr, impact);
        if (query != SegmentResult::Hit && query != SegmentResult::Miss)
        {
            return Fail("Projectile impact normal query unavailable");
        }
        if (query == SegmentResult::Hit)
        {
            normal = impact.normal;
            point = impact.point;
        }
    }
    AddExplosion(point, normal);
    return !failed;
}

bool CEnergyBlastProjectile::InitialUpdate()
{
    // Source CreateObject delivers INITIALUPDATE before weapon Setup; the default
    // non-Zealot branch selects FIRETRAIL_2, half lifetime, no light.
    trail_error = "OBJFX_FIRETRAIL_2 (scale 0.5, SCALELIFETIME) unavailable";
    return CProjectile::InitialUpdate();
}
bool CEnergyBlastProjectile::Update()
{
    const float scale = g_pServerDE->GetTime() - scale_start < .01f ? 0 : .25f;
    if (!g_pServerDE->ScaleObject(m_hObject, {scale, scale, scale}))
        return Fail("Energy projectile scale rejected");
    return CProjectile::Update();
}
void CEnergyBlastProjectile::AddExplosion(DVector position, DVector normal)
{
    SceneExplosionReport report;
    report.source = g_pServerDE->GetObjectName(m_hObject);
    report.position = position;
    report.normal = normal;
    report.type = divine ? 12 : 7; // EXP_FLAME_SMALL for a divine Zealot.
    report.model_event =
        g_pServerDE->PublishExplosionModels(m_hObject, report.type, position, normal, report.model_error);
    report.sprite_event =
        g_pServerDE->PublishExplosionSprites(m_hObject, report.type, position, normal, report.sprite_error);
    report.ring_event = g_pServerDE->PublishExplosionRings(m_hObject, report.type, position, normal, report.ring_error);
    report.fragment_event =
        g_pServerDE->PublishExplosionFragments(m_hObject, report.type, position, normal, report.fragment_error);
    report.flame_event =
        g_pServerDE->PublishExplosionFlames(m_hObject, report.type, position, normal, report.flame_error);
    report.light_event =
        g_pServerDE->PublishExplosionLights(m_hObject, report.type, position, normal, report.light_error);
    SoundRequest request;
    report.sound = request.resource = "Sounds\\Weapons\\Tesla\\impact.wav";
    request.inner_radius = 75;
    request.outer_radius = 750;
    request.priority = 1;
    report.voice = g_pServerDE->PlayDetachedSound(m_hObject, request, position, report.sound_error);
    g_pServerDE->RecordSceneExplosion(report);
}
