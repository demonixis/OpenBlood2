// Original ObjectDLL laser beam, sparks, transient model and impact objects.
#include "LegacyEffects.h"
#include "ClientSplatFX.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include "engine/movement_state.h"
#include "radius_damage.h"
#include <algorithm>
#include <numbers>
#include <random>

using namespace OpenBlood2;

bool CClientLaserBeamSFX::Valid() const
{
    return type <= 13 && save_fields::finite(damage) && damage >= 0 && save_fields::finite(radius) && radius >= 0 &&
           save_fields::finite(sound_radius) && sound_radius >= 0 && physics_vector(source) &&
           physics_vector(destination);
}
void CClientLaserBeamSFX::Fire(HOBJECT sender)
{
    auto* server = GetServerDE();
    DRotation rotation;
    if (server->GetObjectPos(m_hObject, &source) != DE_OK || server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
    {
        error = "Laser emitter pose unavailable";
        return;
    }
    const auto end = vector_add(source, vector_scale(rotate_vector({0, 0, 1}, rotation), 3000));
    SegmentHit hit;
    const auto query = server->IntersectSegment(source, end, m_hObject, hit, false, SegmentObjects::SolidOnly,
                                                SegmentFilter::IgnoreVolumes);
    if (query != SegmentResult::Hit && query != SegmentResult::Miss)
    {
        error = "Laser emitter segment unavailable";
        return;
    }
    destination = query == SegmentResult::Hit ? hit.point : end;
    targets = DamageObjectsInRadius(sender, this, destination, radius, damage, DamageType::Normal);
    if (!server->IsObjectAlive(m_hObject))
        return;
    effect = server->PublishLaserBeam(m_hObject, source, destination, error, type);
    if (!sound.empty() && sound_radius > 0)
    {
        SoundRequest request;
        request.resource = sound;
        request.speaker = m_hObject;
        request.space = SoundSpace::Spatial;
        request.outer_radius = sound_radius;
        request.inner_radius = sound_radius * .1f;
        request.priority = 2;
        server->PlayCharacterSound(m_hObject, request, false, sound_error);
    }
}
DDWORD CClientLaserBeamSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (message == MID_PRECREATE && (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP))
    {
        GenericProp property;
        if (server->GetPropGeneric("BeamType", &property) == DE_OK)
        {
            if (property.m_Long < 0 || property.m_Long > 13)
                return 0;
            type = static_cast<DBYTE>(property.m_Long);
        }
        if (server->GetPropGeneric("Sound", &property) == DE_OK)
            sound = property.m_String;
        server->GetPropReal("Damage", &damage);
        server->GetPropReal("DamageRadius", &radius);
        server->GetPropReal("SoundRadius", &sound_radius);
        if (!Valid())
            return 0;
    }
    else if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME)
    {
        server->SetObjectFlags(m_hObject, (server->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY) | FLAG_FORCECLIENTUPDATE);
        server->SetObjectUserFlags(m_hObject, server->GetObjectUserFlags(m_hObject) | USRFLG_SAVEABLE);
        server->SetNextUpdate(m_hObject, .1f);
    }
    else if (message == MID_UPDATE)
        server->SetNextUpdate(m_hObject, .1f);
    else if (message == MID_SAVEOBJECT && data)
    {
        const auto h = *static_cast<HMESSAGEWRITE*>(data);
        server->WriteToMessageVector(h, &source);
        server->WriteToMessageVector(h, &destination);
        server->WriteToMessageByte(h, type);
        server->WriteToMessageFloat(h, damage);
        server->WriteToMessageFloat(h, radius);
        save_fields::write_string(h, sound);
        server->WriteToMessageFloat(h, sound_radius);
    }
    else if (message == MID_LOADOBJECT && data)
    {
        const auto h = *static_cast<HMESSAGEREAD*>(data);
        server->ReadFromMessageVector(h, &source);
        server->ReadFromMessageVector(h, &destination);
        type = server->ReadFromMessageByte(h);
        damage = server->ReadFromMessageFloat(h);
        radius = server->ReadFromMessageFloat(h);
        const bool read_sound = save_fields::read_string(h, sound);
        sound_radius = server->ReadFromMessageFloat(h);
        if (!read_sound || !Valid())
            server->RejectPayload(h, "Invalid laser emitter payload");
    }
    return CClientSFX::EngineMessageFn(message, data, reason);
}
DDWORD CClientLaserBeamSFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    auto* server = GetServerDE();
    if (message == 1000 /* MID_TRIGGER */)
        if (auto text = server->ReadFromMessageHString(read))
        {
            const auto* command = server->GetStringData(text);
            const bool fire = command && std::string_view(command).starts_with("FIRE");
            server->FreeString(text);
            if (fire)
                Fire(sender);
        }
    return CClientSFX::ObjectMessageFn(sender, message, read);
}

std::uint64_t CClientSparksSFX::Setup(SparkSettings settings, std::string& error)
{
    // The original packet sends zero position and identity rotation regardless of the carrier pose.
    settings.position = {};
    return GetServerDE()->PublishSparks(m_hObject, settings, error);
}
DDWORD TransientClientSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_INITIALUPDATE)
        GetServerDE()->SetNextUpdate(m_hObject, .0001f);
    else if (message == MID_UPDATE)
        GetServerDE()->RemoveObject(m_hObject);
    return CClientSFX::EngineMessageFn(message, data, reason);
}

std::uint64_t CBulletImpactSFX::Setup(DVector normal, float scale, std::string sprite, std::string& error)
{
    auto* server = GetServerDE();
    ImpactMarkSettings settings{{}, normal, scale, std::move(sprite)};
    if (server->GetObjectPos(m_hObject, &settings.position) != DE_OK)
    {
        error = "Bullet impact pose unavailable";
        return 0;
    }
    bool reused = false;
    return server->PublishImpactMark(m_hObject, settings, reused, error);
}

CClientMarkSFX::~CClientMarkSFX()
{
    if (owns_effect && g_pServerDE)
        g_pServerDE->RemoveImpactMark(effect);
}
bool CClientMarkSFX::Setup(DVector normal, DBYTE ammo, SurfaceType surface, HOBJECT link)
{
    auto* server = GetServerDE();
    if (effect || target || (link && !server->IsObjectAlive(link)))
        return false;
    ImpactMarkSettings settings{{}, normal, ammo == AMMO_BULLET ? .05f : .035f, {}};
    if (server->GetObjectPos(m_hObject, &settings.position) != DE_OK)
        return false;
    int variant = 0;
    switch (surface)
    {
        case SURFTYPE_UNKNOWN:
            break;
        case SURFTYPE_METAL:
            settings.sprite = "sprites/blthole.spr";
            break;
        case SURFTYPE_PLASTIC:
        case SURFTYPE_CLOTH:
            settings.sprite = "sprites/blthole2.spr";
            break;
        case SURFTYPE_WOOD:
            settings.sprite = "sprites/blthole5.spr";
            break;
        case SURFTYPE_GLASS:
            if (!server->RandomInt(0, 1, variant))
                return false;
            settings.sprite = variant ? "sprites/glshole1.spr" : "sprites/glshole2.spr";
            break;
        default:
            if (!server->RandomInt(0, 1, variant))
                return false;
            settings.sprite = variant ? "sprites/blthole3.spr" : "sprites/blthole4.spr";
            break;
    }
    if (link && server->CreateInterObjectLink(m_hObject, link) != DE_OK)
        return false;
    target = link;
    if (settings.sprite.empty())
        return true;
    bool reused = false;
    effect = server->PublishImpactMark(m_hObject, settings, reused, error);
    owns_effect = effect && !reused;
    if (!effect && target)
    {
        server->BreakInterObjectLink(m_hObject, target);
        target = nullptr;
    }
    return effect != 0;
}
DDWORD CClientMarkSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_LINKBROKEN && data == target)
        GetServerDE()->RemoveObject(m_hObject);
    return CClientSFX::EngineMessageFn(message, data, reason);
}

bool CClientGibFX::Setup(DebrisEffect settings)
{
    auto* server = GetServerDE();
    if (!IsDemoBuild() || !server->IsObjectAlive(m_hObject) || !physics_vector(settings.position) ||
        !physics_vector(settings.velocity) || !physics_vector(settings.dimensions) ||
        !save_fields::finite(settings.scale) || settings.scale <= 0 || !settings.amount)
        return false;
    settings.source = server->GetObjectName(m_hObject);
    settings.velocity = vector_scale(settings.velocity, -1);
    server->RecordDebrisEffect(settings);
    return true;
}
DDWORD CClientGibFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE && !IsDemoBuild())
        return 0;
    return TransientClientSFX::EngineMessageFn(message, data, reason);
}

bool CClientBloodTrail::Setup(HOBJECT parent)
{
    auto* server = GetServerDE();
    if (parent_ || !server->IsObjectAlive(parent) || server->CreateInterObjectLink(m_hObject, parent) != DE_OK)
        return false;
    const DVector offset{};
    const DRotation rotation{};
    HATTACHMENT attachment = 0;
    if (server->CreateAttachment(parent, m_hObject, nullptr, &offset, &rotation, &attachment) != DE_OK)
    {
        server->BreakInterObjectLink(m_hObject, parent);
        return false;
    }
    parent_ = parent;
    return true;
}
DDWORD CClientBloodTrail::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (message == MID_INITIALUPDATE)
        server->SetNextUpdate(m_hObject, .02f);
    else if (message == MID_LINKBROKEN && data == parent_)
        server->RemoveObject(m_hObject);
    else if (message == MID_UPDATE)
    {
        DVector origin;
        DRotation rotation;
        SegmentHit hit;
        if (server->GetObjectPos(m_hObject, &origin) != DE_OK ||
            server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
            return 0;
        const auto end = vector_add(origin, rotate_vector({0, -5, 0}, rotation));
        const bool struck =
            server->IntersectSegment(origin, end, parent_, hit, false, SegmentObjects::SolidOnly) == SegmentResult::Hit;
        if (struck)
        {
            ObjectCreateStruct state;
            state.m_Pos = vector_add(hit.point, vector_scale(hit.normal, .5f));
            auto* splat =
                dynamic_cast<CClientSplatFX*>(server->CreateObject(server->GetClass("CClientSplatFX"), &state));
            if (splat && splat->Setup(hit.point, hit.normal, .2f, .01f))
                ++splats;
        }
        server->SetNextUpdate(m_hObject, struck ? .1f : .2f);
    }
    return CClientSFX::EngineMessageFn(message, data, reason);
}

bool CClientTracer::Setup(DVector destination, HOBJECT gun)
{
    auto* server = GetServerDE();
    DVector position;
    if (!server->IsObjectAlive(gun) || server->GetObjectPos(m_hObject, &position) != DE_OK ||
        !physics_vector(destination))
        return false;
    const auto displacement = vector_subtract(destination, position);
    DRotation rotation;
    if (server->AlignRotation(rotation, displacement) != DE_OK)
        return false;
    const auto forward = rotate_vector({0, 0, 1}, rotation);
    const auto right = rotate_vector({1, 0, 0}, rotation);
    const auto start = vector_add(position, vector_scale(forward, 50));
    SegmentHit hit;
    if (server->CastWorldRay(start, forward, hit) != SegmentResult::Hit)
        return false;
    const auto to_world = vector_subtract(hit.point, position);
    const float distance = std::sqrt(vector_dot(to_world, to_world)) - 50;
    if (distance < 50)
        return false;
    // Presentation-only draw, independent from the authoritative weapon RNG.
    std::minstd_rand random(static_cast<unsigned>(std::fmod(server->GetTime() * 1000., 2147483646.)) + 1);
    const auto center =
        vector_add(start, vector_scale(forward, 50 + std::uniform_real_distribution<float>(0, distance)(random)));
    bool drawn = true;
    for (const float offset : {0.f, .05f, -.05f})
    {
        const auto axis = vector_add(center, vector_scale(right, offset));
        drawn &= server->PublishLine(m_hObject,
                                     {vector_subtract(axis, vector_scale(forward, 50)),
                                      vector_add(axis, vector_scale(forward, 50)),
                                      {1, .8f, 0},
                                      {1, 0, 0},
                                      .1f,
                                      .9f},
                                     .1f);
    }
    return drawn;
}

bool CClientLaserFX::Setup(HOBJECT gun)
{
    auto* server = GetServerDE();
    if (gun_ || !server->IsObjectAlive(gun) || server->CreateInterObjectLink(m_hObject, gun) != DE_OK)
        return false;
    gun_ = gun;
    server->SetNextUpdate(m_hObject, .001f);
    return true;
}
void CClientLaserFX::Update()
{
    auto* server = GetServerDE();
    server->SetNextUpdate(m_hObject, .001f);
    DVector source;
    DRotation rotation;
    visible = false;
    if (server->GetObjectPos(m_hObject, &source) != DE_OK || server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
        return;
    const auto forward = rotate_vector({0, 0, 1}, rotation);
    const auto right = vector_scale(rotate_vector({1, 0, 0}, rotation), .15f);
    const auto up = vector_scale(rotate_vector({0, 1, 0}, rotation), .15f);
    auto end = vector_add(source, vector_scale(forward, 4000));
    SegmentHit hit;
    const auto result = server->CastWorldRay(source, forward, hit);
    if (result != SegmentResult::Miss && result != SegmentResult::Hit)
        return;
    if (result == SegmentResult::Hit)
    {
        // Source takes the magnitude of the absolute hit point, then clamps it to range.
        const auto range = std::min(4000.f, std::sqrt(vector_dot(hit.point, hit.point)));
        end = vector_add(source, vector_scale(forward, range));
    }
    const std::array<DVector, 5> offsets{{{}, right, vector_scale(right, -1), up, vector_scale(up, -1)}};
    for (unsigned i = 0; i < lines.size(); ++i)
        lines[i] = {vector_add(source, offsets[i]), vector_add(end, offsets[i]), {1, 0, 0}, {1, 0, 0}, .5f, .5f};
    visible = true;
}
DDWORD CClientLaserFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_LINKBROKEN && data == gun_)
        GetServerDE()->RemoveObject(m_hObject);
    else if (message == MID_UPDATE && gun_)
        Update();
    return CClientSFX::EngineMessageFn(message, data, reason);
}

bool CShellCasing::RandomSpin()
{
    constexpr float spin = 4 * std::numbers::pi_v<float>;
    return GetServerDE()->RandomFloat(-spin, spin, pitch_speed) && GetServerDE()->RandomFloat(-spin, spin, yaw_speed);
}
bool CShellCasing::Setup(HOBJECT shooter, bool left)
{
    auto* server = GetServerDE();
    DRotation rotation;
    if (configured || !server->IsObjectAlive(shooter) || server->GetObjectRotation(m_hObject, &rotation) != DE_OK ||
        server->CreateInterObjectLink(m_hObject, shooter) != DE_OK)
        return false;
    float upward = 0, sideways = 0, forward = 0;
    const bool drawn = server->RandomFloat(60, 90, upward) && server->RandomFloat(50, 70, sideways) &&
                       server->RandomFloat(10, 25, forward);
    auto launch = rotate_vector({left ? -sideways : sideways, upward, forward}, rotation);
    if (!drawn || server->SetVelocity(m_hObject, &launch) != DE_OK)
    {
        server->BreakInterObjectLink(m_hObject, shooter);
        return false;
    }
    configured = true;
    shooter_ = shooter;
    return true;
}
bool CShellCasing::Update()
{
    auto* server = GetServerDE();
    if (server->GetTime() >= expires)
        return false;
    if (first)
    {
        first = false;
        server->SetObjectFlags(m_hObject, FLAG_VISIBLE | FLAG_TOUCH_NOTIFY);
    }
    DVector velocity, gravity;
    if (server->GetVelocity(m_hObject, &velocity) != DE_OK || server->GetGlobalForce(&gravity) != DE_OK)
        return false;
    if (!(server->GetObjectFlags(m_hObject) & FLAG_GRAVITY))
    {
        velocity = vector_add(velocity, vector_scale(gravity, .004f));
        if (server->SetVelocity(m_hObject, &velocity) != DE_OK)
            return false;
    }
    if (vector_dot(velocity, velocity) < 25)
    {
        const auto rotation = rotation_from_euler({0, yaw, 0});
        server->SetObjectFlags(m_hObject, FLAG_VISIBLE | FLAG_GRAVITY);
        server->SetNextUpdate(m_hObject, expires - server->GetTime());
        return server->SetObjectRotation(m_hObject, &rotation) == DE_OK;
    }
    pitch += pitch_speed * server->GetFrameTime();
    yaw += yaw_speed * server->GetFrameTime();
    const auto rotation = rotation_from_euler({pitch, yaw, 0});
    server->SetNextUpdate(m_hObject, .001f);
    return server->SetObjectRotation(m_hObject, &rotation) == DE_OK;
}
void CShellCasing::Bounce(HOBJECT target)
{
    auto* server = GetServerDE();
    if (!bounces || (target && (target == shooter_ || !(server->GetObjectFlags(target) & FLAG_SOLID))))
        return;
    DVector velocity, normal;
    if (server->GetVelocity(m_hObject, &velocity) != DE_OK || !server->GetContactNormal(m_hObject, normal))
        return;
    const auto into = vector_dot(velocity, normal) * .3f;
    if (into <= -100)
    {
        velocity = vector_subtract(velocity, vector_scale(normal, into));
        RandomSpin();
    }
    velocity = vector_scale(velocity, .6f);
    server->SetVelocity(m_hObject, &velocity);
    if (--bounces)
    {
        SoundRequest request;
        request.resource = "Sounds\\Weapons\\Shelldrop.wav";
        request.speaker = m_hObject;
        request.space = SoundSpace::Spatial;
        request.outer_radius = 150;
        request.inner_radius = 15;
        request.priority = 1;
        std::string error;
        server->PlayCharacterSound(m_hObject, request, false, error);
    }
    else
        server->SetObjectFlags(m_hObject, FLAG_VISIBLE | FLAG_GRAVITY | FLAG_TOUCH_NOTIFY);
}
DDWORD CShellCasing::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (message == MID_INITIALUPDATE)
    {
        expires = server->GetTime() + 20;
        server->SetObjectFlags(m_hObject, FLAG_TOUCH_NOTIFY);
        server->SetNextUpdate(m_hObject, .001f);
        if (!RandomSpin() || !server->ConfigureModelBody(m_hObject, {1, 18, false, false}))
            server->RemoveObject(m_hObject);
    }
    else if (message == MID_UPDATE && !Update())
        server->RemoveObject(m_hObject);
    else if (message == MID_TOUCHNOTIFY)
        Bounce(static_cast<HOBJECT>(data));
    else if (message == MID_LINKBROKEN && data == shooter_)
        shooter_ = nullptr;
    return BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD CShellCasing::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == damage_message)
        GetServerDE()->RemoveObject(m_hObject);
    return BaseClass::ObjectMessageFn(sender, message, read);
}

bool CModelObject::Setup(float life, DVector periods, bool stop_on_ground, bool random_rotation)
{
    auto* server = GetServerDE();
    if (!server->IsObjectAlive(m_hObject) || !save_fields::finite(life) || !physics_vector(periods))
        return false;
    constexpr float circle = 2 * std::numbers::pi_v<float>;
    DVector pose;
    if (random_rotation && (!server->RandomFloat(0, circle, pose.x) || !server->RandomFloat(0, circle, pose.y) ||
                            !server->RandomFloat(0, circle, pose.z)))
        return false;
    const auto angular_speed = [](float period) { return std::abs(period) > .001f ? circle / period : 0.f; };
    angles = pose;
    speed = {angular_speed(periods.x), angular_speed(periods.y), angular_speed(periods.z)};
    lifetime = life;
    rotating = vector_dot(periods, periods) > .001f;
    stop_rotation = stop_on_ground;
    server->SetNextUpdate(m_hObject, rotating ? .01f : std::max(0.f, lifetime));
    return true;
}
bool CModelObject::Tick()
{
    auto* server = GetServerDE();
    const float now = server->GetTime();
    if (first)
    {
        first = false;
        start = now;
    }
    MovementState support;
    if (rotating && stop_rotation && server->GetMovementState(m_hObject, support) && support.standing)
        rotating = false;
    if (rotating)
    {
        angles = vector_add(angles, vector_scale(speed, now - last_update));
        const auto rotation = rotation_from_euler(angles);
        if (!physics_vector(angles) || server->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return false;
        last_update = now;
    }
    const float elapsed = now - start;
    server->SetNextUpdate(m_hObject, rotating ? .01f : std::max(0.f, lifetime - elapsed));
    return lifetime <= 0 || elapsed < lifetime;
}
DDWORD CModelObject::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_INITIALUPDATE)
        GetServerDE()->SetNextUpdate(m_hObject, .01f);
    else if (message == MID_UPDATE && !Tick())
        GetServerDE()->RemoveObject(m_hObject);
    return BaseClass::EngineMessageFn(message, data, reason);
}

bool CImpact::Setup(DVector minimum, DVector maximum, float duration, bool rotateable, bool fading)
{
    auto* server = GetServerDE();
    if (!physics_vector(minimum) || !physics_vector(maximum) || !save_fields::finite(duration) ||
        !server->GetObjectColor(m_hObject, color, alpha))
        return false;
    low = minimum;
    high = maximum;
    lifetime = duration;
    scale_changes = low.x != high.x || low.y != high.y;
    fade = fading;
    if (rotateable)
        server->SetObjectFlags(m_hObject, server->GetObjectFlags(m_hObject) | FLAG_ROTATEABLESPRITE);
    return true;
}
bool CImpact::Tick()
{
    auto* server = GetServerDE();
    const float now = server->GetTime();
    server->SetNextUpdate(m_hObject, .1f);
    if (first)
    {
        first = false;
        start = now;
        if (!server->ScaleObject(m_hObject, low))
            return false;
    }
    if (lifetime <= 0)
        return true;
    const auto elapsed = now - start;
    if (scale_changes)
    {
        const auto interpolated = vector_add(low, vector_scale(vector_subtract(high, low), elapsed / lifetime));
        if (!server->ScaleObject(m_hObject, {std::min(interpolated.x, high.x), std::min(interpolated.y, high.y),
                                             std::min(interpolated.z, high.z)}) ||
            (fade && !server->SetObjectColor(m_hObject, color, std::max(0.f, alpha * (1 - elapsed / lifetime)))))
            return false;
    }
    return elapsed < lifetime;
}
DDWORD CImpact::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    const auto accepted = BaseClass::EngineMessageFn(message, data, reason);
    if (message == MID_PRECREATE && data)
    {
        auto& state = *static_cast<ObjectCreateStruct*>(data);
        if (!state.m_Filename[0])
            legacy_copy(state.m_Filename, "sprites\\pinpuncherimpact.spr");
        state.m_SkinName[0] = 0;
        state.m_Scale = {.1f, .1f, 0};
    }
    else if (message == MID_INITIALUPDATE)
    {
        GetServerDE()->SetObjectFlags(m_hObject, FLAG_VISIBLE);
        GetServerDE()->SetNextUpdate(m_hObject, .1f);
    }
    else if (message == MID_UPDATE && !Tick())
        GetServerDE()->RemoveObject(m_hObject);
    return accepted;
}
DDWORD CImpactRicochet::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    const auto accepted = BaseClass::EngineMessageFn(message, data, reason);
    if (message == MID_PRECREATE && data)
    {
        auto& state = *static_cast<ObjectCreateStruct*>(data);
        state.m_Flags = FLAG_VISIBLE;
        state.m_Scale = {.25f, .25f, .25f};
    }
    else if (message == MID_INITIALUPDATE)
    {
        start = GetServerDE()->GetTime();
        GetServerDE()->SetNextUpdate(m_hObject, .1f);
    }
    else if (message == MID_UPDATE)
    {
        if (GetServerDE()->GetTime() - start >= 8.f / 15.f)
            GetServerDE()->RemoveObject(m_hObject);
        else
            GetServerDE()->SetNextUpdate(m_hObject, .1f);
    }
    return accepted;
}
