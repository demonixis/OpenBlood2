#pragma once
#include "engine/music_asset.h"
#include "game/player_messages.h"
#include <map>
#include <string_view>

namespace OpenBlood2
{
    struct MusicCatalog;
    class AudioMixer;
    struct MusicAssetRequest
    {
        std::string role, filename;
        MusicAssetKind kind = MusicAssetKind::Section;
    };
    struct MusicAssetPlan
    {
        std::string directory;
        std::vector<MusicAssetRequest> requests;
    };
    struct MusicLevelRequest
    {
        std::uint8_t from = 0, to = 0;
        std::string_view playlist, transition;
        bool immediate = false, loop = true;
    };
} // namespace OpenBlood2

// Selected original server parsing and client music decisions with typed services.
// Playback admission, logical level and eventual audio success remain distinct.
class CMusic
{
  public:
    // Original server command parsing and client level state, with owned typed delivery.
    static bool ParseTrigger(std::string_view, OpenBlood2::MusicCommand&, std::string&);
    bool PlayMusicLevel(std::uint8_t, OpenBlood2::MusicCatalog&, OpenBlood2::AudioMixer&);
    void HandleMusicMessage(const OpenBlood2::MusicCommand&, OpenBlood2::MusicCatalog&, OpenBlood2::AudioMixer&);
    void EnterWorld(OpenBlood2::MusicCatalog&, OpenBlood2::AudioMixer&);
    void ExitWorld(OpenBlood2::AudioMixer&);
    std::uint8_t logical_level = 0;
    std::uint64_t received = 0, requests = 0, accepted = 0, refused = 0, failures = 0, source_noops = 0;
    std::string error;
    OpenBlood2::MusicCommand last_command;
    static bool BuildAssetPlan(const std::map<std::string, std::string>& properties, OpenBlood2::MusicAssetPlan&,
                               std::string& error);
    // Original PlayMusicLevel decision only. Does not update the game's logical
    // level or pretend that the engine has accepted the requested looping list.
    static bool SelectLevel(std::uint8_t from, std::uint8_t to, OpenBlood2::MusicLevelRequest&);
};
