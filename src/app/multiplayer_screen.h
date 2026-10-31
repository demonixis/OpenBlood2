#pragma once
#include "app/multiplayer_setup.h"
#include "net/discovery.h"
struct ImFont;
namespace OpenBlood2
{
    class MultiplayerScreen
    {
      public:
        bool open(const Resources&, const Config&, bool in_world, std::string& error);
        void close();
        void draw();
        bool active = false;
        std::optional<Options> pending;
        MultiplayerSetup setup;
        ImFont* font = nullptr;

      private:
        Net::LanDiscovery browser_;
        bool browsing_ = false, replaces_solo_ = false;
        double refresh_ = -10;
        std::string error_, discovery_error_;
        void start(bool host);
    };
} // namespace OpenBlood2
