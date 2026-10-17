#pragma once
#include "engine/music_asset.h"
#include <map>

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
} // namespace OpenBlood2

// Selected original client Music.cpp playlist initialization. This is an asset
// plan only, not InitMusic/LoadSong/PlayList or a successful playback facade.
class CMusic
{
  public:
    static bool BuildAssetPlan(const std::map<std::string, std::string>& properties, OpenBlood2::MusicAssetPlan&,
                               std::string& error);
};
