#pragma once
#include "engine/music_asset.h"
#include <map>
#include <string_view>

namespace OpenBlood2
{
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

// Selected original client Music.cpp asset initialization and level decisions.
// Neither method claims engine InitMusic/LoadSong/PlayList success.
class CMusic
{
  public:
    static bool BuildAssetPlan(const std::map<std::string, std::string>& properties, OpenBlood2::MusicAssetPlan&,
                               std::string& error);
    // Original PlayMusicLevel decision only. Does not update the game's logical
    // level or pretend that the engine has accepted the requested looping list.
    static bool SelectLevel(std::uint8_t from, std::uint8_t to, OpenBlood2::MusicLevelRequest&);
};
