#include "CharacterProfiles.h"
// Selected PlayerObj.cpp:6541-6873 Save/Load: the typed value block, its original byte layout
// with the one-shot client blob, and the model/flag/velocity Load tail (save_load_contracts lot 5).
#include "CharacterNodes.h"
#include "PlayerObj.h"
#include "SaveFields.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
    bool finite(std::initializer_list<float> values)
    {
        return std::all_of(values.begin(), values.end(), [](float v) { return std::isfinite(v); });
    }
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
        // Each power's expiry is an offset from now, like the other relative clocks.
        for (float offset : state.power_offsets)
            if (!std::isfinite(offset) || !std::isfinite(now + offset))
            {
                error = "Invalid saved player power expiry";
                return false;
            }
        if (state.power_sounds.size() > 15 || // Source PU_MAXSTACK.
            std::any_of(state.power_sounds.begin(), state.power_sounds.end(),
                        [](std::uint8_t power) { return power >= PU_MAXTYPES; }))
        {
            error = "Invalid saved player power sound stack";
            return false;
        }
        const auto& c = state.carried;
        if (!std::isfinite(state.idle_start_offset) || !std::isfinite(now + state.idle_start_offset) ||
            !finite({c.binding_regen, c.server_regen, c.mouse_axis0, c.mouse_axis1, c.death_timer, c.old_air,
                     c.burning_time}))
        {
            error = "Invalid saved player timer";
            return false;
        }
        for (const auto* vector :
             {&c.last_position, &c.last_velocity, &c.scale, &c.forward, &c.gun_muzzle, &c.left_gun_muzzle})
            if (!finite({(*vector)[0], (*vector)[1], (*vector)[2]}))
            {
                error = "Invalid saved player vector";
                return false;
            }
        if (!finite({c.view_rotation[0], c.view_rotation[1], c.view_rotation[2], c.view_rotation[3]}))
        {
            error = "Invalid saved player view rotation";
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
        if (player.ctf_team || player.m_hFlagObject || player.m_hFlagAttach)
        {
            error = "CTF player requires unsaved match/flag state";
            return false;
        }
        if (server->IsObjectAlive(player.orb_object))
        {
            error = "Active Orb requires unsaved projectile/camera state";
            return false;
        }
        // The original Save writes every power's flag and relative expiry, so no active
        // power is refused here (the inventory carries Anger's multiplier, the damage
        // aggregate Willpower's nigh invulnerability and the engine Stealth's color).
        for (unsigned slot = 0; slot < inventory_weapon_slots; ++slot)
            for (bool left : {false, true})
                if (const auto* weapon = player.m_InventoryMgr.GetWeapon(slot, left); weapon && weapon->firing)
                {
                    error = "Player fields during a weapon callback rejected";
                    return false;
                }
        return true;
    }
    DVector to_vector(const std::array<float, 3>& v)
    {
        return {v[0], v[1], v[2]};
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
    state.spectator = m_bSpectatorMode;
    state.imprisoned = m_bImprisoned;
    state.imprison_start_offset = m_fImprisonStart - now;
    state.imprison_length = m_fImprisonLength;
    state.falling = m_bFalling;
    state.server_falling = m_bServerFalling;
    state.fall_start_offset = m_startFall - now;
    state.min_fall_y = m_fMinFallPos;
    state.max_fall_y = m_fMaxFallPos;
    state.idle_start_offset = m_startIdle - now;
    const float expiries[PU_MAXTYPES]{m_fNighInvulnerableTime, m_fInvisibleTime, m_fTripleDamageTime,
                                      m_fIncorporealTime, m_fSilentTime};
    for (int i = 0; i < PU_MAXTYPES; ++i)
    {
        state.powers[i] = m_bPowerupActivated[i];
        state.power_offsets[i] = expiries[i] - now;
    }
    if (m_nPowerupSoundStackPos < 0 || m_nPowerupSoundStackPos > int(m_ePowerupSoundStack.size()))
    {
        error = "Invalid current player power sound stack";
        return false;
    }
    for (int i = 0; i < m_nPowerupSoundStackPos; ++i)
    {
        if (m_ePowerupSoundStack[i] < 0 || m_ePowerupSoundStack[i] >= PU_MAXTYPES)
        {
            error = "Invalid current player power sound stack";
            return false;
        }
        state.power_sounds.push_back(static_cast<std::uint8_t>(m_ePowerupSoundStack[i]));
    }
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
    state.carried = original_values;
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
    // Original `saved + GetTime()`, choosing the nearest float that saves the same offset again.
    const auto restored = [now](float offset) { return OpenBlood2::save_fields::restored_time(offset, now); };
    m_fSmellTime = restored(state.smell_time_offset);
    m_fSmellDelay = state.smell_delay;
    m_bDead = state.dead;
    movement_blocked = state.movement_blocked;
    m_bSpectatorMode = state.spectator;
    m_bImprisoned = state.imprisoned;
    m_fImprisonStart = restored(state.imprison_start_offset);
    m_fImprisonLength = state.imprison_length;
    m_bFalling = state.falling;
    m_bServerFalling = state.server_falling;
    m_startFall = restored(state.fall_start_offset);
    m_fMinFallPos = state.min_fall_y;
    m_fMaxFallPos = state.max_fall_y;
    m_startIdle = restored(state.idle_start_offset);
    float* expiries[PU_MAXTYPES]{&m_fNighInvulnerableTime, &m_fInvisibleTime, &m_fTripleDamageTime, &m_fIncorporealTime,
                                 &m_fSilentTime};
    for (int i = 0; i < PU_MAXTYPES; ++i)
    {
        m_bPowerupActivated[i] = state.powers[i];
        *expiries[i] = restored(state.power_offsets[i]);
    }
    // The sound handles are not saved: CheckPowerups restarts the top power's loop.
    m_nPowerupSoundStackPos = static_cast<int>(state.power_sounds.size());
    for (std::size_t i = 0; i < state.power_sounds.size(); ++i)
        m_ePowerupSoundStack[i] = static_cast<PowerupType>(state.power_sounds[i]);
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
    original_values = state.carried;
    m_nLastHitPoints = m_nLastDamage = 0; // Original Load invalidates health/damage caches.
    last_sent_items_.reset();
    last_sent_stats_ = {0, 0, 0}; // Source invalidates its last-sent stat caches without changing actual stats.
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
    m_nZoomType = 0;
    // Source Load resets zoom without a message (the client starts unzoomed). No Respawn, teleport,
    // dimension/animation-clock reconstruction, scheduling or client association.
    return true;
}

void CPlayerObj::SetClientSaveData(HMESSAGEREAD blob)
{
    if (m_hClientSaveData)
        GetServerDE()->EndHMessageRead(m_hClientSaveData);
    m_hClientSaveData = blob;
}

void CPlayerObj::Save(HMESSAGEWRITE hWrite, DDWORD)
{
    auto* s = GetServerDE();
    OpenBlood2::PlayerState state;
    std::string error;
    if (!SaveFields(state, error))
    {
        s->RejectPayload(hWrite, "Save: " + error);
        return;
    }
    const auto& c = state.carried;
    const auto byte = [&](unsigned value) { s->WriteToMessageByte(hWrite, static_cast<DBYTE>(value)); };
    const auto real = [&](float value) { s->WriteToMessageFloat(hWrite, value); };
    const auto vec = [&](const std::array<float, 3>& value)
    {
        const auto v = to_vector(value);
        s->WriteToMessageVector(hWrite, &v);
    };
    byte(c.demo_playback);
    s->WriteToMessageDWord(hWrite, c.control_flags);
    byte(c.firing_weapon);
    byte(state.falling);
    byte(state.server_falling);
    real(state.fall_start_offset); // The -1 "not falling" sentinel passes through the conversion.
    real(state.idle_start_offset);
    real(state.min_fall_y);
    real(state.max_fall_y);
    real(c.binding_regen);
    real(c.server_regen);
    byte(state.spectator);
    real(state.smell_time_offset);
    real(state.smell_delay);
    byte(state.dead);
    byte(state.movement_blocked);
    for (bool power : state.powers) // INVULNERABLE, INVISIBLE, TRIPLEDAMAGE, INCORPOREAL, SILENT.
        byte(power);
    for (float offset : state.power_offsets)
        real(offset);
    byte(static_cast<unsigned>(state.power_sounds.size()));
    for (auto power : state.power_sounds)
        byte(power);
    s->WriteToMessageHString(hWrite, m_hstrTrigger);
    vec(c.last_position);
    vec(c.last_velocity);
    vec(c.scale);
    real(state.pitch);
    real(state.yaw);
    const DRotation view{{c.view_rotation[0], c.view_rotation[1], c.view_rotation[2]}, c.view_rotation[3]};
    s->WriteToMessageRotation(hWrite, &view);
    vec(c.forward);
    vec(c.gun_muzzle);
    vec(c.left_gun_muzzle);
    s->WriteToMessageHString(hWrite, m_hstrPlayerName);
    byte(c.client_flags);
    real(c.mouse_axis0);
    real(c.mouse_axis1);
    byte(m_bZoomView);
    byte(m_nZoomType);
    s->WriteToMessageObject(hWrite, nullptr); // m_hCameraObj: plain object (D7); this port has no crane camera.
    byte(state.last_crouch);
    byte(c.last_jump);
    byte(state.forced_crouch);
    s->WriteToMessageDWord(hWrite, c.frags);
    s->WriteToMessageDWord(hWrite, c.melee_kills);
    byte(state.character);
    byte(state.skin);
    for (auto attribute : {state.strength, state.speed, state.resistance, state.magic})
        byte(attribute);
    for (auto attribute : c.binding_attributes)
        byte(attribute);
    for (auto binding : c.bindings)
        byte(binding);
    byte(state.move_state);
    for (auto value : {c.injured_leg, c.node_hit, c.side_hit}) // (DBYTE) values written as Words.
        s->WriteToMessageWord(hWrite, value);
    real(c.death_timer);
    byte(c.slow_death);
    real(c.old_air);
    real(state.air);
    byte(c.swimming_jump);
    byte(c.can_swim_jump);
    byte(state.imprisoned);
    real(state.imprison_start_offset);
    real(state.imprison_length);
    byte(c.burning);
    real(c.burning_time);
    // The one-shot client blob: the first save that includes the player consumes it.
    byte(static_cast<bool>(m_hClientSaveData));
    if (m_hClientSaveData)
    {
        s->WriteToMessageHMessageRead(hWrite, m_hClientSaveData);
        s->EndHMessageRead(m_hClientSaveData);
    }
    m_hClientSaveData = nullptr;
}

void CPlayerObj::Load(HMESSAGEREAD hRead, DDWORD)
{
    auto* s = GetServerDE();
    OpenBlood2::PlayerState state;
    auto& c = state.carried;
    bool booleans = true;
    const auto byte = [&] { return s->ReadFromMessageByte(hRead); };
    const auto flag = [&]
    {
        const auto value = byte();
        booleans = booleans && value <= 1;
        return value != 0;
    };
    const auto real = [&] { return s->ReadFromMessageFloat(hRead); };
    const auto vec = [&](std::array<float, 3>& out)
    {
        DVector v;
        s->ReadFromMessageVector(hRead, &v);
        out = {v.x, v.y, v.z};
    };
    c.demo_playback = byte();
    c.control_flags = s->ReadFromMessageDWord(hRead);
    c.firing_weapon = byte();
    state.falling = flag();
    state.server_falling = flag();
    state.fall_start_offset = real();
    state.idle_start_offset = real();
    state.min_fall_y = real();
    state.max_fall_y = real();
    c.binding_regen = real();
    c.server_regen = real();
    state.spectator = flag();
    state.smell_time_offset = real();
    state.smell_delay = real();
    state.dead = flag();
    state.movement_blocked = flag();
    for (auto& power : state.powers)
        power = flag();
    for (auto& offset : state.power_offsets)
        offset = real();
    const DBYTE sounds = byte();
    if (sounds > m_ePowerupSoundStack.size())
    {
        s->RejectPayload(hRead, "Load: power sound stack exceeds PU_MAXSTACK");
        return; // The original would overrun its 15-entry array.
    }
    for (DBYTE i = 0; i < sounds; ++i)
        state.power_sounds.push_back(byte());
    OpenBlood2::save_fields::read_nullable(hRead, state.trigger, 100);
    vec(c.last_position);
    vec(c.last_velocity);
    vec(c.scale);
    state.pitch = real();
    state.yaw = real();
    DRotation view;
    s->ReadFromMessageRotation(hRead, &view);
    c.view_rotation = {view.m_Vec.x, view.m_Vec.y, view.m_Vec.z, view.m_Spin};
    vec(c.forward);
    vec(c.gun_muzzle);
    vec(c.left_gun_muzzle);
    OpenBlood2::save_fields::read_nullable(hRead, state.name, 100);
    c.client_flags = byte();
    c.mouse_axis0 = real();
    c.mouse_axis1 = real();
    byte(); // m_bZoomView and m_nZoomType: the Load tail forces both off.
    byte();
    s->ReadFromMessageObject(hRead); // m_hCameraObj: NULL in a save (D7); unused by the original.
    state.last_crouch = flag();
    c.last_jump = byte();
    state.forced_crouch = flag();
    c.frags = s->ReadFromMessageDWord(hRead);
    c.melee_kills = s->ReadFromMessageDWord(hRead);
    state.character = byte();
    state.skin = byte();
    for (auto* attribute : {&state.strength, &state.speed, &state.resistance, &state.magic})
        *attribute = byte();
    for (auto& attribute : c.binding_attributes)
        attribute = byte();
    for (auto& binding : c.bindings)
        binding = byte();
    state.move_state = byte();
    bool words = true;
    for (auto* value : {&c.injured_leg, &c.node_hit, &c.side_hit})
    {
        const auto word = s->ReadFromMessageWord(hRead);
        words = words && word <= 0xFF; // The original Save stores (DBYTE) values.
        *value = static_cast<std::uint8_t>(word);
    }
    c.death_timer = real();
    c.slow_death = byte();
    c.old_air = real();
    state.air = real();
    c.swimming_jump = byte();
    c.can_swim_jump = byte();
    state.imprisoned = flag();
    state.imprison_start_offset = real();
    state.imprison_length = real();
    c.burning = byte();
    c.burning_time = real();
    // Original Load broadcasts the blob to all clients: it does not know the HCLIENT yet.
    if (flag())
        if (const auto blob = s->ReadFromMessageHMessageRead(hRead))
        {
            const bool sent = s->SendClientSaveData(blob) == DE_OK;
            s->EndHMessageRead(blob);
            if (!sent)
            {
                s->RejectPayload(hRead, "Load: the client blob could not be queued");
                return;
            }
        }
    std::string error;
    if (!booleans || !words)
        s->RejectPayload(hRead, "Load: a saved boolean or (DBYTE) word is out of range");
    else if (!LoadFields(state, error) || !RestoreModelState(error))
        s->RejectPayload(hRead, "Load: " + error);
    else
    {
        // The engine record already restored the body; the saved profile replaces SINGLEPLAYER_INIT.
        restore_engine_loaded = true;
        attributes_received = true;
        crouched = move_state == MoveState::Crouch || move_state == MoveState::Crawl ||
                   move_state == MoveState::FireCrouch || move_state == MoveState::FireCrawl;
        eye_level = crouched ? 12.0f : OpenBlood2::character_profiles[m_nCharacter].eye;
    }
}
