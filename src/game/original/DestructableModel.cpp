// Selective original-source port. See destructable_model_contracts.md for boundaries.
#include "DestructableModel.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include "engine/model_commands.h"
#include <algorithm>
#include <charconv>
#include <cmath>

using namespace OpenBlood2;
namespace
{
    bool number(std::string_view text, float& out)
    {
        if (text.starts_with('+'))
        {
            text.remove_prefix(1);
            if (text.starts_with('-'))
                return false;
        }
        if (text.empty() || text.starts_with('+'))
            return false;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
        return ec == std::errc{} && end == text.data() + text.size() && std::isfinite(out);
    }
    bool dimensions(DVector value)
    {
        return physics_vector(value) && value.x >= 0 && value.y >= 0 && value.z >= 0;
    }
} // namespace
CDestructableModel::CDestructableModel() : B2BaseClass(OT_MODEL)
{
    AddAggregate(&m_damage);
    AddAggregate(&debris);
}
CDestructableModel::~CDestructableModel()
{
    if (sliding_sound && GetServerDE())
        GetServerDE()->KillSound(sliding_sound);
}
bool CDestructableModel::Fail(const char* detail)
{
    if (error.empty())
        error = detail;
    auto* s = GetServerDE();
    s->SetNextUpdate(m_hObject, 0);
    if (!initialized)
        s->RemoveObject(m_hObject);
    else
    {
        DVector zero{};
        s->SetVelocity(m_hObject, &zero);
        s->SetObjectState(m_hObject, OBJSTATE_INACTIVE);
    }
    return false;
}
bool CDestructableModel::ReadProp(ObjectCreateStruct& create)
{
    auto* s = GetServerDE();
    GenericProp p;
    auto text = [&](const char* key, std::string& value)
    {
        if (s->GetPropGeneric(key, &p) == DE_OK)
            value = p.m_String;
    };
    auto vec = [&](const char* key, DVector& value)
    {
        if (s->GetPropGeneric(key, &p) == DE_OK)
            value = p.m_Vec;
    };
    auto flag = [&](const char* key, bool& value)
    {
        if (s->GetPropGeneric(key, &p) == DE_OK)
            value = p.m_Bool != 0;
    };
    std::string model, skin;
    text("InitFilename", model);
    text("InitSkin", skin);
    if (!model.empty())
        SAFE_STRCPY(create.m_Filename, model.c_str());
    if (!skin.empty())
        SAFE_STRCPY(create.m_SkinName, skin.c_str());
    text("DamageFilename", damage_model);
    text("DamageSkin", damage_skin);
    text("DestroyFilename", destroy_model);
    text("DestroySkin", destroy_skin);
    text("SlidingSound", sliding_path);
    vec("InitDims", init_dims);
    vec("DamageDims", damage_dims);
    vec("DestroyDims", destroy_dims);
    vec("TintColor", tint);
    bool visible = true, solid = true, gravity = true, chrome = false;
    flag("DestroyVisible", visible);
    flag("DestroySolid", solid);
    flag("DestroyGravity", gravity);
    destroy_flags = (visible ? FLAG_VISIBLE : 0) | (solid ? FLAG_SOLID : 0) | (gravity ? FLAG_GRAVITY : 0);
    flag("Pushable", pushable);
    flag("Destructable", destructable);
    flag("Chrome", chrome);
    s->GetPropReal("HitPoints", &hit_points);
    s->GetPropReal("DamageHitPoints", &damage_hit_points);
    s->GetPropReal("ObjectMass", &mass);
    s->GetPropReal("ModelScale", &scale);
    s->GetPropReal("Alpha", &alpha);
    target_alpha = alpha;
    if (s->GetPropGeneric("SurfaceType", &p) == DE_OK)
    {
        if (p.m_Long < 0 || p.m_Long > 255)
            return Reject("SurfaceType out of range");
        surface = static_cast<unsigned>(p.m_Long);
    }
    // Environment mapping and partial replacement pairs are not silently rendered as opaque skins.
    if (chrome)
        return Reject("Chrome environment mapping is unavailable");
    if (model.size() > MAX_CS_FILENAME_LEN || skin.size() > MAX_CS_FILENAME_LEN ||
        damage_model.size() > MAX_CS_FILENAME_LEN || damage_skin.size() > MAX_CS_FILENAME_LEN ||
        destroy_model.size() > MAX_CS_FILENAME_LEN || destroy_skin.size() > MAX_CS_FILENAME_LEN)
        return Reject("model or skin filename too long");
    if (destroy_model.empty() != destroy_skin.empty())
        return Reject("partial DestroyFilename/DestroySkin pair");
    if (!m_damage.spawn_valid)
        return Reject("SpawnObject not accepted: " + m_damage.spawn_object);
    if (!debris.valid)
        return Reject("invalid debris Scale");
    if (!dimensions(init_dims) || !dimensions(damage_dims) || !dimensions(destroy_dims))
        return Reject("invalid authored dimensions");
    if (!physics_vector(tint) || tint.x < 0 || tint.y < 0 || tint.z < 0 || tint.x > 255 || tint.y > 255 ||
        tint.z > 255)
        return Reject("TintColor out of range");
    if (!std::isfinite(hit_points) || std::abs(hit_points) > 1e6f || !std::isfinite(damage_hit_points) ||
        std::abs(damage_hit_points) > 1e6f || !std::isfinite(mass) || mass < 0 || mass > 1e6f ||
        !std::isfinite(scale) || scale <= 0 || scale > 1e6f || !std::isfinite(alpha) || alpha < 0 || alpha > 1)
        return Reject("invalid HitPoints, ObjectMass, ModelScale or Alpha");
    return true;
}
bool CDestructableModel::Reject(const std::string& reason)
{
    GetServerDE()->RejectPrecreate(reason);
    return false;
}
bool CDestructableModel::SetDimensions(DVector& dims, bool rotate, float shadow_limit)
{
    auto* s = GetServerDE();
    if (vector_dot(dims, dims) == 0)
    {
        if (s->GetModelAnimUserDims(m_hObject, &dims, 0) != DE_OK)
            return Fail("Model clip-zero dimensions unavailable");
        if (rotate)
        {
            DRotation rotation;
            if (s->GetObjectRotation(m_hObject, &rotation) != DE_OK)
                return Fail("Model rotation unavailable");
            DVector bounds{};
            for (int x : {-1, 1})
                for (int y : {-1, 1})
                    for (int z : {-1, 1})
                    {
                        const auto point = rotate_vector(rotation, {dims.x * x, dims.y * y, dims.z * z});
                        bounds.x = std::max(bounds.x, std::abs(point.x));
                        bounds.y = std::max(bounds.y, std::abs(point.y));
                        bounds.z = std::max(bounds.z, std::abs(point.z));
                    }
            dims = bounds;
        }
        dims = vector_scale(dims, scale);
    }
    if (!dimensions(dims) || s->SetObjectDims(m_hObject, &dims) != DE_OK)
        return Fail("Model dimensions rejected");
    if (vector_dot(dims, dims) < shadow_limit)
        s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_SHADOW);
    // SFX_DESTRUCTABLEMODEL only sets client physics dimensions. Direct integration shares this body.
    ++dimensions_publications;
    return true;
}
bool CDestructableModel::InitialUpdate(float reason)
{
    auto* s = GetServerDE();
    if (!s->SetObjectColor(m_hObject, vector_scale(tint, 1.f / 255), alpha) || !m_damage.Init(m_hObject) ||
        !debris.Init(m_hObject))
        return Fail("Model damage/color initialization failed");
    m_damage.m_fMaxHitPoints = hit_points;
    m_damage.SetHitPoints(hit_points);
    m_damage.m_fMass = mass;
    m_damage.apply_damage_physics = pushable;
    if (damage_model.empty() || damage_skin.empty())
        damage_hit_points = 0;
    if (!s->SetBlockingPriority(m_hObject, pushable ? 40 : 100) || !s->ScaleObject(m_hObject, {scale, scale, scale}) ||
        !SetDimensions(init_dims, true, 1000) ||
        !s->ConfigureModelBody(m_hObject, {mass, 5 + mass * 10 / 500, pushable}))
        return Fail("Model resource/body initialization failed");
    s->SetObjectUserFlags(m_hObject, s->GetObjectUserFlags(m_hObject) | (1u << 3) |
                                         (pushable ? (1u << 4) | (1u << 5) : 0) | (surface << 24));
    s->SetDeactivationTime(m_hObject, 1);
    if (sliding_path.empty())
    {
        const char* material = "wood";
        const char* materials[] = {"stone", "metal", "wood",   "terrain", "plastic",
                                   "glass", "flesh", "liquid", "energy"};
        for (unsigned i = 0; i < CDebris::surfaces.size(); ++i)
            if (debris.GetType() == CDebris::surfaces[i])
                material = materials[i];
        sliding_path = std::string("sounds/events/push") + material + ".wav";
    }
    if (debris.exploding)
        m_damage.m_fDeathDelay = .25f;
    if (reason == INITIALUPDATE_WORLDFILE && (s->GetObjectFlags(m_hObject) & FLAG_GRAVITY))
    {
        ground_result = s->MoveObjectToGround(m_hObject);
        if (ground_result != DE_OK && ground_result != DE_NOTFOUND)
            return Fail("Model ground placement unavailable");
        // The source helper also leaves a no-hit position unchanged. Retain that result for inspection.
    }
    initialized = true;
    return true;
}
bool CDestructableModel::ChangeModel(const std::string& model, const std::string& skin, DVector& dims, bool destroyed)
{
    auto* s = GetServerDE();
    if (s->SetModelFilenames(m_hObject, model.c_str(), skin.c_str()) != DE_OK ||
        !s->ScaleObject(m_hObject, {scale, scale, scale}))
        return Fail("Replacement model unavailable");
    if (destroyed)
        s->SetObjectFlags(m_hObject, destroy_flags);
    return SetDimensions(dims, false, 8000);
}
void CDestructableModel::HandleDamage()
{
    if (dead_state || !destructable || !error.empty() || !GetServerDE()->IsObjectAlive(m_hObject))
        return;
    if (m_damage.m_fHitPoints <= 0)
        GetServerDE()->SetNextUpdate(m_hObject, .001f);
    else if (damage_hit_points > 0 && m_damage.m_fHitPoints <= damage_hit_points &&
             ChangeModel(damage_model, damage_skin, damage_dims, false))
        damage_hit_points = 0;
}
void CDestructableModel::HandleDestruction()
{
    if (destroying_)
        return;
    destroying_ = true;
    dead_state = true;
    const bool remove = destroy_model.empty() && destroy_skin.empty();
    if (remove || ChangeModel(destroy_model, destroy_skin, destroy_dims, true))
    {
        ++destructions;
        // Keep checked source ownership through area-damage callbacks; commit deferred removal afterwards.
        debris.Create(m_damage.last_damage.direction, m_damage.m_fLastDamageAmount);
        if (remove)
            GetServerDE()->RemoveObject(m_hObject);
    }
    destroying_ = false;
}
void CDestructableModel::Update()
{
    auto* s = GetServerDE();
    if (!initialized || !error.empty())
        return;
    bool again = false;
    if (m_damage.m_fHitPoints <= 0)
    {
        if (m_damage.IsDead())
            HandleDestruction();
        else
            s->SetNextUpdate(m_hObject, .001f);
        if (!s->IsObjectAlive(m_hObject) || !error.empty())
            return;
    }
    if (sliding_frames)
        --sliding_frames;
    if (standing_frames)
        --standing_frames;
    if (sliding)
    {
        DVector position{};
        s->GetObjectPos(m_hObject, &position);
        bool slid = false;
        auto delta = vector_subtract(position, last_position);
        if (vector_dot(delta, delta) > .1f)
        {
            last_position = position;
            if (standing)
            {
                if (!sliding_sound)
                {
                    SoundRequest request;
                    request.resource = sliding_path;
                    request.speaker = m_hObject;
                    request.loop = true;
                    request.space = SoundSpace::Spatial;
                    request.outer_radius = 1000;
                    request.inner_radius = 250;
                    sliding_sound = s->PlayObjectLoopSound(m_hObject, request, sound_error);
                }
                slid = true;
                sliding_frames = 2;
                again = true;
            }
        }
        if (!slid && !sliding_frames)
        {
            sliding = false;
            if (sliding_sound)
                s->KillSound(sliding_sound);
            sliding_sound = 0;
        }
        else
            again = true;
    }
    if (standing)
    {
        MovementState support;
        if (!s->GetMovementState(m_hObject, support))
        {
            Fail("Model support query unavailable");
            return;
        }
        if (!support.standing && !standing_frames)
            standing = false;
    }
    if (std::abs(alpha - target_alpha) > .0001f)
    {
        const auto now = s->GetTime();
        const float adjust = fade_rate * (now - last_time);
        last_time = now;
        if (std::abs(adjust) >= std::abs(target_alpha - alpha))
            alpha = target_alpha;
        else
        {
            alpha += adjust;
            again = true;
        }
        if (!s->SetObjectColor(m_hObject, vector_scale(tint, 1.f / 255), alpha))
        {
            Fail("Model fade rejected");
            return;
        }
    }
    if (again)
    {
        s->SetNextUpdate(m_hObject, .001f);
        s->SetDeactivationTime(m_hObject, 1);
    }
}
void CDestructableModel::TouchNotify(HOBJECT other, float force)
{
    auto* s = GetServerDE();
    bool again = false;
    if (!sliding && dynamic_cast<CBaseCharacter*>(s->HandleToObject(other)))
    {
        sliding = true;
        sliding_frames = 2;
        again = true;
    }
    MovementState support;
    if (!s->GetMovementState(m_hObject, support))
    {
        Fail("Model contact support unavailable");
        return;
    }
    if (std::isfinite(force) && std::abs(force) > 0 && support.standing)
    {
        standing = true;
        standing_frames = 2;
        again = true;
    }
    if (again)
    {
        s->SetNextUpdate(m_hObject, .001f);
        s->SetDeactivationTime(m_hObject, 1);
    }
}
bool CDestructableModel::HandleTriggerMessage(std::string_view text)
{
    trigger_error.clear();
    std::vector<std::vector<std::string>> commands;
    if (text.size() > 4096 || text.find('\0') != text.npos || !model_commands(text, commands))
    {
        trigger_error = "Invalid model command text";
        return false;
    }
    auto alpha_command = [](const auto& a)
    { return a.size() > 1 && !CaseLess{}(a[0], "Alpha") && !CaseLess{}("Alpha", a[0]); };
    // Validate the whole group before mutation, then advance each parsed command (source reused pMsg).
    for (const auto& a : commands)
        if (alpha_command(a))
        {
            float value, duration = 0;
            if (!number(a[1], value) || value < 0 || value > 1 || (a.size() == 3 && !number(a[2], duration)) ||
                (duration > 0 && !std::isfinite((value - alpha) / duration)))
            {
                trigger_error = "Invalid Alpha value or fade duration";
                return false;
            }
        }
    for (const auto& a : commands)
        if (alpha_command(a))
        {
            float duration = 0;
            number(a[1], target_alpha);
            if (a.size() == 3)
                number(a[2], duration);
            fade_rate = duration > 0 ? (target_alpha - alpha) / duration : 1e10f;
            last_time = GetServerDE()->GetTime();
            GetServerDE()->SetNextUpdate(m_hObject, .01f);
            GetServerDE()->SetDeactivationTime(m_hObject, 1);
        }
    return true;
}
DDWORD CDestructableModel::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_SAVEOBJECT || message == MID_LOADOBJECT)
    {
        trigger_error = "Destructible model save/restore is unavailable";
        return 0;
    }
    if (message == MID_PRECREATE)
    {
        const auto result = B2BaseClass::EngineMessageFn(message, data, reason);
        if (!result || !data || reason == PRECREATE_SAVEGAME)
            return 0;
        auto& create = *static_cast<ObjectCreateStruct*>(data);
        create.m_ObjectType = OT_MODEL;
        if ((reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP) && !ReadProp(create))
            return 0;
        if (pushable)
            create.m_Flags |= FLAG_TOUCH_NOTIFY | FLAG_GRAVITY;
        if (reason == PRECREATE_STRINGPROP)
            create.m_Flags |=
                FLAG_GRAVITY | FLAG_SOLID | FLAG_SHADOW | FLAG_VISIBLE | FLAG_TOUCH_NOTIFY | FLAG_MODELGOURAUDSHADE;
        return !(create.m_Flags & FLAG_GOTHRUWORLD);
    }
    if (message == MID_INITIALUPDATE)
    {
        const auto result = B2BaseClass::EngineMessageFn(message, data, reason);
        if (GetServerDE()->IsObjectAlive(m_hObject))
            InitialUpdate(reason);
        return result;
    }
    if (message == MID_UPDATE)
        Update();
    if (message == MID_TOUCHNOTIFY && initialized && error.empty())
        TouchNotify(static_cast<HOBJECT>(data), reason);
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD CDestructableModel::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == damage_message)
    {
        const auto result = B2BaseClass::ObjectMessageFn(sender, message, read);
        HandleDamage();
        return result;
    }
    if (message == 1000)
    {
        auto* s = GetServerDE();
        auto text = s->ReadFromMessageHString(read);
        const char* value = text ? s->GetStringData(text) : nullptr;
        if (value && error.empty())
            HandleTriggerMessage(value);
        if (text)
            s->FreeString(text);
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
