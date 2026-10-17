#pragma once
// Selected original VoiceMgr.cpp/.h rules. Copyright (c) 1998 Monolith Productions Inc.
#include "engine/resources.h"
#include "server_de.h"
#include <array>
#include <random>

class CVoiceGroup
{
  public:
    void Init(const OpenBlood2::Resources&, std::string_view standard, std::string_view custom);
    const std::string* GetRandomSoundString(std::mt19937&);
    std::vector<std::string> sounds;
    int last = -1;
    bool custom = false;
    unsigned rejected = 0;
};
class CVoiceMgr
{
  public:
    explicit CVoiceMgr(ServerDE& server) : server_(server) {}
    ~CVoiceMgr()
    {
        StopAll();
    }
    CVoiceMgr(const CVoiceMgr&) = delete;
    CVoiceMgr& operator=(const CVoiceMgr&) = delete;
    void Init(const OpenBlood2::Resources&);
    void StopAll();
    void Reset();
    void SetNextPlayTime(float delta);
    void InheritHistory(const CVoiceMgr&);
    bool PlayEventSound(HOBJECT player, int character, int event, bool external_camera);
    bool PlayUniqueSound(HOBJECT player, int character, int unique, bool external_camera);
    bool PlaySound(HOBJECT player, std::string_view resource, bool stream, float delay = .5f);
    bool IsOkToPlaySound(bool external_camera);
    std::array<std::array<CVoiceGroup, 12>, 4> groups;
    std::array<std::array<bool, 9>, 4> played{};
    OpenBlood2::SoundId voice = 0;
    float next_play_time = 0;
    bool on = true;
    std::uint64_t requests = 0, starts = 0, refused = 0, failures = 0, stops = 0;
    std::string last_resource, reason, error;

  private:
    ServerDE& server_;
    std::mt19937 random_{0xB200C01u}; // Client-only portable randomness, independent of gameplay.
};
