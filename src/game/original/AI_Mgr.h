#pragma once
#include "PlayerObj.h"
#include <memory>

namespace OpenBlood2
{
    enum class AIKeyOutcome
    {
        None,
        Played,
        ChanceSkipped,
        VoiceBusy,
        CameraBlocked,
        ScriptBlocked,
        NoTarget,
        Unavailable,
        Invalid,
        PlaybackFailed
    };
    const char* ai_key_outcome_name(AIKeyOutcome);
    enum class AISightOutcome
    {
        None,
        Body,
        Head,
        Blocked,
        Miss,
        Invalid,
        Unavailable
    };
    const char* ai_sight_outcome_name(AISightOutcome);
    struct AISight
    {
        AISightOutcome outcome = AISightOutcome::None;
        HOBJECT target = nullptr;
        DVector origin{}, body{}, head{};
        SegmentHit body_hit, head_hit;
        SegmentResult body_result = SegmentResult::Miss, head_result = SegmentResult::Miss;
        bool origin_head = false, retried_head = false;
        unsigned rays = 0;
    };
} // namespace OpenBlood2

// Selective original AI_Mgr::InitStatics/OnStringKey port. No production AI class
// registration, state machine, target acquisition, Fire or MC_Extra implementation yet.
// Selected CanSee queries are documented in AI_SIGHT_CONTRACTS.md.
class AI_Mgr : public CBaseCharacter
{
  public:
    static constexpr int StateIdle = 5, StateScript = 101;
    bool InitStatics(std::shared_ptr<CAnim_Sound>);
    bool CanSee(HOBJECT);
    OpenBlood2::AISight sight;
    std::uint64_t sight_queries = 0;
    const CAnim_Sound* AnimSound() const
    {
        return m_pAnim_Sound.get();
    }
    int m_nState = StateIdle;
    HOBJECT m_hTarget = nullptr;
    OpenBlood2::AIKeyOutcome key_outcome = OpenBlood2::AIKeyOutcome::None;
    std::uint64_t model_keys = 0, audio_keys = 0, audio_starts = 0, key_failures = 0;
    std::uint64_t camera_suppressed = 0, script_suppressed = 0, chance_suppressed = 0;
    std::uint64_t busy_voices = 0, no_target = 0, unavailable_keys = 0;
    int selected_variant = 0, chance_roll = -1;
    std::string ai_key, ai_key_error, ai_sound_resource;

  protected:
    bool OnStringKey(ArgList*) override;

  private:
    // Original subclasses share class/gender tables and their retained voice.
    // Shared ownership replaces a borrowed pointer, preserving sharing safely.
    std::shared_ptr<CAnim_Sound> m_pAnim_Sound;
};
