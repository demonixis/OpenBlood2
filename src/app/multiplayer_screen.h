#pragma once
#include "app/multiplayer_setup.h"
#include "game/menu.h"
#include "net/discovery.h"
#include <SDL3/SDL_events.h>

namespace OpenBlood2
{
    class MultiplayerScreen
    {
      public:
        ~MultiplayerScreen();
        bool open(Menu&, const Resources&, const Config&, const Localization&, const LocalizedFont*,
                  std::string& error);
        void close();
        void update(Menu&);
        void command(Menu&, const MenuCommand&, SDL_Window*);
        bool editing() const
        {
            return edit_window_ != nullptr;
        }
        void cancel_edit(Menu&);
        void edit_event(Menu&, const SDL_Event&, bool accept, bool back);

        std::optional<Options> pending;

      private:
        enum Option : int
        {
            Join,
            Host,
            Profile,
            Name,
            Character,
            Team,
            Mode,
            Map,
            Port,
            End,
            Score,
            Minutes,
            FriendlyFire,
            NegativeFrags,
            FlagValue,
            OnlyFlags,
            TeamSize,
            Rotation,
            AddMap,
            ClearRotation,
            Start,
            Address,
            Connect,
            Lan,
            Refresh,
            Back,
            LanServer = 100
        };

        const char* label(std::string_view key, const char* english) const;
        std::string map_label(const std::string&) const;
        void start(bool host);
        void refresh();
        void rebuild(Menu&);
        void begin_edit(Menu&, Option, SDL_Window*);
        void finish_edit(Menu&);
        void show_edit(Menu&);
        int* number(Option, int& minimum, int& maximum);

        MultiplayerSetup setup_;
        Net::LanDiscovery browser_;
        const Localization* locale_ = nullptr;
        const LocalizedFont* font_ = nullptr;
        MenuPage page_ = MenuPage::Main;
        bool browsing_ = false, dirty_ = false, custom_name_ = false;
        double refresh_ = -10;
        std::string error_, discovery_error_;
        std::string_view error_key_;
        std::map<std::string, std::string, std::less<>> map_titles_;
        std::vector<Net::ServerAddress> lan_rows_;
        SDL_Window* edit_window_ = nullptr;
        Option field_ = Name;
        std::string draft_;
        std::size_t cursor_ = 0;
        bool select_all_ = false;
    };
} // namespace OpenBlood2
