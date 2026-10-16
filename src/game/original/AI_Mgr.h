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
} // namespace OpenBlood2

// Selective original AI_Mgr::InitStatics/OnStringKey port. No production AI class
// registration, state machine, perception, Fire or MC_Extra implementation yet.
class AI_Mgr : public CBaseCharacter
{
  public:
    static constexpr int StateIdle = 5, StateScript = 101;
    bool InitStatics(std::shared_ptr<CAnim_Sound>);
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
