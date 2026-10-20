#include "VolumeBrush.h"
#include "PlayerObj.h"
#include "PolyGrid.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
namespace
{
    // Original VolumeBrush constructor m_dwFlags, ORed into every creation.
    constexpr DDWORD constructor_flags =
        FLAG_TOUCH_NOTIFY | FLAG_FULLPOSITIONRES | FLAG_GOTHRUWORLD | FLAG_FORCECLIENTUPDATE;
} // namespace

VolumeBrush::VolumeBrush() : B2BaseClass(OT_CONTAINER) {}
bool VolumeBrush::ReadProp(ObjectCreateStruct& state)
{
    auto real = [&](const char* name, float& value) { g_pServerDE->GetPropReal(name, &value); };
    auto boolean = [&](const char* name, bool& value)
    {
        DBOOL b = value;
        g_pServerDE->GetPropBool(name, &b);
        value = b != 0;
    };
    real("Viscosity", info.viscosity);
    info.viscosity = std::clamp(info.viscosity, 0.0f, 1.0f);
    boolean("Hidden", info.hidden);
    boolean("ShowSurface", show_surface);
    boolean("FogEnable", fog);
    boolean("Locked", locked);
    boolean("UnlockKeyRemove", remove_key);
    real("FogNearZ", fog_near);
    real("FogFarZ", fog_far);
    real("SurfaceHeight", surface_height);
    real("SurfaceAlpha", surface_alpha);
    real("Damage", damage);
    real("XScaleMin", surface.scale_min[0]);
    real("YScaleMin", surface.scale_min[1]);
    real("XScaleMax", surface.scale_max[0]);
    real("YScaleMax", surface.scale_max[1]);
    real("XScaleDuration", surface.duration[0]);
    real("YScaleDuration", surface.duration[1]);
    g_pServerDE->GetPropVector("SurfaceColor1", &surface.color1);
    g_pServerDE->GetPropVector("SurfaceColor2", &surface.color2);
    g_pServerDE->GetPropVector("Current", &info.current);
    g_pServerDE->GetPropVector("FogColor", &fog_color);
    GenericProp prop;
    if (g_pServerDE->GetPropGeneric("NumSurfacePolies", &prop) == DE_OK)
    {
        if (prop.m_Long < 0)
            return false;
        surface.polies = unsigned(prop.m_Long);
    }
    if (g_pServerDE->GetPropGeneric("Code", &prop) == DE_OK)
    {
        if (prop.m_Long < CC_NOTHING || prop.m_Long > CC_FREEFALL || prop.m_Long == CC_CRANECONTROL)
            return false;
        info.code = static_cast<ContainerCode>(prop.m_Long);
    }
    if (g_pServerDE->GetPropGeneric("DamageType", &prop) == DE_OK)
    {
        if (prop.m_Long != 0 && prop.m_Long != 4 && prop.m_Long != 6 && prop.m_Long != 7 && prop.m_Long != 8 &&
            prop.m_Long != 9 && prop.m_Long != 10 && prop.m_Long != 14)
            return false;
        damage_type = static_cast<DamageType>(prop.m_Long);
    }
    if (g_pServerDE->GetPropGeneric("SpriteSurfaceName", &prop) == DE_OK)
        sprite = prop.m_String;
    if (g_pServerDE->GetPropGeneric("UnlockKeyName", &prop) == DE_OK)
        unlock_key = prop.m_String;
    // The original FreeFall default is 9,999,999 damage/second. The 0.1s
    // dose must fit the existing typed damage-message limit of one million.
    if (!physics_vector(info.current) || !std::isfinite(damage) || damage < 0 || damage > 10000000)
        return false;
    state.m_ObjectType = OT_CONTAINER;
    state.m_ContainerCode = info.code;
    state.m_Flags |= constructor_flags;
    if (IsLiquid(info.code))
        state.m_Flags |= FLAG_RAYHIT;
    state.m_Flags &= ~(FLAG_SOLID | FLAG_GRAVITY);
    SAFE_STRCPY(state.m_Filename, state.m_Name);
    state.m_SkinName[0] = 0;
    return true;
}
DDWORD VolumeBrush::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        const auto accepted = B2BaseClass::EngineMessageFn(message, data, reason);
        if (accepted && data && reason == PRECREATE_SAVEGAME)
        {
            // Original precreate still sets the type, the BSP name and the constructor flags.
            // Departure: the saved container code is kept. The original reapplies the class
            // constructor's code, which would turn a generic VolumeBrush authored with another
            // Code into CC_NOTHING; the port's subclasses take their code from properties.
            auto& state = *static_cast<ObjectCreateStruct*>(data);
            state.m_ObjectType = OT_CONTAINER;
            state.m_Flags |= constructor_flags;
            SAFE_STRCPY(state.m_Filename, state.m_Name);
            state.m_SkinName[0] = 0;
            return 1;
        }
        return accepted && data && ReadProp(*static_cast<ObjectCreateStruct*>(data));
    }
    if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    if (message == MID_INITIALUPDATE && reason == INITIALUPDATE_SAVEGAME)
        info.object = m_hObject; // The original returns at once; Load recreates the surface.
    else if (message == MID_INITIALUPDATE)
    {
        info.object = m_hObject;
        saved_flags_ = g_pServerDE->GetObjectFlags(m_hObject);
        g_pServerDE->SetObjectUserFlags(m_hObject, (1u << 3) | (info.hidden ? 0 : FLAG_VISIBLE));
        if (show_surface)
            CreateSurface();
        // Static volume transforms remain unavailable; no pointless polling updates.
        // Fog rendering and key handshakes remain explicit pending requests.
    }
    if (message == MID_AFFECTPHYSICS && data)
        UpdatePhysics(*static_cast<ContainerPhysics*>(data));
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
void VolumeBrush::CreateSurface()
{
    ObjectCreateStruct state;
    if (g_pServerDE->GetObjectPos(m_hObject, &state.m_Pos) != DE_OK ||
        g_pServerDE->GetObjectRotation(m_hObject, &state.m_Rotation) != DE_OK ||
        g_pServerDE->GetObjectDims(m_hObject, &surface.dims) != DE_OK)
    {
        surface_error = "Volume surface pose/dimensions unavailable";
        return;
    }
    last_position_ = state.m_Pos;
    state.m_Pos.y += surface.dims.y - surface_height / 2;
    surface.dims.y = surface_height;
    surface.pan = {1 + info.current.x * .01f, 1 + info.current.y * .01f}; // Source uses Y, not Z.
    surface.alpha = surface_alpha;
    surface.sprite = sprite;
    if (!valid_polygrid(surface))
    {
        surface_error = "Invalid volume surface settings";
        return;
    }
    auto* child = dynamic_cast<PolyGrid*>(g_pServerDE->CreateObject(g_pServerDE->GetClass("PolyGrid"), &state));
    if (!child)
    {
        surface_error = "PolyGrid runtime creation failed";
        return;
    }
    surface_object = child->m_hObject;
    if (!child->Setup(surface))
        surface_error = child->error;
    g_pServerDE->SetObjectUserFlags(surface_object, (1u << 3) | (info.hidden ? 0 : FLAG_VISIBLE));
}
void VolumeBrush::UpdatePhysics(ContainerPhysics& value)
{
    if (info.hidden)
        return;
    ++physics_calls;
    if (!value.damage_only && !value.player)
    {
        // Original inverted viscosity only applies when the factor is positive.
        const auto factor = 1 - info.viscosity;
        if (factor > 0)
        {
            value.velocity = vector_scale(value.velocity, factor);
            value.acceleration = vector_scale(value.acceleration, factor);
        }
        if (IsLiquid(info.code) && value.gravity)
        {
            value.gravity = false;
            value.acceleration.y += info.gravity;
        }
        value.velocity = vector_add(value.velocity, vector_scale(info.current, value.seconds));
    }
    if (value.damage_only && damage > 0)
    {
        const auto now = g_pServerDE->GetTime();
        if (now - last_damage_time_ >= .1f || now == last_damage_time_)
        {
            last_damage_time_ = now;
            DVector position;
            g_pServerDE->GetObjectPos(value.object, &position);
            auto message = g_pServerDE->StartMessageToObject(this, value.object, damage_message);
            if (message)
            {
                if (g_pServerDE->WriteDamage(message, {{}, position, damage * .1f, damage_type, m_hObject}) == DE_OK)
                {
                    g_pServerDE->EndMessage(message);
                    ++damage_events;
                }
                else
                    g_pServerDE->EndHMessageWrite(message);
            }
        }
    }
}
void Ladder::UpdatePhysics(ContainerPhysics& value)
{
    if (!info.hidden && !value.damage_only)
        ++physics_calls;
}
void Conveyor::UpdatePhysics(ContainerPhysics& value)
{
    // Original VolumeBrushTypes.cpp: only supported characters receive the
    // server current. The shared local player already receives it in MoveMgr.
    // This override intentionally has no Hidden check, unlike the client path.
    if (value.damage_only || value.player)
        return;
    auto* character = dynamic_cast<CBaseCharacter*>(g_pServerDE->HandleToObject(value.object));
    MovementState support;
    if (character && g_pServerDE->GetMovementState(value.object, support) && support.standing)
    {
        value.velocity = vector_add(value.velocity, vector_scale(info.current, value.seconds));
        ++physics_calls;
    }
}
DDWORD VolumeBrush::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto string = g_pServerDE->ReadFromMessageHString(read);
        if (string)
        {
            auto text = g_pServerDE->GetStringData(string);
            std::string command = text ? text : "";
            for (auto& c : command)
                if (c >= 'a' && c <= 'z')
                    c = char(c - 'a' + 'A');
            if (command == "ON" || command == "OFF" || command == "TOGGLE")
            {
                info.hidden = command == "OFF" || (command == "TOGGLE" && !info.hidden);
                g_pServerDE->SetObjectFlags(m_hObject, info.hidden ? saved_flags_ & ~FLAG_VISIBLE : saved_flags_);
                g_pServerDE->SetObjectUserFlags(m_hObject, (1u << 3) | (info.hidden ? 0 : FLAG_VISIBLE));
                if (g_pServerDE->IsObjectAlive(surface_object))
                    g_pServerDE->SetObjectUserFlags(surface_object, (1u << 3) | (info.hidden ? 0 : FLAG_VISIBLE));
                ++triggers;
            }
            g_pServerDE->FreeString(string);
        }
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
void VolumeBrush::Save(HMESSAGEWRITE h)
{
    // Original VolumeBrush.cpp:758-806. The surface handle is not saved (Load recreates it).
    // The port keeps the authored viscosity, not the original's internally inverted value.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    const float now = s->GetTime();
    s->WriteToMessageFloat(h, info.viscosity);
    s->WriteToMessageByte(h, static_cast<DBYTE>(info.code));
    s->WriteToMessageVector(h, &info.current);
    s->WriteToMessageFloat(h, damage);
    s->WriteToMessageByte(h, static_cast<DBYTE>(damage_type));
    s->WriteToMessageFloat(h, surface_height);
    s->WriteToMessageByte(h, show_surface);
    s->WriteToMessageVector(h, &surface.color1);
    s->WriteToMessageVector(h, &surface.color2);
    s->WriteToMessageVector(h, &last_position_);
    s->WriteToMessageByte(h, info.hidden);
    s->WriteToMessageFloat(h, info.gravity);
    s->WriteToMessageFloat(h, last_damage_time_ - now);
    s->WriteToMessageFloat(h, surface.scale_min[0]);
    s->WriteToMessageFloat(h, surface.scale_max[0]);
    s->WriteToMessageFloat(h, surface.scale_min[1]);
    s->WriteToMessageFloat(h, surface.scale_max[1]);
    s->WriteToMessageFloat(h, surface.duration[0]);
    s->WriteToMessageFloat(h, surface.duration[1]);
    F::write_string(h, sprite);
    s->WriteToMessageDWord(h, constructor_flags);
    s->WriteToMessageDWord(h, saved_flags_);
    s->WriteToMessageByte(h, locked);
    s->WriteToMessageByte(h, remove_key);
    F::write_string(h, unlock_key);
    s->WriteToMessageByte(h, fog);
    s->WriteToMessageFloat(h, fog_far);
    s->WriteToMessageFloat(h, fog_near);
    s->WriteToMessageVector(h, &fog_color);
    s->WriteToMessageFloat(h, surface_alpha);
    s->WriteToMessageDWord(h, surface.polies);
}
void VolumeBrush::Load(HMESSAGEREAD h)
{
    // Original VolumeBrush.cpp:815-878, decoded and checked before any member changes.
    // Departure (decision 4): the original forgets m_hPlayerClass here and then routes the
    // player through the non-player branch. The port's engine marks the player in
    // ContainerPhysics, so a restored volume still recognizes it.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    const float now = s->GetTime();
    ContainerInfo i;
    PolyGridSettings grid;
    std::string saved_sprite, saved_key;
    DVector position{}, saved_fog_color{};
    bool show = false, saved_locked = false, saved_remove = false, saved_fog = false;
    i.viscosity = s->ReadFromMessageFloat(h);
    const DBYTE code = s->ReadFromMessageByte(h);
    s->ReadFromMessageVector(h, &i.current);
    const float saved_damage = s->ReadFromMessageFloat(h);
    const DBYTE type = s->ReadFromMessageByte(h);
    const float height = s->ReadFromMessageFloat(h);
    bool ok = F::read_bool(h, show);
    s->ReadFromMessageVector(h, &grid.color1);
    s->ReadFromMessageVector(h, &grid.color2);
    s->ReadFromMessageVector(h, &position);
    ok = F::read_bool(h, i.hidden) && ok;
    i.gravity = s->ReadFromMessageFloat(h);
    const float last_damage = s->ReadFromMessageFloat(h);
    grid.scale_min[0] = s->ReadFromMessageFloat(h);
    grid.scale_max[0] = s->ReadFromMessageFloat(h);
    grid.scale_min[1] = s->ReadFromMessageFloat(h);
    grid.scale_max[1] = s->ReadFromMessageFloat(h);
    grid.duration[0] = s->ReadFromMessageFloat(h);
    grid.duration[1] = s->ReadFromMessageFloat(h);
    ok = F::read_string(h, saved_sprite) && ok;
    const DDWORD flags = s->ReadFromMessageDWord(h);
    const DDWORD saved_object_flags = s->ReadFromMessageDWord(h);
    ok = F::read_bool(h, saved_locked) && ok;
    ok = F::read_bool(h, saved_remove) && ok;
    ok = F::read_string(h, saved_key) && ok;
    ok = F::read_bool(h, saved_fog) && ok;
    const float far_z = s->ReadFromMessageFloat(h), near_z = s->ReadFromMessageFloat(h);
    s->ReadFromMessageVector(h, &saved_fog_color);
    const float alpha = s->ReadFromMessageFloat(h);
    grid.polies = s->ReadFromMessageDWord(h);
    if (!ok)
        return;
    for (float value :
         {i.viscosity, saved_damage, height, i.gravity, last_damage, far_z, near_z, alpha, grid.scale_min[0],
          grid.scale_max[0], grid.scale_min[1], grid.scale_max[1], grid.duration[0], grid.duration[1]})
        ok = ok && std::isfinite(value);
    for (const auto& value : {i.current, grid.color1, grid.color2, position, saved_fog_color})
        ok = ok && physics_vector(value);
    const bool valid_type =
        type == 0 || type == 4 || type == 6 || type == 7 || type == 8 || type == 9 || type == 10 || type == 14;
    if (!ok || !(i.viscosity >= 0 && i.viscosity <= 1) || code > CC_MINEFIELD || code == CC_CRANECONTROL ||
        !valid_type || saved_damage < 0 || saved_damage > 10000000 || flags != constructor_flags ||
        grid.polies > 0x7FFFFFFF)
    {
        s->RejectPayload(h, "volume block holds values the port cannot represent");
        return;
    }
    info.viscosity = i.viscosity;
    info.code = static_cast<ContainerCode>(code);
    info.current = i.current;
    info.hidden = i.hidden;
    info.gravity = i.gravity;
    damage = saved_damage;
    damage_type = static_cast<DamageType>(type);
    surface_height = height;
    show_surface = show;
    for (unsigned axis = 0; axis < 2; ++axis)
    {
        surface.scale_min[axis] = grid.scale_min[axis];
        surface.scale_max[axis] = grid.scale_max[axis];
        surface.duration[axis] = grid.duration[axis];
    }
    surface.color1 = grid.color1;
    surface.color2 = grid.color2;
    surface.polies = grid.polies;
    last_position_ = position;
    last_damage_time_ = F::restored_time(last_damage, now);
    sprite = std::move(saved_sprite);
    saved_flags_ = saved_object_flags;
    locked = saved_locked;
    remove_key = saved_remove;
    unlock_key = std::move(saved_key);
    fog = saved_fog;
    fog_far = far_z;
    fog_near = near_z;
    fog_color = saved_fog_color;
    surface_alpha = alpha;
    // Load tail: recreate the always-load surface PolyGrid, then reset both user flags.
    if (show_surface)
        CreateSurface();
    const DDWORD user = (1u << 3) | (info.hidden ? 0 : FLAG_VISIBLE);
    s->SetObjectUserFlags(m_hObject, user);
    if (s->IsObjectAlive(surface_object))
        s->SetObjectUserFlags(surface_object, user);
}
