#pragma once
#include "Anim_Sound.h"
#include "Destructable.h"

// Original corpse lifetime, damage, limb loss and burning; see death_effect_contracts.md.
class CCorpse : public B2BaseClass
{
  public:
    CCorpse();
    ~CCorpse() override;
    bool Setup(std::string_view sound_root, bool limb_loss, HOBJECT fire = nullptr);
    void ClearBurn();
    bool UpdateBurn();
    bool HandleDamage(const OpenBlood2::Damage&);
    int CalculateHitLimb(DVector, DVector);
    bool dead = false;
    HOBJECT fire_source = nullptr, smoke_source = nullptr, last_limb = nullptr;
    HATTACHMENT burn_attachment = 0;
    OpenBlood2::SoundId burn_sound = 0;
    float burn_start = 0, smoke_start = 0;
    std::uint64_t fire_effect = 0, smoke_effect = 0, gib_events = 0, limbs_created = 0;
    int hit_limb = -1;
    std::string burn_error, gib_error;
    bool OnStringKey(ArgList*);
    bool initialized = false, configured = false, animation_done = false;
    bool limb_loss = true;
    OpenBlood2::BodyPlacement placement;
    float remove_time = 0;
    CDestructable m_damage;
    CAnim_Sound measurements;
    OpenBlood2::SoundId last_sound = 0; // Inspection only; original key omits GETHANDLE.
    std::uint64_t updates = 0, sound_attempts = 0, sound_starts = 0;
    std::string sound_root, sound_resource, sound_error, error, damage_error;
    OpenBlood2::SegmentResult pool_result = OpenBlood2::SegmentResult::Invalid;
    OpenBlood2::SegmentHit pool_hit;
    bool pool_requested = false;
    std::uint64_t pool_event = 0;
    std::string pool_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    bool Update();
};
