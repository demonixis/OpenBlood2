#include "CharacterProfiles.h"
// Selected original PlayerObj.cpp HandleMovement/GetLandingSound and saved fall history.
// Copyright (c) 1997, 1998 Monolith Productions Inc.
#include "PlayerObj.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>

bool CPlayerObj::GetLandingSound(std::string& sound)
{
    auto* server = GetServerDE();
    DVector position{}, dimensions{};
    if (!server || !server->IsObjectAlive(m_hObject) || server->GetObjectPos(m_hObject, &position) != DE_OK ||
        server->GetObjectDims(m_hObject, &dimensions) != DE_OK)
    {
        landing_error = "Landing sound owner/pose unavailable";
        return false;
    }
    position.y -= dimensions.y - 1;
    landing_surface = m_eLastSurface;
    if (!server->GetPointContainerCode(position, landing_container))
    {
        landing_error = "Landing foot container query failed";
        return false;
    }
    if (IsLiquid(landing_container))
        landing_surface = SURFTYPE_LIQUID;
    const char* material = nullptr;
    switch (landing_surface)
    {
        case SURFTYPE_WOOD:
            material = "wood";
            break;
        case SURFTYPE_FLESH:
            material = "flesh";
            break;
        case SURFTYPE_GLASS:
            material = "glass";
            break;
        case SURFTYPE_METAL:
            material = "metal";
            break;
        case SURFTYPE_PLASTIC:
            material = "plastic";
            break;
        case SURFTYPE_STONE:
            material = "stone";
            break;
        case SURFTYPE_TERRAIN:
            material = "terrain";
            break;
        case SURFTYPE_LIQUID:
            material = "water";
            break;
        case SURFTYPE_CLOTH:
            material = "carpet";
            break;
        default:
            landing_error = "No original landing mapping for surface " + std::to_string(landing_surface);
            return false;
    }
    // Source male/female suffix; landing never changes foot alternation.
    sound = std::string("sounds/player/footsteps/") + material +
            (OpenBlood2::character_profiles[m_nCharacter].female ? "/landf.wav" : "/landm.wav");
    landing_error.clear();
    return true;
}

bool CPlayerObj::UpdateLandingSound()
{
    // Original HandleMovement guards surround this block. Blocked/dead players
    // keep their history; free movement resets its start before the source air update.
    if (m_damage.IsDead() || movement_blocked)
        return true;
    auto* server = GetServerDE();
    OpenBlood2::MovementState body;
    if (!server || !server->GetMovementState(m_hObject, body) || !UpdateContainerState())
    {
        landing_error = "Landing movement/container query failed";
        return false;
    }
    bool ladder = false;
    for (std::size_t i = 0; i < containers.count; ++i)
        if (containers.body[i].code == CC_LADDER)
        {
            ladder = !containers.body[i].hidden;
            break;
        }
    const bool free_movement = IsLiquid(containers.head.code) || ladder;
    const bool on_ground = body.standing && body.ground_normal.y >= .5f;
    if (!free_movement && m_bFalling && on_ground)
    {
        ++landing_contacts;
        landing_distance = m_fMaxFallPos - m_fMinFallPos;
        landing_error.clear();
        if (landing_distance > 56 && !IsPowerupActive(PU_INCORPOREAL))
        {
            ++landing_requests;
            landing_resource.clear();
            if (GetLandingSound(landing_resource))
            {
                // Actual source spatial playback and AI sound alert, including alert-after-failure.
                if (PlayPlayerSound(landing_resource, 200, 70, false, 0))
                    ++landing_starts;
                else
                    landing_error = sound_error;
            }
        }
        m_startFall = -1;
    }
    else if (free_movement)
        m_startFall = -1;
    // Primary world support deliberately has no synthetic HOBJECT in this engine.
    m_bServerFalling = !body.standing;
    m_bFalling = !on_ground;
    if (m_bFalling)
    {
        if (m_startFall < 0)
        {
            m_startFall = server->GetTime();
            m_fMinFallPos = m_fMaxFallPos = body.position.y;
        }
        else
        {
            m_fMinFallPos = std::min(m_fMinFallPos, body.position.y);
            m_fMaxFallPos = std::max(m_fMaxFallPos, body.position.y);
        }
    }
    return true; // Missing cosmetic audio does not stop movement or rearm a landing.
}

// Original PlayerObj.cpp ObjectTouch active single-player falling-damage block.
// The older elapsed-fall-time branch in the donor is commented out.
void CPlayerObj::ObjectTouch(HOBJECT other, float force)
{
    if (ObjectTouchDoor(other))
        return; // Original newly queued rotating-door push takes priority.
    auto* server = GetServerDE();
    DVector velocity{}, position{};
    if (!server || !server->IsObjectAlive(m_hObject) || !std::isfinite(force) ||
        server->GetVelocity(m_hObject, &velocity) != DE_OK)
    {
        fall_damage_error = "Invalid falling contact or unavailable velocity";
        return;
    }
    if (!(m_bFalling || m_bServerFalling) || velocity.y >= -1100.f)
        return;
    const float amount = (force - 3000.f) / 25.f; // PA_MIN_DAMAGE_FORCE, original SDK force units.
    if (amount <= 0)
        return;
    ++fall_damage_requests;
    fall_damage_force = force;
    fall_damage_speed = velocity.y;
    fall_damage_amount = amount;
    fall_damage_error.clear();
    if (server->GetObjectPos(m_hObject, &position) != DE_OK)
    {
        fall_damage_error = "Falling damage position unavailable";
        return;
    }
    const auto message = server->StartMessageToObject(this, m_hObject, OpenBlood2::damage_message);
    if (!message)
    {
        fall_damage_error = "Falling damage message unavailable";
        return;
    }
    if (server->WriteDamage(message, {{0, -1, 0}, position, amount, OpenBlood2::DamageType::Normal, m_hObject}) !=
        DE_OK)
    {
        server->EndHMessageWrite(message);
        fall_damage_error = "Falling damage payload rejected";
        return;
    }
    if (server->EndMessage(message) != DE_OK)
        fall_damage_error = "Falling damage delivery failed";
    else
        ++fall_damage_deliveries; // Delivery is not necessarily a health change.
}
