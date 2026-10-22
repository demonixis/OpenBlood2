// Selective original CBloodClientShell loading screen: GetWorldTitle, DrawLoadingScreen and
// LoadScreenData. See Documentation/contracts/loading_screen_contracts.md.
#pragma once
#include "engine/hud_surface.h"
#include "engine/string_resources.h"
#include <memory>
#include <optional>

namespace OpenBlood2
{
    // GetWorldTitle's results for one StartNewWorld.
    struct WorldLoadInfo
    {
        std::string title;           // m_hstrTitle: the last "worldname", else the requested world name.
        std::string load_screen;     // m_hstrLoadScreen: empty selects LoadScreen.pcx with Loading_01.pcx.
        unsigned load_screen_id = 0; // m_nLoadScreenID: nonzero pauses into GS_WAITING after the load.
    };
    // GetWorldTitle over a DAT world-info string; `world` is the name given to StartNewWorld.
    WorldLoadInfo world_load_info(std::string_view info, std::string_view world, bool demo);

    // Original DrawLoadingScreen on the 640x480 reference screen (GS_LOADINGLEVEL and GS_WAITING).
    class LoadingScreen
    {
      public:
        static constexpr unsigned width = 640, height = 480;
        static constexpr unsigned max_load_screen_id = 34; // MAX_LOADSCREEN_DATA_TYPES - 1.
        explicit LoadingScreen(const Resources& resources) : resources_(resources) {}
        // StartNewWorld: GetWorldTitle from the destination DAT header, then the first
        // DrawLoadingScreen's background (m_hLoadingScreen) with its title and bumper text.
        // `action` is GAMETYPE_ACTION, which draws no bumper text.
        bool begin(std::string_view world, bool action, std::string& error);
        // One DrawLoadingScreen into surface(): with a load-screen ID, the GS_WAITING prompt
        // pulsing at `time` seconds, else a loading message drawn at random on every call.
        bool draw(bool waiting, double time, std::string& error);
        // PostUpdate outside the loading states: the surface is deleted.
        void end()
        {
            active_ = false;
            played_waiting_sound = false;
        }
        bool active() const
        {
            return active_;
        }
        const HudSurface& surface() const
        {
            return screen_;
        }
        std::uint64_t revision() const
        {
            return revision_;
        }
        const WorldLoadInfo& info() const
        {
            return info_;
        }
        std::uint32_t random_state = 0x4c4f4144; // Private cosmetic stream (GetRandom); injectable in tests.
        bool played_waiting_sound = false;       // m_bPlayedWaitingSound: the caller plays esc01.wav once.
        unsigned message_id = 0;                 // The last loading message drawn (IDS_LOADING_MESSAGE_n).

      private:
        const Resources& resources_;
        std::unique_ptr<HudCanvas> canvas_;
        HudFont title_font_, text_font_;
        std::optional<StringResources> strings_;
        HudSurface background_, screen_;
        WorldLoadInfo info_;
        bool active_ = false;
        double fade_time_ = 0; // m_fLoadingFadeTime
        bool fade_up_ = false; // m_bLoadingFadeUp
        std::uint64_t revision_ = 0;
        bool lookup(unsigned id, std::string& out, std::string& error) const;
    };
} // namespace OpenBlood2
