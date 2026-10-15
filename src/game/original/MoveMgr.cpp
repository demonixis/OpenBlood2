// Original dry-land movement/control branches, adapted to typed local services.
// Source attribution and deliberate differences: docs/M2_MOVEMENT_SOURCES.json.
#include "MoveMgr.h"
#include "BloodShells.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr DDWORD run = 0x0001, jump = 0x0002, forward = 0x0008, backward = 0x0010, strafe_right = 0x0100,
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
    UpdateOnGround();
    return true;
}
void CMoveMgr::UpdateControlFlags(const MovementInput& input, bool can_move)
{
    m_dwLastControlFlags = m_dwControlFlags;
    m_dwControlFlags = 0;
    if (!can_move)
        return;
    if (input.run)
        m_dwControlFlags |= run;
    if (input.jump)
        m_dwControlFlags |= jump;
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
    if (m_bOnGround && body.ground_normal.y < 0.5f)
    {
        const auto across = normalized(cross(body.ground_normal, {0, 1, 0}));
        body.acceleration = vector_add(body.acceleration, vector_scale(cross(across, body.ground_normal), -2000));
        m_bOnGround = false;
        body.velocity = {0, -magnitude(body.velocity), 0};
    }
}
void CMoveMgr::UpdateMotion(float yaw)
{
    body.acceleration = {};
    UpdateOnGround();
    const auto max_velocity = physics.move_speed * ((m_dwControlFlags & run) ? 1.5f : 1.0f);
    const auto max_acceleration = max_velocity * drag;
    // Source flattened/normalized camera axes: looking up/down never changes
    // walking speed. Digital diagonals retain the original sqrt(2) allowance.
    const DVector front{std::sin(yaw), 0, std::cos(yaw)}, right{std::cos(yaw), 0, -std::sin(yaw)};
    const float forward_accel =
        ((m_dwControlFlags & forward) ? max_acceleration : 0) - ((m_dwControlFlags & backward) ? max_acceleration : 0);
    const float strafe_accel = ((m_dwControlFlags & strafe_right) ? max_acceleration : 0) -
                               ((m_dwControlFlags & strafe_left) ? max_acceleration : 0);
    body.acceleration = vector_add(body.acceleration,
                                   vector_add(vector_scale(front, forward_accel), vector_scale(right, strafe_accel)));
    bool steep = false;
    if (m_bOnGround)
    {
        if (body.ground_normal.z + body.ground_normal.x > body.ground_normal.y)
        {
            body.acceleration.y -= 4000;
            steep = true;
        }
        else
            TiltVectorToPlane(body.acceleration, body.ground_normal);
    }
    m_bFalling = !m_bOnGround;
    if (m_dwControlFlags & jump)
    {
        if (!m_bLastJumpCommand && m_bOnGround && !steep)
        {
            body.velocity.y += physics.jump_speed;
            m_bLastJumpCommand = true;
            m_bOnGround = false;
            ++jumps;
        }
    }
    else
        m_bLastJumpCommand = false;
    auto air_velocity = body.velocity;
    if (!m_bOnGround)
        air_velocity.y = air_velocity.y >= 0 ? 0 : std::max(air_velocity.y / 2, -25.0f);
    if (vector_dot(air_velocity, air_velocity) > 0.01f)
        body.acceleration = vector_add(body.acceleration, vector_scale(air_velocity, -drag));
    const float horizontal = std::hypot(body.velocity.x, body.velocity.z);
    const float cap = max_velocity * std::sqrt(2.0f);
    if (horizontal > cap)
    {
        body.velocity.x *= cap / horizontal;
        body.velocity.z *= cap / horizontal;
    }
    if (m_bLastJumpCommand && body.velocity.y > 0 && body.acceleration.y > 0)
        body.acceleration.y = 0;
}
PlayerMove CMoveMgr::WritePositionInfo(DVector destination, DVector velocity) const
{
    return {m_ClientMoveCode, destination, velocity, body.acceleration, m_bOnGround};
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
    auto gravity = physics.gravity;
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
bool CMoveMgr::Update(CBloodServerShell& shell, const MovementInput& input, float yaw, double seconds, bool can_move)
{
    if (!ready() || !std::isfinite(seconds) || seconds < 0 || seconds > 60 || !std::isfinite(yaw) ||
        server_->GetClientUserData(client_) != object_)
        return false;
    UpdateControlFlags(input, can_move);
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
        UpdateMotion(yaw);
        if (!MoveLocalSolidObject(shell))
            return false;
        ++substeps;
    }
    return true;
}
