#pragma once
#include "cpp_aggregate_de.h"
#include "dlink.h"
#include "game/weapon_messages.h"
#include <string>
class CInventoryMgr;
class CDestructable : public Aggregate
{
  public:
    CDestructable();
    ~CDestructable() override;
    DBOOL Init(HOBJECT, CInventoryMgr* = nullptr);
    void SetHitPoints(float hp)
    {
        m_fHitPoints = hp;
        if (hp)
            m_bDead = DFALSE;
    }
    void SetArmorPoints(float armor)
    {
        m_fArmorPoints = armor;
    }
    bool IsDead() const
    {
        return m_bDead != 0;
    }
    HOBJECT GetLastDamager();
    void ClearLastDamager()
    {
        last_damager_ = nullptr;
    }
    void CalculateResistance(DBYTE);
    bool Heal(float);
    bool MegaHeal(float);
    bool AddWard(float);
    bool AddNecroWard(float);
    void HandleDestruction();
    std::string spawn_object;
    DVector spawn_velocity{};
    bool spawn_valid = true;
    void ApplyDamagePhysics(float, DVector);
    bool apply_damage_physics = false, last_impulse_ok = true;
    std::uint64_t damage_impulses = 0;
    DVector last_impulse{};
    HOBJECT m_hObject = nullptr;
    CInventoryMgr* m_pInventoryMgr = nullptr;
    DLink m_Link{};
    static DLink m_DestructableHead;
    static DDWORD m_dwNumDestructables;
    DBOOL m_bDead = DFALSE;
    float m_fHitPoints = 1, m_fArmorPoints = 0, m_fMass = 1, m_fResistance = 1;
    bool m_bGodMode = false;
    float m_fLastDamageAmount = 0;
    OpenBlood2::Damage last_damage;
    bool can_damage = true, trigger_only = false, handling_damage = false;
    std::string damage_target, damage_message, death_target, death_message;
    void SendTrigger(const std::string& target, const std::string& message);

  protected:
    DDWORD EngineMessageFn(LPBASECLASS, DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(LPBASECLASS, HOBJECT, DDWORD, HMESSAGEREAD) override;

  public:
  private:
    HOBJECT last_damager_ = nullptr; // Checked identity, never a borrowed C++ pointer.
  public:
    float m_fMaxHitPoints = 1, m_fMaxMegaHitPoints = 1, m_fMaxArmorPoints = 1, m_fMaxNecroArmorPoints = 1;
};
