// Selected CPlayerObj::UpdateAnim/Do*Anim/SetAnimation and CAnim_Sound mappings.
// See docs/contracts/locomotion_contracts.md for source evidence and engine adaptations.
#include "PlayerObj.h"
#include <cmath>

using namespace OpenBlood2;

bool CPlayerObj::UpdateAnim(const PlayerControls& controls)
{
    auto* server = GetServerDE();
    if (!server || !m_hClient || !attributes_received || !std::isfinite(controls.yaw) ||
        server->GetClientUserData(m_hClient) != m_hObject || m_damage.IsDead())
        return false;
    m_fYaw = controls.yaw;
    DRotation rotation;
    rotation.m_Vec.y = std::sin(m_fYaw * 0.5f);
    rotation.m_Spin = std::cos(m_fYaw * 0.5f);
    if (server->SetObjectRotation(m_hObject, &rotation) != DE_OK)
        return false;
    const auto& input = controls.movement;
    command_run = input.run;
    crouch_requested = input.crouch;
    const auto* weapon = m_InventoryMgr.CurrentWeapon(false);
    const bool knife = weapon && weapon->data.m_nFireType == 4;
    const bool moving = input.forward || input.backward || input.left || input.right;
    // Retry standing every frame after release; a failed resize never mutates
    // the animation or collision body. A second pass selects crawl/crouch under
    // a ceiling instead of leaving a stale walking clip on a short body.
    for (unsigned attempt = 0; attempt < 2; ++attempt)
    {
        const bool duck = input.crouch || attempt != 0;
        MoveState state;
        std::string name;
        bool looping = true;
        if (input.jump && !duck)
        {
            state = controls.firing ? MoveState::FireJump : MoveState::Jump;
            name = controls.firing ? "fire_jump_" : "jmp_";
            name += knife ? "knife1" : controls.firing ? "1pistol" : "pistol";
            looping = false;
        }
        else if (duck)
        {
            state = moving ? (controls.firing ? MoveState::FireCrawl : MoveState::Crawl)
                           : (controls.firing ? MoveState::FireCrouch : MoveState::Crouch);
            name = controls.firing ? "fire_" : "";
            name += moving ? "crawl_" : "crouch_";
            name += knife ? "knife1" : "1pistol";
            looping = moving;
        }
        else if (moving)
        {
            if (input.left || input.right)
            {
                state = input.left ? MoveState::StrafeLeft : MoveState::StrafeRight;
                // Original ANIM_STRAFE_LEFT_1PISTOL is this same right-hand name,
                // including TYPE_MELEE. Preserve that source alias explicitly.
                name = "strafe_right_1pistol";
            }
            else
            {
                state = input.run ? (controls.firing ? MoveState::FireRun : MoveState::Run)
                                  : (controls.firing ? MoveState::FireWalk : MoveState::Walk);
                name = controls.firing ? "fire_" : "";
                name += input.run ? "run_" : "walk_";
                name += knife ? "knife1" : controls.firing ? "1pistol" : "pistol";
            }
        }
        else if (controls.firing)
        {
            state = MoveState::FireStand;
            name = knife ? "fire_stand_knife1" : "fire_stand_1pistol";
            looping = false;
        }
        else
        {
            state = MoveState::Idle;
            name = move_state == state ? locomotion_clip : (idle_random_() % 2 ? "idle1" : "idle2");
        }
        locomotion_requested_clip = name;
        const unsigned column = knife ? 4 : 2;
        DDWORD animation = UINT32_MAX;
        switch (state)
        {
            case MoveState::Idle:
                animation = m_Anim_Sound.m_nAnim_IDLE[name == "idle2" ? 1 : 0];
                break;
            case MoveState::Walk:
                animation = m_Anim_Sound.m_nAnim_WALK[column];
                break;
            case MoveState::Run:
                animation = m_Anim_Sound.m_nAnim_RUN[column];
                break;
            case MoveState::Jump:
                animation = m_Anim_Sound.m_nAnim_JUMP[column];
                break;
            case MoveState::Crouch:
                animation = m_Anim_Sound.m_nAnim_CROUCH[column];
                break;
            case MoveState::Crawl:
                animation = m_Anim_Sound.m_nAnim_CRAWL[column];
                break;
            case MoveState::StrafeRight:
                animation = m_Anim_Sound.m_nAnim_STRAFERIGHT[column];
                break;
            case MoveState::StrafeLeft:
                animation = m_Anim_Sound.m_nAnim_STRAFELEFT[column];
                break;
            case MoveState::FireStand:
                animation = m_Anim_Sound.m_nAnim_FIRE_STAND[column];
                break;
            case MoveState::FireWalk:
                animation = m_Anim_Sound.m_nAnim_FIRE_WALK[column];
                break;
            case MoveState::FireRun:
                animation = m_Anim_Sound.m_nAnim_FIRE_RUN[column];
                break;
            case MoveState::FireJump:
                animation = m_Anim_Sound.m_nAnim_FIRE_JUMP[column];
                break;
            case MoveState::FireCrouch:
                animation = m_Anim_Sound.m_nAnim_FIRE_CROUCH[column];
                break;
            case MoveState::FireCrawl:
                animation = m_Anim_Sound.m_nAnim_FIRE_CRAWL[column];
                break;
            default:
                break;
        }
        if (!server->GetModelAnimationName(m_hObject, animation, name))
            animation = UINT32_MAX;
        DVector dimensions;
        if (animation == UINT32_MAX || server->GetModelAnimUserDims(m_hObject, &dimensions, animation) != DE_OK)
        {
            locomotion_error = "Missing player animation/dimensions: " + locomotion_requested_clip;
            return false;
        }
        const auto resize = server->ResizeObject(m_hObject, dimensions);
        if (resize == ResizeResult::Blocked)
        {
            ++blocked_resizes;
            locomotion_error = "Blocked animation dimensions: " + name;
            forced_crouch = crouched && !input.crouch;
            if (forced_crouch && attempt == 0)
                continue;
            return true;
        }
        if (resize != ResizeResult::Resized)
        {
            locomotion_error = "Player shape service unavailable or invalid";
            return false;
        }
        const bool jump = state == MoveState::Jump || state == MoveState::FireJump;
        const bool done = (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE) != 0;
        const bool reload = state == MoveState::FireStand && weapon &&
                            server->GetTime() > animation_start_ + weapon->data.m_fReloadTime;
        if (state != move_state || name != locomotion_clip || (!jump && (done || reload)))
        {
            server->SetModelAnimation(m_hObject, animation);
            if (server->ResetModelAnimation(m_hObject) != DE_OK)
                return false;
            server->SetModelLooping(m_hObject, looping);
            animation_start_ = server->GetTime();
            ++animation_changes;
        }
        move_state = state;
        locomotion_clip = std::move(name);
        crouched = duck;
        forced_crouch = duck && !input.crouch;
        eye_level = duck ? 12.0f : 36.0f;
        if (!forced_crouch)
            locomotion_error.clear();
        return true;
    }
    return true;
}
