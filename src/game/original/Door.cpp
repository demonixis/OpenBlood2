#include "Door.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <numbers>
using namespace OpenBlood2;

bool Door::ReadProp(ObjectCreateStruct& create)
{
    auto real = [&](const char* key, float& value) { g_pServerDE->GetPropReal(key, &value); };
    auto boolean = [&](const char* key, bool& value)
    {
        DBOOL out = value;
        g_pServerDE->GetPropBool(key, &out);
        value = out != 0;
    };
    auto string = [&](const char* key, std::string& value)
    {
        GenericProp prop;
        if (g_pServerDE->GetPropGeneric(key, &prop) == DE_OK)
            value = prop.m_String;
    };
    real("Speed", speed);
    real("ClosingSpeed", closing_speed);
    real("MoveDist", move_dist);
    real("WaitTime", wait_time);
    real("OpenDelay", open_delay);
    real("SoundRadius", sound_radius);
    boolean("BoxPhysics", box_physics);
    boolean("TriggerClosed", trigger_closed);
    boolean("StartOpen", start_open);
    boolean("RemainsOpen", remains_open);
    boolean("SelfTrigger", self_trigger);
    boolean("FireThrough", fire_through);
    boolean("TouchActivate", touch_activate);
    boolean("PlayerActivate", player_activate);
    boolean("AIActivate", ai_activate);
    boolean("ObjectActivate", object_activate);
    boolean("TriggerRelayActivate", relay_activate);
    boolean("NamedObjectActivate", named_activate);
    boolean("Locked", locked);
    string("OpenBusySound", open_busy);
    string("OpenStopSound", open_stop);
    string("CloseBusySound", close_busy);
    string("CloseStopSound", close_stop);
    string("PortalName", portal);
    string("ActivationObjectName", activation_name);
    string("LockedSound", locked_sound);
    string("UnlockedSound", unlocked_sound);
    string("UnlockKeyName", key_name);
    g_pServerDE->GetPropVector("MoveDir", &move_dir);
    g_pServerDE->GetPropVector("TriggerDims", &trigger_dims);
    GenericProp prop;
    if (g_pServerDE->GetPropGeneric("Waveform", &prop) != DE_OK || prop.m_Long < 0 || prop.m_Long > 3)
        return false;
    waveform = static_cast<unsigned>(prop.m_Long);
    // Authored retail data includes WaitTime=-1 and ClosingSpeed=-1. Preserve
    // the source arithmetic, not an invented "never close" sentinel.
    for (float v : {speed, closing_speed, move_dist, wait_time, open_delay, sound_radius})
        if (!std::isfinite(v) || std::abs(v) > 1000000)
            return false;
    if (open_delay < 0 || sound_radius < 0)
        return false;
    if (!physics_vector(move_dir) || !physics_vector(trigger_dims) || trigger_dims.x < 0 || trigger_dims.y < 0 ||
        trigger_dims.z < 0)
        return false;
    const auto length = std::sqrt(vector_dot(move_dir, move_dir));
    if (move_dist != 0 && length < 1e-6f)
        return false;
    move_dir = length > 0 ? vector_scale(move_dir, 1 / length) : DVector{};
    if (closing_speed == 0)
        closing_speed = speed;
    if (open_delay == 0)
        open_delay = .01f;
    create.m_ObjectType = OT_WORLDMODEL;
    SAFE_STRCPY(create.m_Filename, create.m_Name);
    create.m_SkinName[0] = 0;
    create.m_Flags |= FLAG_GOTHRUWORLD | FLAG_FULLPOSITIONRES | FLAG_MODELGOURAUDSHADE | FLAG_DONTFOLLOWSTANDING;
    if (box_physics)
        create.m_Flags |= FLAG_BOXPHYSICS;
    else
        create.m_Flags &= ~FLAG_BOXPHYSICS;
    return true;
}
float Door::GetWaveValue(float speed, float percent, unsigned wave)
{
    const auto pi = std::numbers::pi_v<float>;
    switch (wave)
    {
        case 1:
            return speed * std::sin(percent * pi) + speed * .1f;
        case 2:
            return speed * std::cos(percent * pi / 2) + speed * .1f;
        case 3:
            return speed * std::sin(percent * pi / 2) + speed * .1f;
        default:
            return speed;
    }
}
void Door::StartSound(const std::string& sound, bool loop)
{
    // Record requests without claiming a connected audio backend.
    last_sound = sound;
    sound_loop = loop && !sound.empty();
    if (!sound.empty())
        ++sound_requests;
}
void Door::SetOpen(bool sound)
{
    if (sound)
        StartSound(open_stop, false);
    state = Open;
    if (trigger_closed || remains_open)
        g_pServerDE->SetNextUpdate(m_hObject, 0);
    else
        open_time = wait_time;
}
void Door::SetClosed(bool sound)
{
    if (sound)
        StartSound(close_stop, false);
    g_pServerDE->SetNextUpdate(m_hObject, 0);
    state = Closed;
    portal_open = false;
}
void Door::SetOpening()
{
    g_pServerDE->SetNextUpdate(m_hObject, open_delay);
    started_moving = false;
    state = Opening;
}
void Door::SetClosing()
{
    g_pServerDE->SetNextUpdate(m_hObject, .01f);
    state = Closing;
    StartSound(close_busy, true);
}
void Door::Move(bool opening)
{
    if (opening && !started_moving)
    {
        started_moving = true;
        StartSound(open_busy, true);
        portal_open = true;
    }
    DVector position;
    g_pServerDE->GetObjectPos(m_hObject, &position);
    const auto target = opening ? open_pos : closed_pos;
    const auto direction = vector_subtract(target, position);
    const auto distance = std::sqrt(vector_dot(direction, direction));
    // Guard the original zero-distance division/normalization.
    const auto percent = move_dist != 0 ? 1 - distance / move_dist : 1;
    const auto travel = GetWaveValue(opening ? speed : closing_speed, percent, waveform) * g_pServerDE->GetFrameTime();
    const bool finish = distance == 0 || travel > distance;
    const auto destination = finish ? target : vector_add(position, vector_scale(direction, travel / distance));
    last_move = g_pServerDE->MoveWorldModel(m_hObject, destination);
    // Unlike the old unchecked MoveObject call, a rejected move cannot report
    // a reached endpoint. A blocked door retries without crushing damage.
    if (last_move == MoverResult::Moved && finish)
    {
        if (opening)
            SetOpen();
        else
            SetClosed();
    }
}
DDWORD Door::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        const auto accepted = B2BaseClass::EngineMessageFn(message, data, reason);
        // Save payloads are not implemented; do not instantiate fake restores.
        return accepted && reason != 3 && data && ReadProp(*static_cast<ObjectCreateStruct*>(data));
    }
    if (message == MID_INITIALUPDATE)
    {
        g_pServerDE->GetObjectPos(m_hObject, &closed_pos);
        open_pos = vector_add(closed_pos, vector_scale(move_dir, move_dist));
        if (!physics_vector(open_pos))
            g_pServerDE->RemoveObject(m_hObject);
        else if (start_open)
        {
            std::swap(open_pos, closed_pos);
            move_dir = vector_scale(move_dir, -1);
            last_move = g_pServerDE->MoveWorldModel(m_hObject, closed_pos);
            if (last_move != MoverResult::Moved)
                g_pServerDE->RemoveObject(m_hObject);
        }
        SetClosed(false);
        g_pServerDE->SetNextUpdate(m_hObject, .01f);
        g_pServerDE->SetObjectUserFlags(m_hObject, 1u << 3);
    }
    if (message == MID_UPDATE)
    {
        g_pServerDE->SetNextUpdate(m_hObject, .01f);
        if (state == Opening || state == Closing)
            Move(state == Opening);
        else if (state == Open)
        {
            open_time -= g_pServerDE->GetFrameTime();
            if (open_time <= 0)
            {
                open_time = 0;
                SetClosing();
            }
        }
    }
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD Door::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto text = g_pServerDE->ReadFromMessageHString(read);
        if (text)
        {
            auto raw = g_pServerDE->GetStringData(text);
            std::string command = raw ? raw : "";
            for (auto& c : command)
                if (c >= 'a' && c <= 'z')
                    c = char(c - 'a' + 'A');
            if (command == "TRIGGER")
            {
                ++triggers;
                if (state == Closed || state == Closing)
                    SetOpening();
                else if (state == Open)
                {
                    if (trigger_closed)
                        SetClosing();
                    else
                        SetOpen(false);
                }
            }
            else if (command == "TRIGGERCLOSE")
            {
                ++triggers;
                if (state == Open || state == Opening)
                    SetClosing();
            }
            else if (command == "LOCK" || command == "UNLOCK")
                pending_command = command; // Original forwards to its child Trigger, not a door lock.
            g_pServerDE->FreeString(text);
        }
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
