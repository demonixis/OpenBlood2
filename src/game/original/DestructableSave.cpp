// Original Destructable.cpp:1134-1245. Typed owned values replace packet reads;
// validation precedes mutation, and no old object/attacker identity is serialized.
#include "Destructable.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <cmath>

namespace
{
    bool valid(const OpenBlood2::DestructableState& state, std::string& error)
    {
        for (float value :
             {state.mass, state.max_health, state.max_mega_health, state.health, state.max_armor, state.max_necro_armor,
              state.armor, state.resistance, state.last_amount, state.last_percent})
            if (!std::isfinite(value) || value < 0)
            {
                error = "Invalid saved damage scalar";
                return false;
            }
        // Source death-health may be negative; a delay can overshoot below zero.
        if (!std::isfinite(state.death_health) || !std::isfinite(state.death_delay) ||
            !OpenBlood2::physics_vector(state.last_direction) || !OpenBlood2::physics_vector(state.spawn_velocity))
        {
            error = "Invalid saved death counter, delay or damage vector";
            return false;
        }
        using OpenBlood2::DamageType;
        switch (state.last_type)
        {
            case DamageType::Normal:
            case DamageType::Fire:
            case DamageType::Melee:
            case DamageType::Explode:
            case DamageType::Suffocate:
            case DamageType::Death:
            case DamageType::FreeFall:
            case DamageType::Blind:
            case DamageType::Slow:
            case DamageType::Acid:
                break;
            default:
                error = "Unsupported saved damage type";
                return false;
        }
        for (const auto* text : {&state.damage_target, &state.damage_message, &state.death_target, &state.death_message,
                                 &state.spawn_object})
            if (text->size() > 100 || text->find('\0') != std::string::npos)
            {
                error = "Invalid saved damage string";
                return false;
            }
        return true;
    }
} // namespace

bool CDestructable::Save(OpenBlood2::DestructableState& output, std::string& error) const
{
    error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || handling_damage || !spawn_valid)
    {
        error = "Damage save requires a live owner, valid spawn and no active damage callback";
        return false;
    }
    OpenBlood2::DestructableState state;
    state.apply_damage_physics = apply_damage_physics;
    state.mass = m_fMass;
    state.max_health = m_fMaxHitPoints;
    state.max_mega_health = m_fMaxMegaHitPoints;
    state.health = can_damage ? m_fHitPoints : 1.0e6f;
    state.death_health = m_fDeathHitPoints;
    state.max_armor = m_fMaxArmorPoints;
    state.max_necro_armor = m_fMaxNecroArmorPoints;
    state.armor = m_fArmorPoints;
    state.resistance = m_fResistance;
    state.dead = m_bDead != 0;
    state.nigh_invulnerable = m_bNighInvulnerable;
    state.last_type = reaction_damage_type;
    state.last_location = last_damage_location;
    state.last_direction = last_damage.direction;
    state.last_amount = m_fLastDamageAmount;
    state.last_percent = m_fLastDamagePercent;
    state.damage_target = damage_target;
    state.damage_message = damage_message;
    state.death_target = death_target;
    state.death_message = death_message;
    state.spawn_object = spawn_object;
    state.spawn_velocity = spawn_velocity;
    state.god_mode = m_bGodMode;
    state.trigger_only = trigger_only;
    state.death_delay = m_fDeathDelay;
    if (!valid(state, error))
        return false;
    output = std::move(state);
    return true;
}

bool CDestructable::Load(const OpenBlood2::DestructableState& state, std::string& error)
{
    error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || handling_damage)
    {
        error = "Damage restore requires a live destination outside damage callbacks";
        return false;
    }
    const auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(m_hObject));
    if (player && player->GetClient())
    {
        error = "Damage restore requires a player before client association";
        return false;
    }
    if (!valid(state, error))
        return false;
    if (!state.spawn_object.empty() && !server->ValidateSpawnObject(state.spawn_object))
    {
        error = "Saved destruction spawn is unavailable";
        return false;
    }
    apply_damage_physics = state.apply_damage_physics;
    m_fMass = state.mass;
    m_fMaxHitPoints = state.max_health;
    m_fMaxMegaHitPoints = state.max_mega_health;
    m_fHitPoints = state.health;
    m_fDeathHitPoints = state.death_health;
    m_fMaxArmorPoints = state.max_armor;
    m_fMaxNecroArmorPoints = state.max_necro_armor;
    m_fArmorPoints = state.armor;
    m_fResistance = state.resistance;
    m_bDead = state.dead;
    m_bNighInvulnerable = state.nigh_invulnerable;
    reaction_damage_type = state.last_type;
    last_damage_location = state.last_location;
    last_damage = {}; // Raw delivery/attacker/position are not original saved fields.
    last_damage.direction = state.last_direction;
    m_fLastDamageAmount = state.last_amount;
    m_fLastDamagePercent = state.last_percent;
    damage_target = state.damage_target;
    damage_message = state.damage_message;
    death_target = state.death_target;
    death_message = state.death_message;
    spawn_object = state.spawn_object;
    spawn_velocity = state.spawn_velocity;
    spawn_valid = true;
    m_bGodMode = state.god_mode;
    trigger_only = state.trigger_only;
    m_fDeathDelay = state.death_delay;
    can_damage = m_fHitPoints < 1.0e6f; // Original sentinel, not a clamp to maximum HP.
    last_damager_ = nullptr;
    hit = {}; // Old diagnostic pose queries cannot describe the restored destination.
    if (!m_Link.m_pData)
    {
        dl_Insert(&m_DestructableHead, &m_Link);
        m_Link.m_pData = this;
        ++m_dwNumDestructables;
    }
    return true;
}
