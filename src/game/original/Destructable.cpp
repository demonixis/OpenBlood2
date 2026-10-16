// Selected shared damage/lifetime and brush trigger branches; see M2_BRUSH_SOURCES.json.
#include "Destructable.h"
#include "AI_Mgr.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include "spawn_object.h"
#include "trigger_helpers.h"
#include <algorithm>
#include <cmath>
DLink CDestructable::m_DestructableHead{};
DDWORD CDestructable::m_dwNumDestructables = 0;
CDestructable::CDestructable()
{
    if (m_dwNumDestructables == 0)
        dl_TieOff(&m_DestructableHead);
}
CDestructable::~CDestructable()
{
    if (m_Link.m_pData && m_dwNumDestructables > 0)
    {
        dl_Remove(&m_Link);
        --m_dwNumDestructables;
    }
}
DBOOL CDestructable::Init(HOBJECT object, CInventoryMgr* inventory)
{
    if (!object || !BaseClass::GetServerDE())
        return DFALSE;
    m_hObject = object;
    if (!m_Link.m_pData)
    {
        dl_Insert(&m_DestructableHead, &m_Link);
        m_Link.m_pData = this;
        ++m_dwNumDestructables;
    }
    m_pInventoryMgr = inventory;
    return DTRUE;
}
HOBJECT CDestructable::GetLastDamager()
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(last_damager_))
        last_damager_ = nullptr;
    return last_damager_;
}
void CDestructable::CalculateResistance(DBYTE value)
{
    if (value == 1)
        m_fResistance = 1.20f;
    else if (value == 2)
        m_fResistance = 1.10f;
    else if (value == 3)
        m_fResistance = 1.0f;
    else if (value == 4)
        m_fResistance = 0.9f;
    else if (value == 5)
        m_fResistance = 0.8f;
    else if (value >= 6)
        m_fResistance = 0.7f;
}

void CDestructable::ApplyDamagePhysics(float damage, DVector direction)
{
    using namespace OpenBlood2;
    auto* server = BaseClass::GetServerDE();
    const auto squared = vector_dot(direction, direction);
    if (!server || !server->IsObjectAlive(m_hObject) || squared < .01f)
        return;
    if (m_fMass <= 0)
        m_fMass = 1;
    last_impulse = vector_scale(direction, (damage * 2500 / m_fMass) / std::sqrt(squared));
    DVector velocity{};
    last_impulse_ok = server->GetVelocity(m_hObject, &velocity) == DE_OK;
    if (last_impulse_ok)
    {
        velocity = vector_add(velocity, last_impulse);
        last_impulse_ok = server->SetVelocity(m_hObject, &velocity) == DE_OK;
    }
    damage_impulses += last_impulse_ok;
}
DDWORD CDestructable::EngineMessageFn(LPBASECLASS owner, DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE && (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP))
    {
        auto* server = BaseClass::GetServerDE();
        GenericProp prop;
        auto text = [&](const char* key, std::string& value)
        {
            if (server->GetPropGeneric(key, &prop) == DE_OK)
                value = prop.m_String;
        };
        text("DamageTriggerTarget", damage_target);
        text("DamageTriggerMessage", damage_message);
        text("DeathTriggerTarget", death_target);
        text("DeathTriggerMessage", death_message);
        text("SpawnObject", spawn_object);
        if (server->GetPropGeneric("SpawnObjectVel", &prop) == DE_OK)
            spawn_velocity = prop.m_Vec;
        spawn_valid = spawn_object.empty() ||
                      (server->ValidateSpawnObject(spawn_object) && OpenBlood2::physics_vector(spawn_velocity));
        if (server->GetPropGeneric("CanDamage", &prop) == DE_OK)
            can_damage = prop.m_Bool != 0;
        if (server->GetPropGeneric("TriggerDestroyOnly", &prop) == DE_OK)
            trigger_only = prop.m_Bool != 0;
    }
    if (message == MID_UPDATE)
    {
        m_fLastDamageAmount = m_fLastDamagePercent = 0; // Original per-update accumulators.
        if (m_bDead && m_fDeathDelay > 0)
            m_fDeathDelay -= BaseClass::GetServerDE()->GetFrameTime();
    }
    return Aggregate::EngineMessageFn(owner, message, data, reason);
}
void CDestructable::SendTrigger(const std::string& target, const std::string& message)
{
    auto* server = BaseClass::GetServerDE();
    if (target.empty() || !server->IsObjectAlive(m_hObject))
        return;
    auto name = server->CreateString(target.c_str());
    auto text = server->CreateString(message.c_str());
    SendTriggerMsgToObjects(server->HandleToObject(m_hObject), name, text);
    if (name)
        server->FreeString(name);
    if (text)
        server->FreeString(text);
}
// Shared damage; production NPC localization is in AIHitReaction.cpp.
// Player/NPC impulses, powerups and complete death reactions remain bounded separately.
DDWORD CDestructable::ObjectMessageFn(LPBASECLASS owner, HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto* server = BaseClass::GetServerDE();
        auto text = server->ReadFromMessageHString(read);
        const auto* raw = text ? server->GetStringData(text) : nullptr;
        if (raw && !OpenBlood2::CaseLess{}(raw, "KILL") && !OpenBlood2::CaseLess{}("KILL", raw) &&
            server->IsObjectAlive(sender) && !handling_damage)
        {
            auto output =
                server->StartMessageToObject(server->HandleToObject(sender), m_hObject, OpenBlood2::damage_message);
            OpenBlood2::Damage value{{}, {}, 1, OpenBlood2::DamageType::Death, sender};
            if (output)
            {
                server->WriteDamage(output, value);
                server->EndMessage(output);
            }
        }
        if (text)
            server->FreeString(text);
    }
    if (message == OpenBlood2::damage_message)
    {
        OpenBlood2::Damage damage;
        if (!BaseClass::GetServerDE()->ReadDamage(read, damage))
            return 0;
        if (!m_bGodMode && !m_bDead && can_damage && !handling_damage &&
            (!trigger_only || damage.type == OpenBlood2::DamageType::Death))
        {
            handling_damage = true;
            auto amount = damage.amount * m_fResistance;
            if (damage.type == OpenBlood2::DamageType::Death)
            {
                m_fArmorPoints = 0;
                m_fDeathHitPoints = 0;
                amount = m_fHitPoints;
            }
            else if (ai_hit_localization)
            {
                auto* ai = dynamic_cast<AI_Mgr*>(owner);
                if (!ai || !ai->AnimSound() || !LocalizeHit(damage, *ai->AnimSound()))
                {
                    handling_damage = false;
                    if (ai)
                        ai->RuntimeFailure(hit.error.empty() ? "NPC hit table unavailable" : hit.error);
                    return 0;
                }
                node_hit = hit.node;
                side_hit = hit.side;
                hit.head_kill = node_hit == 1 && ai->m_bCabal;
                if (hit.head_kill)
                    m_fHitPoints = 0; // Original Cabal headshot bypasses armor before ordinary absorption.
                amount *= hit.multiplier;
            }
            if (damage.type != OpenBlood2::DamageType::Death && m_bNighInvulnerable &&
                dynamic_cast<CBaseCharacter*>(owner))
                amount *= .05f; // Original character modifier, before impulses and armor.
            if (apply_damage_physics && damage.type != OpenBlood2::DamageType::Death)
                ApplyDamagePhysics(amount, damage.direction);
            if (m_fArmorPoints > 0 && damage.type != OpenBlood2::DamageType::Suffocate)
            {
                const auto ratio = m_fArmorPoints <= 25    ? .3f
                                   : m_fArmorPoints <= 50  ? .5f
                                   : m_fArmorPoints <= 100 ? .7f
                                   : m_fArmorPoints <= 150 ? .8f
                                                           : .9f;
                auto absorb = amount * ratio;
                m_fArmorPoints -= absorb;
                if (m_fArmorPoints < 0)
                {
                    absorb += m_fArmorPoints;
                    m_fArmorPoints = 0;
                }
                amount -= absorb;
            }
            amount = std::max(amount, 0.0f);
            m_fHitPoints -= amount;
            if (damage.type != OpenBlood2::DamageType::Death)
                m_fDeathHitPoints -= amount;
            if (damage.type == OpenBlood2::DamageType::Death)
            {
                // Source instant-death branch initializes these before adding the
                // incoming resisted packet, even if current HP is below maximum.
                m_fLastDamageAmount = m_fMaxHitPoints + damage.amount * m_fResistance;
                m_fLastDamagePercent = 100;
                if (m_fMaxHitPoints > 0)
                    m_fLastDamagePercent += damage.amount * m_fResistance / m_fMaxHitPoints;
            }
            else
            {
                m_fLastDamageAmount += amount;
                if (m_fMaxHitPoints > 0)
                    m_fLastDamagePercent += amount / m_fMaxHitPoints;
            }
            last_damage = damage;
            if (amount != 0)
                reaction_damage_type = damage.type;
            ++accepted_damage;
            // Original AI-only retaliation identity.
            // A checked token avoids sharing/breaking the AI target's directed link.
            if (dynamic_cast<AI_Mgr*>(owner) && damage.type != OpenBlood2::DamageType::Death)
                last_damager_ = damage.attacker;
            if (m_fHitPoints <= 0)
            {
                m_bDead = DTRUE;
                m_fHitPoints = 0;
                HandleDestruction();
            }
            if (BaseClass::GetServerDE()->IsObjectAlive(m_hObject))
                SendTrigger(damage_target, damage_message);
            handling_damage = false;
        }
    }
    return Aggregate::ObjectMessageFn(owner, sender, message, read);
}

void CDestructable::HandleDestruction()
{
    auto* server = BaseClass::GetServerDE();
    OpenBlood2::SpawnReport report;
    if (!spawn_object.empty())
    {
        auto name = server->GetObjectName(m_hObject);
        report.source = name ? name : "";
        report.request = spawn_object;
        report.velocity = spawn_velocity;
    }
    SendTrigger(death_target, death_message);
    if (spawn_object.empty())
        return;
    DRotation rotation;
    if (!server->IsObjectAlive(m_hObject))
        report.outcome = "Cancelled: source removed by death trigger";
    else
    {
        server->GetObjectPos(m_hObject, &report.position);
        if (server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
            report.outcome = "Failed: source rotation unavailable";
        else
        {
            report.child = SpawnObject(spawn_object, report.position, rotation, spawn_velocity);
            report.outcome = report.child ? "Created" : "Failed: object creation or velocity";
        }
    }
    server->RecordSpawn(report);
}
namespace
{
    bool add_stat(float& value, float cap, float amount)
    {
        if (!std::isfinite(amount) || amount < 0 || amount > 1e6f || value >= cap)
            return false;
        value = std::min(value + amount, cap);
        return true;
    }
} // namespace
bool CDestructable::Heal(float amount)
{
    if (!add_stat(m_fHitPoints, m_fMaxHitPoints, amount))
        return false;
    m_bDead = DFALSE;
    m_fDeathHitPoints = m_fHitPoints;
    return true;
}
bool CDestructable::MegaHeal(float amount)
{
    if (!add_stat(m_fHitPoints, m_fMaxMegaHitPoints, amount))
        return false;
    m_bDead = DFALSE;
    m_fDeathHitPoints = m_fHitPoints;
    return true;
}
bool CDestructable::AddWard(float amount)
{
    return add_stat(m_fArmorPoints, m_fMaxArmorPoints, amount);
}
bool CDestructable::AddNecroWard(float amount)
{
    return add_stat(m_fArmorPoints, m_fMaxNecroArmorPoints, amount);
}
