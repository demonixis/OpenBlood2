// Selected VolumeBrushTypes.cpp CraneControl and WreckingBall.cpp.
#include "CraneObjects.h"
#include "CameraObj.h"
#include "LegacyLines.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <numbers>
using namespace OpenBlood2;

namespace
{
    constexpr float chain_length = 580, crane_limit = .6f * std::numbers::pi_v<float>;

    HOBJECT first_named(ServerDE& server, const char* name)
    {
        auto* list = server.FindNamedObjects(name);
        if (!list)
            return nullptr;
        const auto handle = list->m_pFirstLink ? list->m_pFirstLink->m_hObject : nullptr;
        server.RelinquishList(list);
        return handle;
    }

    DVector euler_angles(const DRotation& rotation)
    {
        const auto forward = rotate_vector(rotation, {0, 0, 1});
        DVector angles{std::atan2(-forward.y, std::hypot(forward.x, forward.z)),
                       std::atan2(forward.x, forward.z), 0};
        const auto untilted = rotation_from_euler(angles);
        const auto right = rotate_vector(rotation, {1, 0, 0});
        angles.z = std::atan2(vector_dot(right, rotate_vector(untilted, {0, 1, 0})),
                              vector_dot(right, rotate_vector(untilted, {1, 0, 0})));
        return angles;
    }
}

CraneControl::~CraneControl()
{
    if (auto* server = GetServerDE(); server && camera && server->IsObjectAlive(camera))
        server->RemoveObject(camera);
}

void CraneControl::UpdatePhysics(ContainerPhysics& physics)
{
    // The authoritative volume pass runs once per frame for both local and
    // remote players. Generic body substeps cannot repeat the source yaw increment.
    if (!physics.damage_only)
        return;
    auto* server = GetServerDE();
    auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(physics.object));
    if (!player)
        return;
    ++physics_calls;
    if (!server->IsObjectAlive(camera))
    {
        ObjectCreateStruct create;
        SAFE_STRCPY(create.m_Name, "cranecam");
        if (server->GetObjectPos(m_hObject, &create.m_Pos) == DE_OK)
            if (auto* actor = server->CreateObject(server->GetClass("CameraObj"), &create))
            {
                camera = actor->m_hObject;
                server->SetObjectFlags(camera, 0);
            }
    }
    if (!server->IsObjectAlive(crane_))
        crane_ = first_named(*server, "crane");
    if (!server->IsObjectAlive(ball_))
        ball_ = first_named(*server, "WreckingBall");
    if (!crane_ || !ball_)
    {
        control_error = "CraneControl requires the named crane and WreckingBall";
        return;
    }
    if (auto* view = dynamic_cast<CameraObj*>(server->HandleToObject(camera)); view && view->GetLinkObject() != ball_)
    {
        if (view->SetLinkObject(ball_))
            server->SetNextUpdate(camera, .01f);
    }
    // The original lock query and the player's camera selection are disabled.
    DVector head;
    ContainerState containers;
    if (server->GetObjectPos(player->m_hObject, &head) != DE_OK)
        return;
    head.y += player->eye_level;
    if (!server->GetContainers(player->m_hObject, head, containers) || containers.head.object != m_hObject ||
        containers.head.code != CC_CRANECONTROL)
        return;

    const auto next = std::clamp(yaw + .03f * player->crane_turn, -crane_limit, crane_limit);
    const auto target = rotation_from_euler({0, next, 0});
    if (server->GetObjectType(crane_) == OT_WORLDMODEL)
    {
        DVector position;
        DRotation rotation;
        if (server->GetObjectPos(crane_, &position) != DE_OK || server->GetObjectRotation(crane_, &rotation) != DE_OK ||
            server->RotateWorldModel(crane_, {euler_angles(rotation), {0, next, 0}, position, position}) !=
                MoverResult::Moved)
        {
            control_error = "Crane world-model rotation blocked";
            return;
        }
    }
    else if (server->SetObjectRotation(crane_, &target) != DE_OK)
    {
        control_error = "Crane rotation unavailable";
        return;
    }
    yaw = next;
    server->SetObjectUserFlags(ball_, static_cast<DDWORD>(static_cast<std::int32_t>(yaw)));
    control_error.clear();
}

WreckingBall::WreckingBall() : B2BaseClass(OT_MODEL)
{
    AddAggregate(&damage);
}

DDWORD WreckingBall::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (msg == MID_PRECREATE)
    {
        if (!data || !B2BaseClass::EngineMessageFn(msg, data, reason))
            return 0;
        auto& state = *static_cast<ObjectCreateStruct*>(data);
        SAFE_STRCPY(state.m_Filename, "models/worldobjects/wreckingball.abc");
        SAFE_STRCPY(state.m_SkinName, "skins/worldobjects/wreckingball.dtx");
        SAFE_STRCPY(state.m_Name, "WreckingBall");
        state.m_NextUpdate = .01f;
        state.m_Flags = FLAG_VISIBLE | FLAG_SOLID | FLAG_SHADOW | FLAG_TOUCH_NOTIFY | FLAG_MODELGOURAUDSHADE;
        return 1;
    }
    if (msg == MID_INITIALUPDATE)
    {
        damage.m_fMass = 5000;
        damage.apply_damage_physics = true;
        DVector dims;
        if (!damage.Init(m_hObject) || server->GetModelAnimUserDims(m_hObject, &dims, 0) != DE_OK ||
            server->SetObjectDims(m_hObject, &dims) != DE_OK ||
            !server->ConfigureModelBody(m_hObject, {damage.m_fMass, 0}) ||
            !server->SetBlockingPriority(m_hObject, 50))
        {
            server->RemoveObject(m_hObject);
            return 0;
        }
        server->SetNextUpdate(m_hObject, .01f);
    }
    else if (msg == MID_UPDATE && !Update())
        server->RemoveObject(m_hObject);
    else if (msg == MID_TOUCHNOTIFY)
        DamageNearbyModels();
    return B2BaseClass::EngineMessageFn(msg, data, reason);
}

bool WreckingBall::ReadAnchor(DVector& attach) const
{
    auto* server = GetServerDE();
    DRotation rotation;
    if (!server->IsObjectAlive(crane) || server->GetObjectPos(crane, &attach) != DE_OK ||
        server->GetObjectRotation(crane, &rotation) != DE_OK)
        return false;
    auto forward = rotate_vector(rotation, {0, 0, 1});
    forward.x = -forward.x; // Original mirrored X component on the beam.
    attach = vector_add(attach, vector_add(vector_scale(forward, 180), rotate_vector(rotation, {0, 330, 0})));
    return true;
}

bool WreckingBall::Update()
{
    auto* server = GetServerDE();
    DVector attach, position;
    if (first_)
        crane = first_named(*server, "crane");
    if (!ReadAnchor(attach) || server->GetObjectPos(m_hObject, &position) != DE_OK)
    {
        error = "WreckingBall has no live crane anchor";
        return false;
    }
    server->SetNextUpdate(m_hObject, .01f);
    if (first_)
    {
        first_ = false;
        position = vector_add(attach, {0, -chain_length, 0});
        if (server->TeleportObject(m_hObject, &position) != DE_OK ||
            server->ResolveInitialWorldOverlap(m_hObject) != DE_OK ||
            server->GetObjectPos(m_hObject, &position) != DE_OK)
        {
            error = "WreckingBall starting pose blocked";
            return false;
        }
        ObjectCreateStruct create;
        create.m_Pos = attach;
        align_rotation(vector_subtract(position, attach), create.m_Rotation);
        if (auto* line = dynamic_cast<CClientCastLineSFX*>(
                server->CreateObject(server->GetClass("CClientCastLineSFX"), &create)))
        {
            if (line->Setup({}, {}, 1, 1, m_hObject))
                chain = line->m_hObject;
            else
                server->RemoveObject(line->m_hObject);
        }
        anchor = attach;
    }

    // Source derives vertical height from horizontal displacement. Bound the
    // horizontal length before the square root when a moving anchor outruns the ball.
    auto offset = vector_subtract(position, attach);
    const auto horizontal = std::hypot(offset.x, offset.z);
    if (horizontal > chain_length)
    {
        offset.x *= chain_length / horizontal;
        offset.z *= chain_length / horizontal;
    }
    offset.y = -std::sqrt(std::max(0.f, chain_length * chain_length - offset.x * offset.x - offset.z * offset.z));
    position = vector_add(attach, offset);
    if (server->MoveObject(m_hObject, &position) != DE_OK)
    {
        error = "WreckingBall movement blocked";
        return false;
    }
    DVector acceleration{-offset.x, 0, -offset.z};
    server->SetAcceleration(m_hObject, &acceleration);
    const auto movement = vector_subtract(attach, anchor);
    if (vector_dot(movement, movement) < 1.f)
    {
        DVector velocity;
        if (server->GetVelocity(m_hObject, &velocity) == DE_OK)
        {
            velocity = vector_scale(velocity, 1.f - .2f * server->GetFrameTime());
            server->SetVelocity(m_hObject, &velocity);
        }
    }
    if (server->IsObjectAlive(chain) && server->GetObjectPos(m_hObject, &position) == DE_OK)
    {
        DRotation rotation;
        if (align_rotation(vector_subtract(position, attach), rotation))
            server->SetObjectRotation(chain, &rotation);
        server->TeleportObject(chain, &attach);
    }
    anchor = attach;
    return true;
}

void WreckingBall::DamageNearbyModels()
{
    auto* server = GetServerDE();
    DVector position, velocity;
    std::vector<HOBJECT> targets;
    if (server->GetObjectPos(m_hObject, &position) != DE_OK || server->GetVelocity(m_hObject, &velocity) != DE_OK ||
        !server->CollectObjectsTouchingSphere(position, 300, targets))
        return;
    for (auto target : targets)
        if (server->IsObjectAlive(target) && server->GetObjectType(target) == OT_WORLDMODEL)
            if (const auto message = server->StartMessageToObject(this, target, damage_message))
            {
                if (server->WriteDamage(message, {velocity, position, 20000, DamageType::Normal, m_hObject}) == DE_OK)
                    server->EndMessage(message);
                else
                    server->EndHMessageWrite(message);
            }
}
