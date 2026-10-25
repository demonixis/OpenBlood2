// Original movement/control/container branches, adapted to typed local services.
// Source attribution and deliberate differences: Documentation/implementation/sources/m2_movement_sources.json.
#include "MoveMgr.h"
#include "BloodShells.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr DDWORD run = 0x0001, jump = 0x0002, crouch = 0x0004, forward = 0x0008, backward = 0x0010,
                     strafe_right = 0x0100,
                     strafe_left = 0x0200; // Shared/SharedMovement.h
    constexpr float drag = 15.0f;          // Original g_fDragCoeff.
    DVector cross(DVector a, DVector b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    float magnitude(DVector v)
    {
        return std::sqrt(OpenBlood2::vector_dot(v, v));
    }
    DVector normalized(DVector v)
    {
        const auto n = magnitude(v);
        return n > 0 ? OpenBlood2::vector_scale(v, 1 / n) : DVector{};
    }
    void TiltVectorToPlane(DVector& v, DVector normal)
    {
        auto q = cross(normal, v);
        if (OpenBlood2::vector_dot(q, q) > 0.001f)
            v = OpenBlood2::vector_scale(cross(normalized(q), normal), magnitude(v));
    }
} // namespace
using namespace OpenBlood2;

bool CMoveMgr::Init(ServerDE& server, HCLIENT client)
{
    Term();
    auto object = server.GetClientUserData(client);
    if (!object || !server.GetMovementState(object, body))
        return false;
    server_ = &server;
    client_ = client;
    object_ = object;
    return true;
}
void CMoveMgr::Term()
{
    *this = CMoveMgr{};
}
bool CMoveMgr::OnPhysicsUpdate(const PlayerPhysics& value)
{
    if (!server_ || server_->GetClientUserData(client_) != object_ || !physics_vector(value.gravity) ||
        !std::isfinite(value.move_speed) || value.move_speed < 0 || value.move_speed > 10000 ||
        !std::isfinite(value.jump_speed) || value.jump_speed < 0 || value.jump_speed > 10000)
        return false;
    physics = value;
    configured_ = true;
    return true;
}
bool CMoveMgr::OnServerForcePos(const PlayerPosition& value)
{
    if (!server_ || server_->GetClientUserData(client_) != object_ || !physics_vector(value.position))
        return false;
    // The server already teleported the shared body. Reset motion as the
    // original force-position handler does, without replaying a second teleport.
    DVector zero;
    if (server_->SetVelocity(object_, &zero) != DE_OK || server_->SetAcceleration(object_, &zero) != DE_OK ||
        !server_->GetMovementState(object_, body))
        return false;
    m_ClientMoveCode = value.move_code;
    positioned_ = true;
    m_bLastJumpCommand = false;
    containers = {};
    m_eLastContainerCode = CC_NOTHING;
    m_bBodyInLiquid = m_bBodyOnLadder = m_bBodyOnConveyor = m_bSwimmingOnSurface = m_bCanSwimJump = m_bSwimmingJump =
        false;
    m_bUseGravity = true;
    UpdateOnGround();
    return true;
}
void CMoveMgr::UpdateControlFlags(const MovementInput& input, bool can_move)
{
    input_ = can_move ? input : MovementInput{};
    m_dwLastControlFlags = m_dwControlFlags;
    m_dwControlFlags = 0;
    if (!can_move)
        return;
    if (input.run)
        m_dwControlFlags |= run;
    if (input.jump)
        m_dwControlFlags |= jump;
    if (input.crouch)
        m_dwControlFlags |= crouch;
    if (input.forward)
        m_dwControlFlags |= forward;
    if (input.backward)
        m_dwControlFlags |= backward;
    if (input.left)
        m_dwControlFlags |= strafe_left;
    if (input.right)
        m_dwControlFlags |= strafe_right;
}
void CMoveMgr::UpdateOnGround()
{
    m_bOnGround = body.standing;
    if (m_bOnGround)
        m_eLastSurface = static_cast<SurfaceType>(body.ground_surface);
    if (m_bOnGround && body.ground_normal.y < 0.5f)
    {
        const auto across = normalized(cross(body.ground_normal, {0, 1, 0}));
        body.acceleration = vector_add(body.acceleration, vector_scale(cross(across, body.ground_normal), -2000));
        m_bOnGround = false;
        body.velocity = {0, -magnitude(body.velocity), 0};
    }
}
void CMoveMgr::UpdateInLiquid(const ContainerInfo& info)
{
    if (info.hidden || (!IsLiquid(containers.head.code) && m_bOnGround))
        return;
    m_bBodyInLiquid = true;
    const auto speed = magnitude(body.velocity);
    if (speed > 1)
        body.velocity = vector_scale(body.velocity, std::max(0.0f, speed - m_FrameTime * m_fSwimVel) / speed);
    m_bUseGravity = false;
    if (m_bSwimmingOnSurface)
    {
        const bool moving = vector_dot(body.acceleration, body.acceleration) > .0001f ||
                            vector_dot(body.velocity, body.velocity) > .0001f;
        if (moving && (body.velocity.y > 0 || body.acceleration.y > 0))
            body.velocity.y = body.acceleration.y = 0;
        else if (!moving)
            body.acceleration.y += info.gravity;
    }
    else
        body.acceleration.y += info.gravity;
}
void CMoveMgr::UpdateOnLadder(const ContainerInfo& info)
{
    if (info.hidden)
        return;
    m_bBodyOnLadder = true;
    const auto speed = magnitude(body.velocity);
    if (speed > 1)
        body.velocity = vector_scale(body.velocity, std::max(0.0f, speed - m_FrameTime * (m_fLadderVel / .2f)) / speed);
    m_bUseGravity = false;
}
void CMoveMgr::UpdateOnConveyor(const ContainerInfo& info)
{
    if (info.hidden)
        return;
    m_bBodyOnConveyor = true;
    const auto speed = magnitude(body.velocity);
    if (speed > 1)
        body.velocity = vector_scale(body.velocity, std::max(0.0f, speed - m_FrameTime * (m_fLadderVel / .2f)) / speed);
    // Unlike ladders, conveyors retain gravity and ordinary jump controls.
}
void CMoveMgr::UpdateMotion(float yaw, float pitch, CVoiceMgr& voices, int character, bool external_camera)
{
    body.acceleration = {};
    m_bUseGravity = true;
    m_bBodyInLiquid = m_bBodyOnLadder = m_bBodyOnConveyor = false;
    bool did_liquid = false, did_ladder = false, did_conveyor = false;
    for (std::size_t i = 0; i < containers.count; ++i)
    {
        const auto& info = containers.body[i];
        if (IsLiquid(info.code) && !did_liquid)
        {
            UpdateInLiquid(info);
            did_liquid = true;
        }
        else if (info.code == CC_LADDER && !did_ladder)
        {
            UpdateOnLadder(info);
            did_ladder = true;
        }
        else if (info.code == CC_CONVEYOR && !did_conveyor)
        {
            UpdateOnConveyor(info);
            did_conveyor = true;
        }
        if (!info.hidden)
            body.velocity = vector_add(body.velocity, vector_scale(info.current, m_FrameTime));
    }
    const bool head_liquid = IsLiquid(containers.head.code), in_liquid = head_liquid || m_bBodyInLiquid;
    const bool free_movement = head_liquid || m_bBodyOnLadder;
    UpdateOnGround();
    m_fSwimVel = (m_dwControlFlags & run) ? 175.0f : 87.5f;
    m_fLadderVel = (m_dwControlFlags & run) ? 400.0f : 200.0f;
    const auto land_velocity = physics.move_speed * ((m_dwControlFlags & run) ? 1.5f : 1.0f) *
                               ((((m_dwControlFlags & crouch) || m_bForcedCrouch) && !free_movement) ? .5f : 1.0f);
    const auto max_velocity = m_bBodyInLiquid ? m_fSwimVel : m_bBodyOnLadder ? m_fLadderVel : land_velocity;
    const auto max_acceleration =
        max_velocity * drag * (m_bBodyOnLadder ? .5f : 1.0f) * (input_.mouse_move ? 2.f : 1.f);
    if (!m_bBodyInLiquid)
        m_bSwimmingJump = false;
    const auto speed = magnitude(body.velocity);
    const bool ladder_cap = m_bBodyOnLadder || m_bBodyOnConveyor;
    const auto cap3 = ladder_cap ? m_fLadderVel : m_fSwimVel;
    if ((ladder_cap || (m_bBodyInLiquid && (head_liquid || !m_bOnGround))) && speed > cap3)
    {
        body.velocity = vector_scale(body.velocity, cap3 / speed);
        if (!ladder_cap && m_bSwimmingJump)
            body.velocity.y = physics.jump_speed * .75f;
    }
    const float cp = (free_movement || in_liquid) ? std::cos(pitch) : 1.0f;
    const DVector front{std::sin(yaw) * cp, (free_movement || in_liquid) ? -std::sin(pitch) : 0.0f, std::cos(yaw) * cp};
    const DVector right{std::cos(yaw), 0, -std::sin(yaw)};
    const auto forward_accel =
        max_acceleration *
        std::clamp((float(bool(m_dwControlFlags & forward)) - float(bool(m_dwControlFlags & backward))) /
                           (input_.mouse_move ? 2.f : 1.f) +
                       input_.mouse_forward,
                   -1.f, 1.f);
    const auto strafe_accel =
        max_acceleration * std::clamp(float(bool(m_dwControlFlags & strafe_right)) -
                                          float(bool(m_dwControlFlags & strafe_left)) + input_.mouse_strafe,
                                      -1.f, 1.f);
    body.acceleration = vector_add(body.acceleration,
                                   vector_add(vector_scale(front, forward_accel), vector_scale(right, strafe_accel)));
    bool steep = false;
    if (m_bOnGround && !free_movement)
    {
        if (body.ground_normal.z + body.ground_normal.x > body.ground_normal.y)
        {
            body.acceleration.y -= 4000;
            steep = true;
        }
        else
            TiltVectorToPlane(body.acceleration, body.ground_normal);
    }
    if (IsLiquid(m_eLastContainerCode) && !head_liquid && !m_bOnGround && !m_bBodyOnLadder)
        m_bSwimmingOnSurface = true;
    else if (head_liquid)
    {
        m_bSwimmingOnSurface = false;
        m_bCanSwimJump = true;
    }
    m_bFalling = !m_bOnGround;
    if (m_dwControlFlags & jump)
    {
        if (free_movement)
            body.acceleration.y += max_acceleration;
        else if (!m_bLastJumpCommand)
        {
            if (m_bBodyInLiquid && !head_liquid)
            {
                if (m_bCanSwimJump)
                {
                    m_bSwimmingJump = true;
                    m_bCanSwimJump = false;
                }
                else if (m_bOnGround)
                    m_bSwimmingJump = true;
                if (m_bSwimmingJump)
                {
                    body.velocity.y += physics.jump_speed * .75f;
                    m_bLastJumpCommand = true;
                    m_bOnGround = false;
                    m_bSwimmingOnSurface = false;
                    ++jumps;
                }
            }
            else if (m_bOnGround && !steep)
            {
                body.velocity.y += physics.jump_speed * (m_bForcedCrouch ? .75f : 1.0f);
                // Original client VME_JUMP: only this ground branch, never swimming/ladder ascent.
                ++jump_voice_requests;
                jump_voice_starts += voices.PlayEventSound(object_, character, 11, external_camera);
                m_bLastJumpCommand = true;
                m_bOnGround = false;
                ++jumps;
            }
        }
    }
    else
        m_bLastJumpCommand = false;
    if ((m_dwControlFlags & crouch) && (in_liquid || free_movement) && !m_bOnGround)
        body.acceleration.y -= max_acceleration;
    if (!m_bBodyInLiquid && !m_bBodyOnLadder && !m_bBodyOnConveyor)
    {
        auto air_velocity = body.velocity;
        if (!m_bOnGround)
            air_velocity.y = air_velocity.y >= 0 ? 0 : std::max(air_velocity.y / 2, -25.0f);
        if (vector_dot(air_velocity, air_velocity) > .01f)
            body.acceleration = vector_add(body.acceleration, vector_scale(air_velocity, -drag));
    }
    const auto horizontal = std::hypot(body.velocity.x, body.velocity.z), cap = max_velocity * std::sqrt(2.0f);
    if (horizontal > cap)
    {
        body.velocity.x *= cap / horizontal;
        body.velocity.z *= cap / horizontal;
    }
    if (m_bLastJumpCommand && body.velocity.y > 0 && body.acceleration.y > 0)
        body.acceleration.y = 0;
    m_eLastContainerCode = containers.head.code;
}
PlayerMove CMoveMgr::WritePositionInfo(DVector destination, DVector velocity) const
{
    return {m_ClientMoveCode,  destination, velocity,
            body.acceleration, m_bOnGround, static_cast<DBYTE>(m_eLastSurface)};
}
bool CMoveMgr::MoveLocalSolidObject(CBloodServerShell& shell)
{
    // Replacement for the unavailable Blood II PhysicsLT::UpdateMovement:
    // constant-acceleration integration, followed by the actual swept solver.
    // Substeps keep source drag stable. Even tiny displacements resolve contact.
    // The source stops applying drag below speed 0.1; let the replacement
    // integrator sleep that horizontal residue instead of creeping forever.
    if (std::hypot(body.velocity.x, body.velocity.z) < 0.1f && body.acceleration.x == 0 && body.acceleration.z == 0)
        body.velocity.x = body.velocity.z = 0;
    auto gravity = m_bUseGravity ? physics.gravity : DVector{};
    if (m_bOnGround && vector_dot(gravity, body.ground_normal) < 0)
        gravity = vector_scale(body.ground_normal, vector_dot(gravity, body.ground_normal));
    // Supported walkable slopes constrain gravity to the contact normal.
    // Otherwise idle players slide forever; source steep-slope acceleration
    // remains separate and jumps immediately regain the full global force.
    const auto acceleration = vector_add(body.acceleration, gravity);
    const auto offset = vector_add(vector_scale(body.velocity, m_FrameTime),
                                   vector_scale(acceleration, 0.5f * m_FrameTime * m_FrameTime));
    const auto velocity = vector_add(body.velocity, vector_scale(acceleration, m_FrameTime));
    const bool was_air = !m_bOnGround;
    if (!shell.OnMessage(client_, WritePositionInfo(vector_add(body.position, offset), velocity)))
        return false;
    if (!server_->GetMovementState(object_, body))
        return false;
    m_bOnGround = body.standing && body.ground_normal.y >= 0.5f;
    m_bFalling = !m_bOnGround;
    if (was_air && m_bOnGround)
        ++landings;
    m_fVelMagnitude = std::hypot(body.velocity.x, body.velocity.z);
    return true;
}
bool CMoveMgr::Update(CBloodServerShell& shell, CVoiceMgr& voices, bool external_camera, const MovementInput& input,
                      float yaw, double seconds, bool can_move, bool firing, float pitch)
{
    if (!ready() || !std::isfinite(seconds) || seconds < 0 || seconds > 60 || !std::isfinite(yaw) ||
        !std::isfinite(pitch) || server_->GetClientUserData(client_) != object_)
        return false;
    if (!std::isfinite(input.mouse_forward) || !std::isfinite(input.mouse_strafe))
        return false;
    UpdateControlFlags(input, can_move);
    auto animation = input;
    animation.forward |= input.mouse_forward > 0;
    animation.backward |= input.mouse_forward < 0;
    animation.right |= input.mouse_strafe > 0;
    animation.left |= input.mouse_strafe < 0;
    if (can_move && !shell.OnMessage(client_, PlayerControls{animation, firing, yaw}))
        return false;
    auto* player = shell.Player(client_);
    if (!player)
        return false;
    m_bForcedCrouch = player->crouched;
    ++updates;
    // Original Update caps a frame at 0.1s. Track discarded time explicitly.
    const auto duration = std::min(seconds, 0.1);
    discarded_time += seconds - duration;
    if (duration == 0)
        return server_->GetMovementState(object_, body);
    const auto count = static_cast<unsigned>(std::max(1.0, std::ceil(duration * 120)));
    m_FrameTime = static_cast<float>(duration / count);
    for (unsigned i = 0; i < count; ++i)
    {
        if (!server_->GetMovementState(object_, body))
            return false;
        if (!player->UpdateContainerState())
            return false;
        containers = player->containers;
        UpdateMotion(yaw, pitch, voices, player->m_nCharacter, external_camera);
        if (!MoveLocalSolidObject(shell))
            return false;
        ++substeps;
    }
    if (!player->UpdateAirLevel(static_cast<float>(duration)))
        return false;
    containers = player->containers;
    return true;
}
