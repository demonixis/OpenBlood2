#pragma once
#include "B2BaseClass.h"
#include "KeyData.h"
#include <vector>
// Selected source rules; bounded storage and diagnostics: keyframer_contracts.md. Save/Load follow
// the original block (save_load_contracts lot 7).
class KeyFramer : public B2BaseClass
{
  public:
    ~KeyFramer() override;
    struct Target
    {
        HOBJECT object = nullptr;
        DVector offset;
        DRotation rotation_offset;
        OpenBlood2::MoverResult result = OpenBlood2::MoverResult::Unavailable;
        std::uint64_t moves = 0, attempts = 0;
    };
    void GoActive();
    void Pause()
    {
        active = false;
        ++revision;
    }
    void Resume()
    {
        active = true;
        ++revision;
    }
    std::string object_name, base_key_name, error, last_command, last_print, last_sound;
    bool start_active = false, looping = false, active = false, initialized = false;
    float time = 0;
    std::size_t next = 0;
    std::vector<KeyData> keys;
    std::vector<Target> targets;
    std::uint64_t processed = 0, sound_requests = 0, print_requests = 0, message_keys = 0, unresolved_messages = 0;
    OpenBlood2::SoundId sound_voice = 0;
    std::uint64_t sound_starts = 0, sound_failures = 0;
    std::string sound_error;
    HOBJECT sound_target = nullptr;
    float sound_radius = 0;
    bool sound_loop = false;
    std::size_t last_message_matches = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool BuildKeyList();
    void RestoreTargets();
    void ProcessKey(const KeyData&);
    void Update();
    void Save(HMESSAGEWRITE);
    void Load(HMESSAGEREAD);
    // Original m_pPosition1/m_pPosition2 as key indices, -1 for NULL. `cursor` is the
    // original m_pCurKey being set (GoActive), so `next` is meaningful.
    std::ptrdiff_t position1 = -1, position2 = -1;
    bool cursor = false;
    std::uint64_t revision = 0;
    // A restored controller rebuilds its target list by name at its next update, as the
    // original rebuilt m_pObjectList; until then the saved offsets are kept verbatim.
    bool rebuild_targets = false;
    std::vector<std::pair<DVector, DRotation>> saved_offsets;
};
