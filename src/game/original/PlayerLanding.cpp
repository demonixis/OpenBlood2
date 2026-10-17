// Selected original PlayerObj.cpp HandleMovement/GetLandingSound and saved fall history.
// Copyright (c) 1997, 1998 Monolith Productions Inc.
#include "PlayerObj.h"
#include "engine/collision.h"
#include <algorithm>

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
    // Only Caleb is admitted by player entry; unlike footsteps this never changes foot alternation.
    sound = std::string("sounds/player/footsteps/") + material + "/landm.wav";
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
