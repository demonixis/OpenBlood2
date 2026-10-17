// Selected original AI_Shared.CPP / Gib.cpp. See DEATH_EFFECT_CONTRACTS.md.
#include "Gib.h"
#include "AI_Mgr.h"
#include "ParticleExplosionFX.h"
#include "engine/collision.h"
#include "game/game_settings.h"
#include <cmath>
#include <cstring>
#include <numbers>
using namespace OpenBlood2;
bool HideLimb(HOBJECT object, int node, std::string& error)
{
    auto* s = BaseClass::GetServerDE();
    if (node != 1 && node != 4 && node != 7 && node != 10 && node != 14)
        return false;
    DBOOL hidden = false;
    if (!s || s->GetModelNodeHideStatus(object, szNodes[node], &hidden) != DE_OK || hidden)
        return false;
    if (!hide_character_node(object, node, true, error))
        return false;
    if (node == 1)
        return hide_character_node(object, 0, true, error);
    // Includes the source's extra +3 node for arms; do not replace with a guessed hierarchy.
    for (int i = 1; i <= 3; ++i)
        if (!hide_character_node(object, node + i, true, error))
            return false;
    return true;
}
HOBJECT CreateLimb(HOBJECT owner, int node, DVector direction, std::string& error)
{
    auto* s = BaseClass::GetServerDE();
    error.clear();
    ObjectCreateStruct state;
    state.m_ObjectType = OT_MODEL;
    std::string model, skin;
    const char* clip = node == 1                    ? "limb_head"
                       : (node == 4 || node == 7)   ? "limb_arm"
                       : (node == 10 || node == 14) ? "limb_leg"
                                                    : nullptr;
    if (!s || !clip || !s->IsObjectAlive(owner) || !physics_vector(direction) ||
        s->QueryModelNodeTransform(owner, szNodes[node], &state.m_Pos, &state.m_Rotation) !=
            ServerDE::NodeQuery::Found ||
        s->GetObjectRotation(owner, &state.m_Rotation) != DE_OK || !s->GetModelFilenames(owner, model, skin))
    {
        error = "Limb source/node/model unavailable";
        return nullptr;
    }
    if (model.size() >= sizeof(state.m_Filename) || skin.size() >= sizeof(state.m_SkinName))
    {
        error = "Limb model path exceeds buffer";
        return nullptr;
    }
    const auto animation = s->GetAnimIndex(owner, clip);
    DVector dims;
    if (animation == UINT32_MAX || s->GetModelAnimUserDims(owner, &dims, animation) != DE_OK)
    {
        error = "Limb animation unavailable";
        return nullptr;
    }
    direction.y = 1;
    float speed;
    if (!s->RandomFloat(200, 500, speed))
        return nullptr;
    direction = vector_scale(direction, speed / std::sqrt(vector_dot(direction, direction)));
    std::strcpy(state.m_Filename, model.c_str());
    std::strcpy(state.m_SkinName, skin.c_str());
    std::strcpy(state.m_Name, "Original detached limb");
    auto* gib = dynamic_cast<CGib*>(s->CreateObject(s->GetClass("CGib"), &state));
    if (!gib)
    {
        error = "Limb creation failed";
        return nullptr;
    }
    const auto h = gib->m_hObject;
    gib->limb = node;
    gib->kickable = node == 1;
    s->SetModelAnimation(h, animation);
    s->SetModelLooping(h, false);
    bool ok = gib->initialized && s->GetModelAnimation(h) == animation;
    for (int i = 0; i < int(NUM_ALL_NODES) && ok; ++i)
    {
        if (node == 1 && i <= 1)
            i = 2;
        else if ((node == 4 || node == 7) && i == 7)
            i = 9;
        else if ((node == 10 || node == 14) && i == 10)
            i = 14;
        ok = hide_character_node(h, i, true, error);
    }
    // Animation nodes can lie closer to the floor/walls than the limb's raw
    // clip box permits. Reuse the bounded nonsolid-body placement policy before
    // the first physics step; never leave a penetrating body in the session.
    DVector anchor, anchor_half;
    if (!ok || s->SetObjectDims(h, &dims) != DE_OK || s->GetObjectPos(owner, &anchor) != DE_OK ||
        s->GetObjectDims(owner, &anchor_half) != DE_OK ||
        !s->PlaceNonSolidModel(h, state.m_Pos, anchor, anchor_half, gib->placement) ||
        s->SetVelocity(h, &direction) != DE_OK)
    {
        s->RemoveObject(h);
        if (error.empty())
            error = "Limb setup failed";
        return nullptr;
    }
    return h;
}
CGib::CGib() : B2BaseClass(OT_MODEL)
{
    AddAggregate(&damage);
    RandomRotation();
}
void CGib::RandomRotation()
{
    auto* s = GetServerDE();
    const float circle = 2 * std::numbers::pi_v<float>;
    if (s)
    {
        s->RandomFloat(-circle, circle, angular_velocity.x);
        s->RandomFloat(-circle, circle, angular_velocity.y);
        s->RandomFloat(-circle, circle, angular_velocity.z);
        angular_velocity = vector_scale(angular_velocity, 1.f / 3);
    }
}
DDWORD CGib::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (msg == MID_PRECREATE)
    {
        auto ret = B2BaseClass::EngineMessageFn(msg, data, reason);
        auto* state = static_cast<ObjectCreateStruct*>(data);
        if (!ret || !state || reason != PRECREATE_NORMAL || !state->m_Filename[0])
            return 0;
        state->m_Flags = FLAG_VISIBLE | FLAG_RAYHIT | FLAG_GRAVITY | FLAG_TOUCH_NOTIFY;
        state->m_ObjectType = OT_MODEL;
        return ret;
    }
    if (msg == MID_INITIALUPDATE)
    {
        initialized = damage.Init(m_hObject) && s->ConfigureCharacterBody(m_hObject, 3, 9);
        damage.SetHitPoints(10);
        damage.m_fMaxHitPoints = 10;
        damage.SetArmorPoints(0);
        damage.m_fMass = 3;
        damage.apply_damage_physics = true;
        s->SetModelLooping(m_hObject, false);
        s->SetObjectUserFlags(m_hObject, s->GetObjectUserFlags(m_hObject) | (100u << 24));
        if (GetGameType() != GAMETYPE_SINGLE)
            remove_time = s->GetTime() + 29.9f;
        s->SetNextUpdate(m_hObject, .1f);
    }
    else if (msg == MID_UPDATE && (!initialized || damage.IsDead() || !Update()))
        s->RemoveObject(m_hObject);
    else if (msg == MID_TOUCHNOTIFY)
        Touch(static_cast<HOBJECT>(data));
    return B2BaseClass::EngineMessageFn(msg, data, reason);
}
bool CGib::Update()
{
    auto* s = GetServerDE();
    if (remove_time && remove_time < s->GetTime())
        return false;
    DVector velocity, dims;
    if (s->GetVelocity(m_hObject, &velocity) != DE_OK)
        return false;
    if (first_update)
    {
        if (s->GetObjectDims(m_hObject, &dims) != DE_OK)
            return false;
        if (dims.y < dims.x * .7f)
            axis = 2;
        if (dims.x < dims.y * .7f)
            axis = 1;
        if (dims.z < dims.y * .7f)
            axis = 3;
        first_update = false;
    }
    if (s->GetObjectPos(m_hObject, &last_position) != DE_OK)
        return false;
    if (add_blood_spurt)
    {
        OpenBlood2::BloodSpurtSettings spurt;
        spurt.position = last_position;
        // Server sends .1 * velocity; original client SFXMgr multiplies it by .6.
        spurt.min_velocity = spurt.max_velocity = vector_scale(vector_scale(velocity, .1f), .6f);
        spurt.texture = 0; // Client selection, independent of the server gameplay random stream.
        spurt_event = s->PublishBloodSpurt(m_hObject, spurt, spurt_error);
        add_blood_spurt = false;
    }
    const float dt = s->GetFrameTime();
    rotate_time -= dt;
    constexpr float pi = std::numbers::pi_v<float>;
    if (vector_dot(velocity, velocity) <= 50 || rotate_time <= 0)
    {
        angular_velocity = {};
        auto snap = [](float a)
        {
            constexpr float p = std::numbers::pi_v<float>;
            return a > p / 2 && a < p * 1.5f ? p : 0.f;
        };
        if (axis == 1)
            angles.y = snap(angles.y);
        if (axis == 2)
            angles.x = snap(angles.x);
        if (axis == 3)
            angles.z = snap(angles.z);
        s->SetNextUpdate(m_hObject, GetGameType() != GAMETYPE_SINGLE ? .01f : 0.f);
    }
    else
    {
        angles = vector_add(angles, vector_scale(angular_velocity, dt));
        if (angles.x > 2 * pi)
            angles.x -= 2 * pi;
        if (angles.y > 2 * pi)
            angles.y -= 2 * pi;
        if (angles.z > 2 * pi)
            angles.z -= 2 * pi;
        s->SetNextUpdate(m_hObject, .01f);
    }
    auto rotation = rotation_from_euler(angles);
    s->SetObjectRotation(m_hObject, &rotation);
    return true;
}
void CGib::BounceSound()
{
    int variant;
    auto* s = GetServerDE();
    if (!s->RandomInt(1, 3, variant))
        return;
    SoundRequest request;
    request.speaker = m_hObject;
    request.resource = "sounds/gibs/flesh/gib_impact" + std::to_string(variant) + ".wav";
    request.outer_radius = 1000;
    request.inner_radius = 100;
    s->PlayCharacterSound(m_hObject, request, false, sound_error);
}
void CGib::Touch(HOBJECT other)
{
    auto* s = GetServerDE();
    DVector v;
    if (kickable && other && s->IsKindOf(s->GetObjectClass(other), s->GetClass("CBaseCharacter")) &&
        s->GetVelocity(other, &v) == DE_OK && vector_dot(v, v) > 25)
    {
        v.y += 300;
        v = vector_scale(v, 2);
        s->SetVelocity(m_hObject, &v);
        bounces = 2;
        RandomRotation();
        BounceSound();
        rotate_time = 1;
        s->SetNextUpdate(m_hObject, .01f);
        ++kicks;
        return;
    }
    if (other && !(s->GetObjectFlags(other) & FLAG_SOLID))
        return;
    if (s->GetVelocity(m_hObject, &v) != DE_OK || int(v.x) + int(v.z) == 0 || --bounces < 0)
        return;
    SegmentHit hit;
    if (s->IntersectSegment(last_position, vector_add(last_position, vector_scale(v, 1.1f)), m_hObject, hit, false,
                            SegmentObjects::SolidOnly) != SegmentResult::Hit)
        return;
    const float r = vector_dot(v, hit.normal) * .3f;
    if (r <= -100)
    {
        BounceSound();
        v = vector_subtract(v, vector_scale(hit.normal, r));
        RandomRotation();
    }
    v = vector_scale(v, .5f);
    s->SetVelocity(m_hObject, &v);
    rotate_time = 1;
    add_blood_spurt = true;
    ++impacts;
    s->SetNextUpdate(m_hObject, .01f);
}
DDWORD CGib::ObjectMessageFn(HOBJECT sender, DDWORD msg, HMESSAGEREAD read)
{
    const auto result = B2BaseClass::ObjectMessageFn(sender, msg, read);
    if (msg == damage_message && damage.IsDead())
    {
        // The limb's original GIB_SMALL descendants remove themselves on first update.
        // Do not invent durable extra fragments for that branch.
        GetServerDE()->RemoveObject(m_hObject);
    }
    return result;
}
