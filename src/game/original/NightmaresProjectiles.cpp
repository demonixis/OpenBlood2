// Selected add-on GameProjectiles.cpp gas grenade and Flayer paths.
#include "NightmaresProjectiles.h"
#include "CampaignProjectiles.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <algorithm>
#include <cmath>

using namespace OpenBlood2;
namespace
{
    constexpr float gas_radius = 200, gas_duration = 10, gas_interval = .5f;
    constexpr float trap_duration = 8, trap_interval = 1, retract_duration = 1.5f;
    constexpr float portal_radius = 800, portal_duration = 20, portal_interval = 2;

    SoundId loop_sound(HOBJECT owner, const char* resource, float radius, unsigned priority, std::string& error)
    {
        SoundRequest request;
        request.resource = resource;
        request.outer_radius = radius;
        request.inner_radius = radius * .1f;
        request.priority = priority;
        request.space = SoundSpace::Spatial;
        request.speaker = owner;
        request.loop = true;
        return g_pServerDE->PlayObjectLoopSound(owner, request, error);
    }

    void stop_sound(SoundId& voice)
    {
        if (voice && g_pServerDE)
            g_pServerDE->KillSound(voice);
        voice = 0;
    }

    std::vector<HOBJECT> destructables()
    {
        std::vector<HOBJECT> result;
        result.reserve(CDestructable::m_dwNumDestructables);
        const auto& head = CDestructable::m_DestructableHead;
        for (const auto* link = head.m_pNext; link && link != &head; link = link->m_pNext)
            result.push_back(static_cast<const CDestructable*>(link->m_pData)->m_hObject);
        return result;
    }

    bool deliver_damage(BaseClass* projectile, HOBJECT target, const Damage& damage)
    {
        auto* s = g_pServerDE;
        const auto message = s->StartMessageToObject(projectile, target, damage_message);
        if (!message)
            return false;
        const auto written = s->WriteDamage(message, damage);
        const auto delivered = s->EndMessage(message);
        return written == DE_OK && delivered == DE_OK;
    }
} // namespace

bool CGasGrenade::InitialUpdate()
{
    if (!IsAddonBuild() || !CProjectile::InitialUpdate())
        return false;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 40, trail_error);
    return true;
}
void CGasGrenade::HandleContact(const SegmentHit& hit)
{
    if (configured && !failed && !smoking && !hit.object && hit.polygon != UINT32_MAX &&
        hit.texture_flags == SURFTYPE_SKY)
    {
        Finish("sky");
        return;
    }
    CGrenade::HandleContact(hit);
}
bool CGasGrenade::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Gas grenade never received Setup");
    const float now = s->GetTime();
    if (smoking)
    {
        if (now > smoke_start + gas_duration)
            return false;
        if (now > last_gas_damage + gas_interval)
        {
            last_gas_damage = now;
            ++gas_ticks;
            return DamageAndWonky();
        }
        return true;
    }
    if (!sound)
        sound = loop_sound(m_hObject, "Sounds\\Weapons\\assault\\projectile.wav", 600, 1, sound_error);
    if (m_bExplode || m_damage.IsDead() || now > m_fStartTime + m_fLifeTime)
    {
        StopSound();
        m_bExplode = smoking = true;
        smoke_start = now;
        Explode();
        if (!s->IsObjectAlive(m_hObject))
            return false;
        s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~(FLAG_GRAVITY | FLAG_VISIBLE));
        DVector velocity{};
        if (s->SetVelocity(m_hObject, &velocity) != DE_OK)
            return Fail("Gas grenade stop velocity rejected");
    }
    if (s->GetObjectPos(m_hObject, &last_position) != DE_OK)
        return Fail("Gas grenade position unavailable");
    return UpdateRotation();
}
void CGasGrenade::AddExplosion(DVector position, DVector)
{
    ++explosion_passes;
    PublishCampaignExplosion(m_hObject, position, {0, 1, 0}, 32, "Sounds\\Weapons\\c4\\explosion_1.wav");
}
bool CGasGrenade::DamageAndWonky()
{
    auto* s = g_pServerDE;
    DVector origin;
    if (s->GetObjectPos(m_hObject, &origin) != DE_OK)
        return Fail("Gas cloud position unavailable");
    for (const auto handle : destructables())
    {
        if (!s->IsObjectAlive(m_hObject))
            return false;
        if (!s->IsObjectAlive(handle) || !dynamic_cast<CBaseCharacter*>(s->HandleToObject(handle)))
            continue;
        DVector position;
        if (s->GetObjectPos(handle, &position) != DE_OK)
            continue;
        const auto direction = vector_subtract(position, origin);
        const float distance = std::sqrt(vector_dot(direction, direction));
        if (distance > gas_radius)
            continue;
        if (!deliver_damage(this, handle, {direction, position, 3 * (1 - distance / gas_radius),
                                           DamageType::Normal, m_hFiredFrom}))
            return Fail("Gas cloud damage delivery failed");
        ++gas_deliveries;
        if (s->IsObjectAlive(handle))
            if (auto* player = dynamic_cast<CPlayerObj*>(s->HandleToObject(handle)); player && player->GetClient())
                player->SendWonkyVision(15, false);
    }
    return true;
}

CFlayerChain::CFlayerChain() : CProjectile(OT_MODEL)
{
    filename = "Models_ao\\Ammo_ao\\chainhook.abc";
    skin = "Skins_ao\\Ammo_ao\\chainhook.dtx";
    damage_type = DamageType::Normal;
    m_damage.m_bGodMode = true;
    m_fLifeTime = 6.5f; // Source custom Update intentionally does not check this clock.
}
CFlayerChain::~CFlayerChain()
{
    auto* s = g_pServerDE;
    if (!s)
        return;
    stop_sound(voice);
    FreeTarget(false);
    if (gib && s->IsObjectAlive(gib))
        s->RemoveObject(gib);
}
bool CFlayerChain::InitialUpdate()
{
    return IsAddonBuild() && CProjectile::InitialUpdate() && g_pServerDE->ScaleObject(m_hObject, {3, 3, 3});
}
DDWORD CFlayerChain::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_LINKBROKEN && data == target)
        target = nullptr;
    return CProjectile::EngineMessageFn(message, data, reason);
}
bool CFlayerChain::SetTarget(HOBJECT handle)
{
    if (handle == target)
        return true;
    auto* s = g_pServerDE;
    if (!s->IsObjectAlive(handle) || !dynamic_cast<CBaseCharacter*>(s->HandleToObject(handle)) ||
        s->CreateInterObjectLink(m_hObject, handle) != DE_OK)
        return false;
    if (target)
        s->BreakInterObjectLink(m_hObject, target);
    target = handle;
    return true;
}
bool CFlayerChain::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .001f);
    DVector position;
    if (!configured || s->GetObjectPos(m_hObject, &position) != DE_OK)
        return Fail("Flayer chain setup or position unavailable");
    if (first_update)
    {
        first_update = false;
        DRotation rotation;
        if (s->GetObjectRotation(m_hObject, &rotation) != DE_OK)
            return Fail("Flayer initial aim unavailable");
        SegmentHit hit;
        const auto to = vector_add(position, vector_scale(rotate_vector(rotation, {0, 0, 1}), 3000));
        const auto result = s->IntersectSegment(position, to, m_hFiredFrom, hit, false, SegmentObjects::SolidOnly,
                                                SegmentFilter::IgnoreVolumes, SegmentStart::SkipInterior);
        if (result == SegmentResult::Hit && hit.object && dynamic_cast<CBaseCharacter*>(s->HandleToObject(hit.object)) &&
            !SetTarget(hit.object))
            return Fail("Flayer initial target link failed");
        PublishCampaignExplosion(m_hObject, position, rotate_vector(rotation, {0, 1, 0}), 33);
    }
    if (!voice && state == State::Tracking)
        voice = loop_sound(m_hObject, "Sounds_ao\\Weapons\\Flayer\\chainloop.wav", 750, 1, sound_error);
    if (m_bExplode || m_damage.IsDead())
    {
        stop_sound(voice);
        Explode();
        return false;
    }
    const auto now = s->GetTime();
    if (state == State::Attached)
    {
        auto* character = target && s->IsObjectAlive(target) ? dynamic_cast<CBaseCharacter*>(s->HandleToObject(target))
                                                           : nullptr;
        const bool timed_out = now > hit_time + trap_duration;
        if (!character || character->m_damage.IsDead() || timed_out)
        {
            FreeTarget(!timed_out);
            Release();
        }
        else if (now > last_damage + trap_interval)
        {
            DVector at;
            if (s->GetObjectPos(target, &at) != DE_OK ||
                !deliver_damage(this, target, {{}, at, m_fDamage, DamageType::Normal, m_hFiredFrom}))
                return Fail("Flayer attached damage delivery failed");
            last_damage = now;
            ++damage_deliveries;
        }
    }
    else if (state == State::Tracking && target)
        return Track(position);
    else if (state == State::Retracting)
    {
        if (now >= retract_time + retract_duration)
            return false;
        auto next = vector_add(hook_start, vector_scale(vector_subtract(hook_end, hook_start),
                                                        (now - retract_time) / retract_duration));
        if (s->TeleportObject(m_hObject, &next) != DE_OK)
            return Fail("Flayer retraction position rejected");
    }
    return true;
}
bool CFlayerChain::Track(DVector position)
{
    auto* s = g_pServerDE;
    DVector at, velocity;
    DRotation rotation, aimed, next;
    if (s->GetObjectPos(target, &at) != DE_OK || s->GetObjectRotation(m_hObject, &rotation) != DE_OK ||
        s->GetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("Flayer tracking pose unavailable");
    const auto direction = vector_subtract(at, position);
    if (vector_dot(direction, direction) < 1e-8f)
        return true;
    if (!align_rotation(direction, rotate_vector(rotation, {0, 1, 0}), aimed) ||
        s->InterpolateRotation(next, rotation, aimed, .2f) != DE_OK)
        return Fail("Flayer tracking rotation unavailable");
    velocity = vector_scale(rotate_vector(next, {0, 0, 1}), std::sqrt(vector_dot(velocity, velocity)));
    if (s->SetObjectRotation(m_hObject, &next) != DE_OK || s->SetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("Flayer tracking velocity unavailable");
    return true;
}
void CFlayerChain::HandleContact(const SegmentHit& hit)
{
    if (state != State::Tracking || !configured || failed || m_bExplode)
        return;
    auto* s = g_pServerDE;
    const bool solid_target = hit.object && hit.object != m_hFiredFrom && s->IsObjectAlive(hit.object) &&
                              (s->GetObjectFlags(hit.object) & FLAG_SOLID);
    auto* character = solid_target ? dynamic_cast<CBaseCharacter*>(s->HandleToObject(hit.object)) : nullptr;
    if (!character)
    {
        CProjectile::HandleContact(hit);
        return;
    }
    DVector zero{};
    DRotation rotation;
    if (!SetTarget(hit.object) || s->SetVelocity(m_hObject, &zero) != DE_OK ||
        s->SetVelocity(hit.object, &zero) != DE_OK || s->GetObjectRotation(m_hObject, &rotation) != DE_OK ||
        s->GetObjectPos(m_hObject, &hook_start) != DE_OK)
    {
        Fail("Flayer attachment failed");
        return;
    }
    s->SetNextUpdate(hit.object, .0001f);
    ++character->m_nTrapped;
    contact = hit;
    contacted = true;
    m_hHitObject = hit.object;
    hit_time = s->GetTime();
    state = State::Attached;
    hook_end = vector_subtract(hook_start, vector_scale(rotate_vector(rotation, {0, 0, 1}), 25 * 4));
    PublishCampaignExplosion(m_hObject, hook_end, rotate_vector(rotation, {0, 1, 0}), 35);
    stop_sound(voice);
    voice = loop_sound(m_hObject, "Sounds_ao\\Weapons\\Flayer\\loophold.wav", 750, 1, sound_error);
}
void CFlayerChain::FreeTarget(bool reset)
{
    auto* s = g_pServerDE;
    if (target && s->IsObjectAlive(target))
    {
        if (state == State::Attached)
            if (auto* character = dynamic_cast<CBaseCharacter*>(s->HandleToObject(target)))
                character->m_nTrapped = reset ? 0 : std::max(0, character->m_nTrapped - 1);
        s->BreakInterObjectLink(m_hObject, target);
    }
    target = m_hHitObject = nullptr;
}
void CFlayerChain::Release()
{
    state = State::Retracting;
    retract_time = g_pServerDE->GetTime();
    CreateGib();
    stop_sound(voice);
    voice = loop_sound(m_hObject, "Sounds_ao\\Weapons\\Flayer\\return.wav", 750, 1, sound_error);
}
void CFlayerChain::CreateGib()
{
    auto* s = g_pServerDE;
    ObjectCreateStruct settings;
    settings.m_ObjectType = OT_MODEL;
    settings.m_Flags = FLAG_VISIBLE | FLAG_FORCECLIENTUPDATE;
    settings.m_Pos = hook_start;
    legacy_copy(settings.m_Filename, "models\\gibs\\flesh\\gib5.abc");
    legacy_copy(settings.m_SkinName, "skins\\gibs\\flesh\\gib7.dtx");
    if (!s->RandomFloat(.5f, .75f, settings.m_Scale.x) || !s->RandomFloat(.5f, .75f, settings.m_Scale.y) ||
        !s->RandomFloat(.4f, .6f, settings.m_Scale.z))
    {
        effect_error = "Flayer gib scale unavailable";
        return;
    }
    auto* piece = s->CreateObject(s->GetClass("BaseClass"), &settings);
    if (!piece)
    {
        effect_error = "Flayer gib model unavailable";
        return;
    }
    gib = piece->m_hObject;
    const DVector offset{0, 0, 5};
    int x = 0, y = 0;
    if (!s->RandomInt(0, 360, x) || !s->RandomInt(0, 360, y))
    {
        s->RemoveObject(gib);
        gib = nullptr;
        effect_error = "Flayer gib rotation unavailable";
        return;
    }
    // Source passes these draws directly as Euler radians.
    const auto rotation = rotation_from_euler({float(x), float(y), 0});
    if (s->CreateAttachment(m_hObject, gib, nullptr, &offset, &rotation, &gib_attachment) != DE_OK)
    {
        s->RemoveObject(gib);
        gib = nullptr;
        effect_error = "Flayer gib attachment unavailable";
        return;
    }
    s->PublishObjectParticles(gib, 41, effect_error);
}
void CFlayerChain::AddExplosion(DVector position, DVector normal)
{
    PublishCampaignExplosion(m_hObject, position, normal, 36, "Sounds_ao\\Weapons\\Flayer\\chainbreak.wav", 750);
}

CFlayerPortal::CFlayerPortal() : CProjectile(OT_NORMAL)
{
    filename = skin = nullptr;
    damage_type = DamageType::Normal;
    flags &= ~FLAG_VISIBLE;
}
CFlayerPortal::~CFlayerPortal()
{
    stop_sound(voice);
}
bool CFlayerPortal::InitialUpdate()
{
    if (!IsAddonBuild() || !CProjectile::InitialUpdate())
        return false;
    g_pServerDE->SetNextUpdate(m_hObject, .1f);
    last_chain_time = m_fStartTime + portal_interval; // First launch after two intervals.
    DVector position;
    if (g_pServerDE->GetObjectPos(m_hObject, &position) != DE_OK)
        return false;
    PublishCampaignExplosion(m_hObject, position, {0, 1, 0}, 34);
    return true;
}
bool CFlayerPortal::Update()
{
    auto* s = g_pServerDE;
    s->SetNextUpdate(m_hObject, .1f);
    if (!configured)
        return Fail("Flayer portal never received Setup");
    if (!voice)
        voice = loop_sound(m_hObject, "Sounds_ao\\Weapons\\Flayer\\chainwarp.wav", 1000, 2, sound_error);
    const auto now = s->GetTime();
    if (now > m_fStartTime + portal_duration)
        return false;
    if (!m_hFiredFrom || now <= last_chain_time + portal_interval)
        return true;
    last_chain_time = now;
    DVector position;
    if (s->GetObjectPos(m_hObject, &position) != DE_OK)
        return Fail("Flayer portal position unavailable");
    for (auto handle : destructables())
    {
        if (handle == m_hFiredFrom || !s->IsObjectAlive(handle) ||
            !dynamic_cast<CBaseCharacter*>(s->HandleToObject(handle)))
            continue;
        DVector at;
        if (s->GetObjectPos(handle, &at) != DE_OK)
            continue;
        const auto offset = vector_subtract(at, position);
        if (vector_dot(offset, offset) <= portal_radius * portal_radius)
            return LaunchChain(position, handle);
    }
    return true;
}
bool CFlayerPortal::LaunchChain(DVector position, HOBJECT target)
{
    auto* s = g_pServerDE;
    DVector direction{0, 1, 0};
    if (!s->RandomFloat(0, 1, direction.x) || !s->RandomFloat(0, 1, direction.z))
        return Fail("Flayer portal direction RNG unavailable");
    direction = vector_scale(direction, 1 / std::sqrt(vector_dot(direction, direction)));
    ObjectCreateStruct settings;
    settings.m_Pos = position;
    legacy_copy(settings.m_Name, "CFlayerChain");
    auto* chain = dynamic_cast<CFlayerChain*>(s->CreateObject(s->GetClass("CFlayerChain"), &settings));
    if (!chain || !chain->Setup(direction, m_nWeaponType, 10, 500, 0, m_hFiredFrom))
        return Fail("Flayer portal chain creation failed");
    if (!chain->SetTarget(target))
    {
        s->RemoveObject(chain->m_hObject);
        return Fail("Flayer portal chain target unavailable");
    }
    last_chain = chain->m_hObject;
    ++chains_created;
    return true;
}
