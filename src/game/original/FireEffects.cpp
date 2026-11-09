// Original ObjectDLL Explosion.cpp and FireFX.cpp with typed model, sprite and light ownership.
#include "FireEffects.h"
#include "LegacyEffects.h"
#include "NativeLight.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace OpenBlood2;

namespace
{
    bool bounded(float value)
    {
        return std::isfinite(value) && value >= 0 && value <= 1e6f;
    }

    bool normalized_color(DVector value)
    {
        return physics_vector(value) && std::min({value.x, value.y, value.z}) >= 0 &&
               std::max({value.x, value.y, value.z}) <= 1;
    }

    HOBJECT make_light(DVector position, DVector color, float radius)
    {
        auto* server = BaseClass::GetServerDE();
        ObjectCreateStruct state;
        state.m_Pos = position;
        auto* object = dynamic_cast<LightFX*>(server->CreateObject(server->GetClass("LightFX"), &state));
        if (!object)
            return nullptr;
        const auto handle = object->m_hObject;
        if (object->SetConstant(color, radius))
            return handle;
        server->RemoveObject(handle);
        return nullptr;
    }

    void remove(HOBJECT handle)
    {
        auto* server = BaseClass::GetServerDE();
        if (server && server->IsObjectAlive(handle))
            server->RemoveObject(handle);
    }

    void scorch(DVector position, DVector rgb, float min_radius, float max_radius, float rise, float hold, float fall)
    {
        auto* server = BaseClass::GetServerDE();
        ObjectCreateStruct state;
        state.m_Pos = position;
        state.m_Flags = 1u << 5; // Original FLAG_ONLYLIGHTWORLD on an OT_LIGHT.
        const std::string props =
            "Color1 \"" + std::to_string(rgb.x) + ' ' + std::to_string(rgb.y) + ' ' + std::to_string(rgb.z) +
            "\"; NumRadiusCycles 1; RadiusMin " + std::to_string(min_radius) + "; RadiusMax " +
            std::to_string(max_radius) + "; RadiusRampUpTime " + std::to_string(rise) + "; RadiusMaxTime " +
            std::to_string(hold) + "; RadiusRampDownTime " + std::to_string(fall) + "; LifeTime " +
            std::to_string(rise + hold + fall);
        server->CreateObjectProps(server->GetClass("LightFX"), &state, props.c_str());
    }
} // namespace

Explosion::~Explosion()
{
    remove(light);
    remove(model);
}

bool Explosion::Setup(const char* sound, float volume_radius, float life, const char* skin, float damage_radius,
                      float max_damage, float minimum, float maximum, bool mark, bool, bool)
{
    if (!first || !bounded(volume_radius) || !bounded(life) || !bounded(damage_radius) || !bounded(max_damage) ||
        !bounded(minimum) || !bounded(maximum) || (skin && std::string_view(skin).size() >= sizeof(ObjectCreateStruct::m_SkinName)))
        return false;
    if (sound)
        sound_file = sound;
    if (skin)
        skin_file = skin;
    sound_radius = volume_radius;
    duration = life;
    min_scale = minimum;
    max_scale = maximum;
    create_mark = mark;
    // DamageRadius/MaxDamage are unused in the original Update. Sparks and smoke creation are disabled.
    return true;
}

bool Explosion::SetupLight(bool enabled, DVector color, float minimum, float maximum)
{
    if (!first || !normalized_color(color) || !bounded(minimum) || !bounded(maximum))
        return false;
    create_light = enabled;
    light_color = color;
    light_min = minimum;
    light_max = maximum;
    return true;
}

bool Explosion::SetupShockwave(const char* sprite, DVector minimum, DVector maximum, float life)
{
    if (!first || !sprite || std::string_view(sprite).size() >= sizeof(ObjectCreateStruct::m_Filename) ||
        !physics_vector(minimum) || !physics_vector(maximum) || !bounded(life))
        return false;
    shockwave_file = sprite;
    shockwave_min = minimum;
    shockwave_max = maximum;
    shockwave_duration = life;
    create_shockwave = true;
    return true;
}

bool Explosion::ReadProperties()
{
    auto* server = GetServerDE();
    GenericProp property;
    const auto text = [&](const char* key, std::string& value)
    {
        if (server->GetPropGeneric(key, &property) == DE_OK && property.m_String[0])
            value = property.m_String;
    };
    text("Model", model_file);
    text("Skin", skin_file);
    text("Sound", sound_file);
    text("ShockwaveFile", shockwave_file);
    server->GetPropReal("SoundRadius", &sound_radius);
    server->GetPropReal("MinScale", &min_scale);
    server->GetPropReal("MaxScale", &max_scale);
    // The unregistered Duration is read into MinScale in the original world-property branch.
    server->GetPropReal("Duration", &min_scale);
    server->GetPropReal("Delay", &delay);
    server->GetPropReal("ShockwaveDuration", &shockwave_duration);
    server->GetPropVector("RotateSpeed", &rotation_period);
    server->GetPropVector("ShockwaveScaleMin", &shockwave_min);
    server->GetPropVector("ShockwaveScaleMax", &shockwave_max);
    if (server->GetPropVector("LightColor", &light_color) == DE_OK)
        light_color = vector_scale(light_color, 1.f / 255);
    if (server->GetPropGeneric("CreateLight", &property) == DE_OK)
        create_light = property.m_Bool != 0;
    if (server->GetPropGeneric("CreateShockwave", &property) == DE_OK)
        create_shockwave = property.m_Bool != 0;
    return bounded(sound_radius) && bounded(min_scale) && bounded(max_scale) && bounded(delay) &&
           bounded(shockwave_duration) && normalized_color(light_color) && physics_vector(rotation_period) &&
           physics_vector(shockwave_min) && physics_vector(shockwave_max);
}

bool Explosion::Explode(float wait)
{
    auto* server = GetServerDE();
    if (!bounded(wait) || !server->IsObjectAlive(m_hObject))
        return false;
    server->SetNextUpdate(m_hObject, wait == 0 ? .01f : wait);
    return true;
}

void Explosion::CreateShockwave(DVector position)
{
    if (shockwave_file.empty())
        return;
    auto* server = GetServerDE();
    ObjectCreateStruct state;
    state.m_Pos = position;
    state.m_Flags = FLAG_VISIBLE | FLAG_ROTATEABLESPRITE;
    state.m_Rotation = rotation_from_euler({std::numbers::pi_v<float> / 2, 0, 0});
    legacy_copy(state.m_Filename, shockwave_file.c_str());
    if (auto* wave = dynamic_cast<CImpact*>(server->CreateObject(server->GetClass("CImpact"), &state)))
    {
        if (wave->Setup(shockwave_min, shockwave_max, shockwave_duration, true, false))
            shockwave = wave->m_hObject;
        else
            remove(wave->m_hObject);
    }
}

void Explosion::CreateExplosion(DVector position)
{
    auto* server = GetServerDE();
    ObjectCreateStruct state;
    state.m_Pos = position;
    state.m_Flags = FLAG_VISIBLE | FLAG_MODELGOURAUDSHADE | FLAG_NOLIGHT;
    DVector angles;
    constexpr float pi = std::numbers::pi_v<float>;
    if (!server->RandomFloat(-pi, pi, angles.x) || !server->RandomFloat(-pi, pi, angles.y) ||
        !server->RandomFloat(-pi, pi, angles.z))
        return;
    state.m_Rotation = rotation_from_euler({angles.x, angles.y, angles.z});
    legacy_copy(state.m_Filename, model_file.c_str());
    legacy_copy(state.m_SkinName, skin_file.c_str());
    auto* object = dynamic_cast<CModelObject*>(server->CreateObject(server->GetClass("CModelObject"), &state));
    if (!object)
        return;
    if (!object->Setup(duration, rotation_period, false, true))
    {
        remove(object->m_hObject);
        return;
    }
    model = object->m_hObject;
    if (create_light)
        light = make_light(position, light_color, light_min);
    server->SetObjectColor(model, light ? light_color : DVector{1, 1, 1}, 1);
    if (create_shockwave)
        CreateShockwave(position);
    if (create_mark)
    {
        scorch(position, {50, 50, 50}, light_min / 3, light_max / 3, duration * .2f, 30, 60);
        scorch(position, {255, 100, 0}, light_min / 10, light_max / 10, duration * .2f, 2, 2);
    }
    if (!sound_file.empty() && sound_radius > 0)
    {
        SoundRequest request;
        request.resource = sound_file;
        request.inner_radius = sound_radius * .1f;
        request.outer_radius = sound_radius;
        request.priority = 2;
        voice = server->PlayDetachedSound(m_hObject, request, position, sound_error);
    }
}

bool Explosion::Tick()
{
    auto* server = GetServerDE();
    server->SetNextUpdate(m_hObject, .01f);
    const float now = server->GetTime();
    if (first)
    {
        first = false;
        start = now;
        DVector position;
        if (server->GetObjectPos(m_hObject, &position) != DE_OK)
            return false;
        CreateExplosion(position);
        return true;
    }
    const float elapsed = now - start;
    if (elapsed > duration || duration == 0)
        return false;
    const auto fraction = elapsed / duration;
    const auto scale = min_scale + std::pow(fraction, .4f) * (max_scale - min_scale);
    if (server->IsObjectAlive(model))
        server->ScaleObject(model, {scale, scale, scale});
    if (auto* lamp = dynamic_cast<LightFX*>(server->HandleToObject(light)); lamp && server->IsObjectAlive(light))
    {
        const float radius_fraction = fraction <= .75f ? fraction / .75f : (1 - fraction) / .25f;
        const float alpha = .4f * std::min(1.f, (1 - fraction) / .25f);
        lamp->SetConstant(light_color, light_min + (light_max - light_min) * radius_fraction);
        const auto color = vector_scale(light_color, alpha);
        for (const auto child : {model, shockwave})
            if (server->IsObjectAlive(child))
                server->SetObjectColor(child, color, alpha);
    }
    return true;
}

DDWORD Explosion::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    const auto accepted = B2BaseClass::EngineMessageFn(message, data, reason);
    if (message == MID_PRECREATE && reason == PRECREATE_WORLDFILE && accepted && !ReadProperties())
        return 0;
    if (message == MID_UPDATE && !Tick())
        GetServerDE()->RemoveObject(m_hObject);
    return accepted;
}

DDWORD Explosion::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    auto* server = GetServerDE();
    if (message == 1000 /* MID_TRIGGER */)
        if (const auto text = server->ReadFromMessageHString(read))
        {
            const auto* value = server->GetStringData(text);
            if (value && same_name(value, "TRIGGER"))
                Explode(delay);
            server->FreeString(text);
        }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}

FireFX::~FireFX()
{
    RemoveFlames();
    remove(light);
}

void FireFX::RemoveFlames()
{
    for (auto& flame : flames_)
    {
        remove(flame.sprite);
        flame.sprite = nullptr;
    }
}

bool FireFX::Setup(HOBJECT object, float burn_time)
{
    auto* server = GetServerDE();
    if (!first_ || !server->IsObjectAlive(object) || object == m_hObject || !std::isfinite(burn_time))
        return false;
    // The original stores BurnTime but never uses it to end the fire.
    target = object;
    return BindTarget();
}

bool FireFX::BindTarget()
{
    auto* server = GetServerDE();
    for (auto& flame : flames_)
    {
        if (!server->RandomFloat(.25f, .5f, flame.duration))
            return false;
        flame.start = server->GetTime();
    }
    if (!target && !target_name_.empty())
        if (auto* objects = server->FindNamedObjects(target_name_.c_str()))
        {
            for (auto* link = objects->m_pFirstLink; link; link = link->m_pNext)
                if (dynamic_cast<CBaseCharacter*>(server->HandleToObject(link->m_hObject)))
                {
                    target = link->m_hObject;
                    break;
                }
            server->RelinquishList(objects);
        }
    if (!target)
        return true; // Keep looking for the named character, as the original did.
    DVector position;
    if (server->CreateInterObjectLink(m_hObject, target) != DE_OK ||
        server->GetObjectPos(m_hObject, &position) != DE_OK)
        return false;
    first_ = false;
    light = make_light(position, {.7f, .5f, .5f}, 250);
    return true;
}

bool FireFX::AddFlame(Flame& flame, DVector position, float now)
{
    auto* server = GetServerDE();
    DVector dims;
    DRotation rotation;
    if (server->GetObjectDims(target, &dims) != DE_OK || server->GetObjectRotation(target, &rotation) != DE_OK ||
        server->GetVelocity(target, &flame.velocity) != DE_OK)
        return false;
    dims = vector_scale(dims, .8f);
    float forward, up, right;
    if (!server->RandomFloat(-dims.x, dims.x, forward) || !server->RandomFloat(-dims.y, dims.y / 2, up) ||
        !server->RandomFloat(-dims.z, dims.z, right))
        return false;
    flame.start = now;
    flame.position = vector_subtract(vector_add(position, vector_scale(rotate_vector(rotation, {0, 0, 1}), forward)),
                                     {right, up, forward});
    ObjectCreateStruct state;
    state.m_ObjectType = OT_SPRITE;
    state.m_Flags = FLAG_VISIBLE;
    state.m_Pos = flame.position;
    state.m_Scale = {.2f, .2f, 0};
    legacy_copy(state.m_Filename, "Sprites\\gibflame.spr");
    remove(flame.sprite);
    flame.sprite = nullptr;
    if (auto* object = server->CreateObject(server->GetClass("BaseClass"), &state))
    {
        flame.sprite = server->ObjectToHandle(object);
        return server->SetVelocity(flame.sprite, &flame.velocity) == DE_OK;
    }
    return true;
}

bool FireFX::Tick()
{
    auto* server = GetServerDE();
    server->SetNextUpdate(m_hObject, .01f);
    next_flame_ = (next_flame_ + 1) % flames_.size();
    if (first_)
        return BindTarget();
    if (!server->IsObjectAlive(target))
        return false;
    const auto now = server->GetTime();
    if (dead_)
        return now <= death_time_ + 5;
    if (const auto* character = dynamic_cast<CBaseCharacter*>(server->HandleToObject(target));
        character && character->m_damage.IsDead())
    {
        RemoveFlames();
        dead_ = true;
        death_time_ = now;
        return true;
    }
    DVector position;
    if (server->GetObjectPos(target, &position) != DE_OK)
        return false;
    auto& next = flames_[next_flame_];
    if (now > next.start + next.duration && !AddFlame(next, position, now))
        return false;
    // The portable physics pass does not integrate ordinary sprites. Their initial target velocity
    // is applied from the immutable spawn pose, preserving the source's free-moving flame sprites.
    for (const auto& flame : flames_)
        if (server->IsObjectAlive(flame.sprite))
        {
            auto at = vector_add(flame.position, vector_scale(flame.velocity, now - flame.start));
            if (server->TeleportObject(flame.sprite, &at) != DE_OK)
                return false;
        }
    if (server->IsObjectAlive(light))
    {
        DVector color;
        float alpha;
        if (server->TeleportObject(light, &position) != DE_OK)
            return false;
        if (server->GetObjectColor(target, color, alpha))
        {
            color.x = std::min(color.x + .3f, 1.f);
            server->SetObjectColor(target, color, alpha);
        }
    }
    return true;
}

DDWORD FireFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    const auto accepted = B2BaseClass::EngineMessageFn(message, data, reason);
    auto* server = GetServerDE();
    if (message == MID_PRECREATE && reason == PRECREATE_WORLDFILE && accepted)
    {
        GenericProp property;
        if (server->GetPropGeneric("PlaceFireOn", &property) == DE_OK)
            target_name_ = property.m_String;
    }
    else if (message == MID_INITIALUPDATE)
        server->SetNextUpdate(m_hObject, .1f);
    else if (message == MID_UPDATE && !Tick())
        server->RemoveObject(m_hObject);
    else if (message == MID_LINKBROKEN && data == target)
        target = nullptr;
    return accepted;
}
