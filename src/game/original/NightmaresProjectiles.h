#pragma once
#include "Grenade.h"

class CGasGrenade final : public CGrenade
{
  public:
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    bool smoking = false;
    float smoke_start = 0, last_gas_damage = 0;
    unsigned gas_ticks = 0, gas_deliveries = 0;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;

  private:
    bool DamageAndWonky();
};

class CFlayerChain final : public CProjectile
{
  public:
    enum class State
    {
        Tracking,
        Attached,
        Retracting
    };
    CFlayerChain();
    ~CFlayerChain() override;
    bool SetTarget(HOBJECT target);
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    State state = State::Tracking;
    HOBJECT target = nullptr, gib = nullptr;
    HATTACHMENT gib_attachment = 0;
    float hit_time = 0, last_damage = 0, retract_time = 0;
    DVector hook_start{}, hook_end{};
    unsigned damage_deliveries = 0;
    std::string effect_error, sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;

  private:
    bool Track(DVector position);
    void Release();
    void FreeTarget(bool reset);
    void CreateGib();
    OpenBlood2::SoundId voice = 0;
    bool first_update = true;
};

class CFlayerPortal final : public CProjectile
{
  public:
    CFlayerPortal();
    ~CFlayerPortal() override;
    bool AdvanceFlight(double) override
    {
        return !failed;
    }
    unsigned chains_created = 0;
    HOBJECT last_chain = nullptr;
    std::string sound_error;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override {}

  private:
    bool LaunchChain(DVector position, HOBJECT target);
    OpenBlood2::SoundId voice = 0;
    float last_chain_time = 0;
};
