#pragma once
#include "app/multiplayer_setup.h"
#include "net/discovery.h"
#include <map>
struct ImFont;
namespace OpenBlood2
{
    class Localization;
    class LocalizedFont;
    const char* multiplayer_label(const Localization*, const LocalizedFont*, std::string_view key, const char* english);

    class MultiplayerScreen
    {
      public:
        bool open(const Resources&, const Config&, bool in_world, std::string& error);
        void close();
        void draw(const Localization* = nullptr, const LocalizedFont* = nullptr, ImFont* localized_face = nullptr);
        bool active = false;
        std::optional<Options> pending;
        MultiplayerSetup setup;
        ImFont* font = nullptr;

      private:
        Net::LanDiscovery browser_;
        bool browsing_ = false, replaces_solo_ = false;
        double refresh_ = -10;
        std::string error_, discovery_error_;
        std::string_view error_key_;
        std::map<std::string, std::string, std::less<>> map_titles_;
        void start(bool host);
    };
} // namespace OpenBlood2
