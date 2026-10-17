#pragma once
#include "Anim_Sound.h"
#include "Destructable.h"

// Selected original CCorpse lifetime/animation/audio. Damage, limbs and fire are
// explicit unavailable branches; see CORPSE_CONTRACTS.md.
class CCorpse : public B2BaseClass
{
  public:
    CCorpse();
    ~CCorpse() override = default;
    bool Setup(std::string_view sound_root, bool limb_loss);
    bool OnStringKey(ArgList*);
    bool initialized = false, configured = false, animation_done = false;
    bool limb_loss = true;
    OpenBlood2::BodyPlacement placement;
    float remove_time = 0;
    CDestructable m_damage;
    CAnim_Sound measurements;
    OpenBlood2::SoundId last_sound = 0; // Inspection only; original key omits GETHANDLE.
    std::uint64_t updates = 0, sound_attempts = 0, sound_starts = 0, damage_refusals = 0;
    std::string sound_root, sound_resource, sound_error, error, damage_error;
    OpenBlood2::SegmentResult pool_result = OpenBlood2::SegmentResult::Invalid;
    OpenBlood2::SegmentHit pool_hit;
    bool pool_requested = false;
    std::string pool_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    bool Update();
};
