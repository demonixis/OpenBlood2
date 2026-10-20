#include "CharacterProfiles.h"
// Selected PlayerObj.cpp:6541-6865 values and model/flag/velocity Load tail.
// Complete engine, client-data and unported gameplay restoration remain unavailable.
#include "CharacterNodes.h"
#include "PlayerObj.h"
#include <algorithm>
#include <cmath>

namespace
{
    bool valid(const OpenBlood2::PlayerState& state, float now, std::string& error)
    {
        if (!std::isfinite(now) || !std::isfinite(state.smell_time_offset) ||
            !std::isfinite(now + state.smell_time_offset) || !std::isfinite(state.smell_delay) ||
            state.smell_delay < 0 || !std::isfinite(state.pitch) || !std::isfinite(state.yaw) ||
            !std::isfinite(state.air) || state.air < 0 || state.air > 100)
        {
            error = "Invalid saved player clock, angle or air level";
            return false;
        }
        if (!std::isfinite(state.imprison_start_offset) || !std::isfinite(now + state.imprison_start_offset) ||
            !std::isfinite(state.imprison_length) || state.imprison_length < 0)
        {
            error = "Invalid saved player imprisonment clock";
            return false;
        }
        if (!std::isfinite(state.fall_start_offset) || !std::isfinite(state.fall_start_offset + now) ||
            !std::isfinite(state.min_fall_y) || !std::isfinite(state.max_fall_y) ||
            std::abs(state.min_fall_y) > 1000000 || std::abs(state.max_fall_y) > 1000000 ||
            state.min_fall_y > state.max_fall_y)
        {
            error = "Invalid saved player fall history";
            return false;
        }
        if (!OpenBlood2::character_profile(state.character) || state.skin != 0 ||
            (state.move_state > 14 && state.move_state != 16))
        {
            error = "Unsupported saved player character, skin or movement state";
            return false;
        }
        for (auto attribute : {state.strength, state.speed, state.resistance, state.magic})
            if (attribute < 1 || attribute > 6)
            {
                error = "Invalid saved player attribute";
                return false;
            }
        for (const auto* text : {&state.trigger, &state.name})
            if (*text && ((*text)->size() > 100 || (*text)->find('\0') != std::string::npos))
            {
                error = "Invalid saved player string";
                return false;
            }
        return true;
    }
    bool available(const CPlayerObj& player, std::string& error)
    {
        auto* server = BaseClass::GetServerDE();
        if (!server || !server->IsObjectAlive(player.m_hObject) || !player.initialized ||
            player.m_damage.handling_damage)
        {
            error = "Player fields require a live initialized owner outside damage callbacks";
            return false;
        }
        if (server->IsObjectAlive(player.orb_object))
        {
            error = "Active Orb requires unsaved projectile/camera state";
            return false;
        }
        // This partial snapshot omits power flags/timers (the original Save writes them).
        // Keep the existing Stealth/Willpower transfer boundary; Anger stays refused
        // so its inventory multiplier cannot become permanent without an expiry.
        for (int i = 0; i < PU_MAXTYPES; ++i)
            if (player.m_bPowerupActivated[i] && i != PU_INVISIBLE && i != PU_INVULNERABLE)
            {
                error = "Active player power requires unported saved expiration timers";
                return false;
            }
        for (unsigned slot = 0; slot < inventory_weapon_slots; ++slot)
            for (bool left : {false, true})
                if (const auto* weapon = player.m_InventoryMgr.GetWeapon(slot, left); weapon && weapon->firing)
                {
                    error = "Player fields during a weapon callback rejected";
                    return false;
                }
        return true;
    }
} // namespace

bool CPlayerObj::SaveFields(OpenBlood2::PlayerState& output, std::string& error) const
{
    error.clear();
    if (!available(*this, error))
        return false;
    auto* server = GetServerDE();
    const float now = server->GetTime();
    OpenBlood2::PlayerState state;
    state.smell_time_offset = m_fSmellTime - now;
    state.smell_delay = m_fSmellDelay;
    state.dead = m_bDead != 0; // Player death flag is distinct from its damage aggregate.
    state.movement_blocked = movement_blocked;
    state.imprisoned = m_bImprisoned;
    state.imprison_start_offset = m_fImprisonStart - now;
    state.imprison_length = m_fImprisonLength;
    state.falling = m_bFalling;
    state.server_falling = m_bServerFalling;
    state.fall_start_offset = m_startFall - now;
    state.min_fall_y = m_fMinFallPos;
    state.max_fall_y = m_fMaxFallPos;
    state.pitch = m_fPitch;
    state.yaw = m_fYaw;
    state.air = m_fAirLevel;
    auto copy = [&](HSTRING from, std::optional<std::string>& to)
    {
        if (!from)
            return true;
        const char* text = server->GetStringData(from);
        if (!text)
        {
            error = "Saved player string is stale";
            return false;
        }
        to = text;
        return true;
    };
    if (!copy(m_hstrTrigger, state.trigger) || !copy(m_hstrPlayerName, state.name))
        return false;
    state.last_crouch = crouch_requested;
    state.forced_crouch = forced_crouch;
    state.character = m_nCharacter;
    state.skin = m_nSkin;
    state.strength = m_nAttribStrength;
    state.speed = m_nAttribSpeed;
    state.resistance = m_nAttribResistance;
    state.magic = m_nAttribMagic;
    if (static_cast<unsigned>(move_state) > 14 && move_state != MoveState::Dead)
    {
        error = "Unsupported current player movement state";
        return false;
    }
    state.move_state = static_cast<std::uint8_t>(move_state);
    if (!valid(state, now, error))
        return false;
    output = std::move(state);
    return true;
}

bool CPlayerObj::LoadFields(const OpenBlood2::PlayerState& state, std::string& error)
{
    error.clear();
    if (!available(*this, error))
        return false;
    if (m_hClient || m_InventoryMgr.m_hClient)
    {
        error = "Player fields restore requires an owner before client association";
        return false;
    }
    auto* server = GetServerDE();
    const float now = server->GetTime();
    if (!valid(state, now, error))
        return false;
    // Allocate both owned strings before replacing either. Null and empty remain distinct.
    struct Strings
    {
        ServerDE* server;
        HSTRING trigger = nullptr, name = nullptr;
        ~Strings()
        {
            if (trigger)
                server->FreeString(trigger);
            if (name)
                server->FreeString(name);
        }
    } strings{server};
    if ((state.trigger && !(strings.trigger = server->CreateString(state.trigger->c_str()))) ||
        (state.name && !(strings.name = server->CreateString(state.name->c_str()))))
    {
        error = "Saved player strings could not be allocated";
        return false;
    }
    std::swap(m_hstrTrigger, strings.trigger);
    std::swap(m_hstrPlayerName, strings.name);
    m_fSmellTime = state.smell_time_offset + now;
    m_fSmellDelay = state.smell_delay;
    m_bDead = state.dead;
    movement_blocked = state.movement_blocked;
    m_bImprisoned = state.imprisoned;
    m_fImprisonStart = state.imprison_start_offset + now;
    m_fImprisonLength = state.imprison_length;
    m_bFalling = state.falling;
    m_bServerFalling = state.server_falling;
    m_startFall = state.fall_start_offset + now;
    m_fMinFallPos = state.min_fall_y;
    m_fMaxFallPos = state.max_fall_y;
    m_fPitch = state.pitch;
    m_fYaw = state.yaw;
    m_fAirLevel = state.air;
    crouch_requested = state.last_crouch;
    forced_crouch = state.forced_crouch;
    m_nCharacter = state.character;
    m_nSkin = state.skin;
    m_nAttribStrength = state.strength;
    m_nAttribSpeed = state.speed;
    m_nAttribResistance = state.resistance;
    m_nAttribMagic = state.magic;
    move_state = static_cast<MoveState>(state.move_state);
    m_nLastHitPoints = m_nLastDamage = 0; // Original Load invalidates health/damage caches.
    last_sent_stats_ = {0, 0, 0};         // Source invalidates its last-sent stat caches without changing actual stats.
    restore_fields_loaded = true;
    restore_model_loaded = false;
    restore_engine_loaded = false;
    restore_model_error.clear();
    // No Respawn, ValidateAttributes, initialization payload or aggregate load here.
    return true;
}

// Selected original Load tail (PlayerObj.cpp:6838-6857). Separate from value
// restoration: failure can leave a partially configured, unassociated owner.
bool CPlayerObj::RestoreModelState(std::string& error)
{
    error.clear();
    restore_model_loaded = false;
    restore_engine_loaded = false;
    auto fail = [&](const char* why)
    {
        error = restore_model_error = why;
        return false;
    };
    if (!available(*this, error))
    {
        restore_model_error = error;
        return false;
    }
    if (!restore_initialization || !restore_fields_loaded || m_hClient || m_InventoryMgr.m_hClient)
        return fail("Player model restore requires loaded fields and an unassociated savegame owner");
    if (!SetCharacter(m_nCharacter))
        return fail("Saved player character model/animation setup failed");
    if (!OpenBlood2::reset_character_nodes(m_hObject, error))
    {
        restore_model_error = error;
        return false;
    }
    auto* server = GetServerDE();
    const DDWORD flags = FLAG_VISIBLE | FLAG_MODELKEYS | FLAG_YROTATION | FLAG_ANIMTRANSITION | FLAG_TOUCH_NOTIFY |
                         FLAG_SOLID | FLAG_STAIRSTEP; // Source Load does not add SHADOW.
    server->SetObjectFlags(m_hObject, flags);
    DVector velocity{};
    if (server->SetVelocity(m_hObject, &velocity) != DE_OK)
        return fail("Saved player velocity reset failed");
    restore_model_loaded = true;
    restore_model_error.clear();
    m_bZoomView = DFALSE;
    // Source Load resets zoom without a message (the client starts unzoomed). No Respawn, teleport,
    // dimension/animation-clock reconstruction, scheduling or client association.
    return true;
}
