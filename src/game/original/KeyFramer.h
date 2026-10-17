#pragma once
#include "B2BaseClass.h"
#include "KeyData.h"
#include <vector>
// Selected source rules; bounded storage and diagnostics: keyframer_contracts.md.
class KeyFramer : public B2BaseClass
{
  public:
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
    std::size_t last_message_matches = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool BuildKeyList();
    void ProcessKey(const KeyData&);
    void Update();
    std::size_t position1 = 0, position2 = 0;
    std::uint64_t revision = 0;
    bool have_positions = false;
};
