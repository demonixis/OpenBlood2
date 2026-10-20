// Selected ObjectDLL/Rotating.cpp rules. See m2_brush_sources.json for adaptations.
#include "Rotating.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
void Rotating::ReadProp()
{
    auto* server = BaseClass::GetServerDE();
    float rev[3]{}, up[3]{}, down[3]{}, direction[3]{};
    const char* axes[] = {"X", "Y", "Z"};
    for (unsigned i = 0; i < 3; ++i)
    {
        const std::string axis = axes[i];
        server->GetPropReal((axis + "AxisRevTime").c_str(), &rev[i]);
        server->GetPropReal((axis + "AxisSpinUpTime").c_str(), &up[i]);
        server->GetPropReal((axis + "AxisSpinDownTime").c_str(), &down[i]);
        DBOOL forward = DFALSE;
        server->GetPropBool((axis + "RotateForward").c_str(), &forward);
        direction[i] = forward ? 1.0f : -1.0f;
        for (float value : {rev[i], up[i], down[i]})
            valid = valid && std::isfinite(value) && std::abs(value) <= 1e6f;
        rev[i] = rev[i] > 0 ? 2 * std::numbers::pi_v<float> / rev[i] : 0;
        valid = valid && std::isfinite(rev[i]) && rev[i] <= 1e6f;
    }
    velocity = {rev[0], rev[1], rev[2]};
    sign = {direction[0], direction[1], direction[2]};
    spin_up = {up[0], up[1], up[2]};
    spin_down = {down[0], down[1], down[2]};
    DBOOL on = DFALSE;
    server->GetPropBool("StartOn", &on);
    if (on)
        state = Normal;
    server->GetPropReal("SoundRadius", &sound_radius);
    valid = valid && std::isfinite(sound_radius) && sound_radius >= 0 && sound_radius <= 1e6f;
    GenericProp prop;
    auto text = [&](const char* key, std::string& value)
    {
        if (server->GetPropGeneric(key, &prop) == DE_OK)
            value = prop.m_String;
    };
    text("SpinUpSound", spin_up_sound);
    text("BusySound", busy_sound);
    text("SpinDownSound", spin_down_sound);
}
void Rotating::Init(HOBJECT handle)
{
    object = handle;
}
void Rotating::StartSound(const std::string& sound, bool loop)
{
    last_sound = sound;
    sound_loop = loop && !sound.empty();
    if (!sound.empty())
        ++sound_requests;
}
void Rotating::SetNormalRotation()
{
    state = Normal;
    last_time = BaseClass::GetServerDE()->GetTime();
    StartSound(busy_sound, true);
    BaseClass::GetServerDE()->SetNextUpdate(object, .001f);
}
void Rotating::SetOff()
{
    state = Off;
    sound_loop = false;
    last_sound.clear();
}
void Rotating::SetSpinUp()
{
    state = SpinUp;
    start_time = BaseClass::GetServerDE()->GetTime();
    time_left = spin_up;
    StartSound(spin_up_sound, false);
}
void Rotating::SetSpinDown()
{
    state = SpinDown;
    start_time = BaseClass::GetServerDE()->GetTime();
    time_left = spin_down;
    StartSound(spin_down_sound, false);
}
void Rotating::UpdateNormalRotation()
{
    auto* server = BaseClass::GetServerDE();
    const auto now = server->GetTime(), dt = now - last_time;
    const auto next =
        vector_add(angles, {sign.x * velocity.x * dt, sign.y * velocity.y * dt, sign.z * velocity.z * dt});
    DVector position;
    server->GetObjectPos(object, &position);
    if (model_rotation)
    {
        // Original RotateObject rotates an ordinary model's visual pose, leaving
        // its axis-aligned physics dimensions unchanged.
        const auto rotation = rotation_from_euler(next);
        last_move = physics_vector(next) && server->SetObjectRotation(object, &rotation) == DE_OK
                        ? MoverResult::Moved
                        : MoverResult::Unavailable;
    }
    else
        last_move = server->RotateWorldModel(object, {pose_angles, next, position, position});
    if (last_move == MoverResult::Moved)
        angles = pose_angles = next;
    // Source updates its timestamp even if RotateObject fails. Do not accumulate
    // an ever larger retry after a blocked or over-budget rotation.
    last_time = now;
}
void Rotating::UpdateSpin(bool up)
{
    const float dt = BaseClass::GetServerDE()->GetFrameTime();
    const auto duration = up ? spin_up : spin_down;
    auto axis = [&](float normal, float duration, float& left, float& speed)
    {
        left -= dt;
        if (normal > 0 && left >= 0 && duration != 0)
        {
            speed = normal * (duration - left) / duration;
            if (!up)
                speed = normal - speed;
            return false;
        }
        speed = up ? normal : 0;
        return true;
    };
    const bool x = axis(normal_velocity.x, duration.x, time_left.x, velocity.x);
    const bool y = axis(normal_velocity.y, duration.y, time_left.y, velocity.y);
    const bool z = axis(normal_velocity.z, duration.z, time_left.z, velocity.z);
    UpdateNormalRotation();
    if (x && y && z)
    {
        if (up)
            SetNormalRotation();
        else
            SetOff();
    }
}
DDWORD Rotating::EngineMessageFn(LPBASECLASS owner, DDWORD message, void* data, DFLOAT reason)
{
    auto* server = BaseClass::GetServerDE();
    if (message == MID_PRECREATE && data && reason != PRECREATE_SAVEGAME)
    {
        if (reason == PRECREATE_WORLDFILE)
            ReadProp(); // String creation retains source defaults.
        GenericProp prop;
        if (server->GetPropGeneric("Rotation", &prop) == DE_OK && reason != 0)
            pose_angles = prop.m_Vec;
        const auto actual = static_cast<ObjectCreateStruct*>(data)->m_Rotation;
        const auto expected = rotation_from_euler(pose_angles);
        valid = valid && physics_vector(pose_angles) &&
                std::abs(vector_dot(expected.m_Vec, actual.m_Vec) + expected.m_Spin * actual.m_Spin) >= .99999f;
    }
    if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    if (message == MID_LOADOBJECT && data)
        Load(owner, *static_cast<HMESSAGEREAD*>(data));
    // Original InitialUpdate is skipped for INITIALUPDATE_SAVEGAME; Load restores the state.
    if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME && server->IsObjectAlive(object))
    {
        if (state != Off)
            SetNormalRotation();
        normal_velocity = velocity;
    }
    if (message == MID_UPDATE && server->IsObjectAlive(object))
    {
        const auto prior = state;
        if (state == Normal)
            UpdateNormalRotation();
        else if (state == SpinUp || state == SpinDown)
            UpdateSpin(state == SpinUp);
        if (prior != Off && server->IsObjectAlive(object))
            server->SetNextUpdate(object, .001f);
    }
    return Aggregate::EngineMessageFn(owner, message, data, reason);
}
DDWORD Rotating::ObjectMessageFn(LPBASECLASS owner, HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    auto* server = BaseClass::GetServerDE();
    if (message == 1000 && server->IsObjectAlive(object))
    {
        auto text = server->ReadFromMessageHString(read);
        const auto* raw = text ? server->GetStringData(text) : nullptr;
        auto is = [&](const char* command) { return raw && !CaseLess{}(raw, command) && !CaseLess{}(command, raw); };
        bool changed = true;
        if (state == Off && is("ON"))
            SetSpinUp();
        else if (state != Off && is("OFF"))
            SetSpinDown();
        else if (is("TOGGLE"))
        {
            if (state == Off)
                SetSpinUp();
            else
                SetSpinDown();
        }
        else
            changed = false;
        if (changed)
        {
            ++commands;
            server->SetNextUpdate(object, .001f);
        }
        if (text)
            server->FreeString(text);
    }
    return Aggregate::ObjectMessageFn(owner, sender, message, read);
}
void Rotating::Save(HMESSAGEWRITE h)
{
    // Original Rotating.cpp:685-712, then the OpenBlood2 pose angles: the replacement world-model
    // rotation poses the shell from them, and before the first move they still hold the
    // authored rotation while the source's zero-based angles do not.
    auto* s = BaseClass::GetServerDE();
    namespace F = OpenBlood2::save_fields;
    const float now = s->GetTime();
    s->WriteToLoadSaveMessageObject(h, object);
    s->WriteToMessageVector(h, &velocity);
    s->WriteToMessageVector(h, &normal_velocity);
    s->WriteToMessageVector(h, &sign);
    s->WriteToMessageVector(h, &spin_up);
    s->WriteToMessageVector(h, &spin_down);
    s->WriteToMessageVector(h, &time_left);
    s->WriteToMessageFloat(h, last_time - now);
    s->WriteToMessageFloat(h, start_time - now);
    s->WriteToMessageFloat(h, angles.x);
    s->WriteToMessageFloat(h, angles.y);
    s->WriteToMessageFloat(h, angles.z);
    s->WriteToMessageDWord(h, state);
    F::write_string(h, busy_sound);
    F::write_string(h, spin_up_sound);
    F::write_string(h, spin_down_sound);
    s->WriteToMessageFloat(h, sound_radius);
    s->WriteToMessageVector(h, &pose_angles);
}
void Rotating::Load(LPBASECLASS owner, HMESSAGEREAD h)
{
    // Original Rotating.cpp:723-750. Busy loops are not restarted (original loss).
    auto* s = BaseClass::GetServerDE();
    namespace F = OpenBlood2::save_fields;
    const float now = s->GetTime();
    HOBJECT self = nullptr;
    DVector v[6]{}, saved_angles{}, saved_pose{};
    std::string busy, up, down;
    s->ReadFromLoadSaveMessageObject(h, &self);
    for (auto& value : v)
        s->ReadFromMessageVector(h, &value);
    const float last = s->ReadFromMessageFloat(h), start = s->ReadFromMessageFloat(h);
    saved_angles.x = s->ReadFromMessageFloat(h);
    saved_angles.y = s->ReadFromMessageFloat(h);
    saved_angles.z = s->ReadFromMessageFloat(h);
    const DDWORD saved_state = s->ReadFromMessageDWord(h);
    bool ok = F::read_string(h, busy);
    ok = F::read_string(h, up) && ok;
    ok = F::read_string(h, down) && ok;
    const float radius = s->ReadFromMessageFloat(h);
    s->ReadFromMessageVector(h, &saved_pose);
    if (!ok)
        return;
    bool valid_vectors = physics_vector(saved_angles) && physics_vector(saved_pose);
    for (const auto& value : v)
        valid_vectors = valid_vectors && physics_vector(value);
    // The replacement rotation requires the pose angles to describe the restored rotation.
    bool aligned = true;
    if (!model_rotation && valid_vectors && s->IsObjectAlive(self))
    {
        DRotation actual;
        const auto expected = rotation_from_euler(saved_pose);
        aligned = s->GetObjectRotation(self, &actual) == DE_OK &&
                  std::abs(vector_dot(expected.m_Vec, actual.m_Vec) + expected.m_Spin * actual.m_Spin) >= .99999f;
    }
    if (!valid_vectors || !std::isfinite(last) || !std::isfinite(start) || saved_state > SpinDown ||
        !F::finite(radius) || radius < 0 || !s->IsObjectAlive(self) || s->HandleToObject(self) != owner || !aligned)
    {
        s->RejectPayload(h, "rotation block holds values the port cannot represent");
        return;
    }
    object = self;
    velocity = v[0];
    normal_velocity = v[1];
    sign = v[2];
    spin_up = v[3];
    spin_down = v[4];
    time_left = v[5];
    last_time = F::restored_time(last, now);
    start_time = F::restored_time(start, now);
    angles = saved_angles;
    state = static_cast<State>(saved_state);
    busy_sound = std::move(busy);
    spin_up_sound = std::move(up);
    spin_down_sound = std::move(down);
    sound_radius = radius;
    pose_angles = saved_pose;
    last_sound.clear();
    sound_loop = false;
}
