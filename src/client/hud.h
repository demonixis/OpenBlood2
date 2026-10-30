#pragma once
#include "client/hud_values.h"
#include "client/world.h"
namespace OpenBlood2
{
    // Presentation values only, read from the checked typed client snapshot.
    struct HudState
    {
        PlayerStats stats;
        bool visible = false, crosshair_visible = false;
        DBYTE ctf_team = 0, ctf_flag = 0;
        DBYTE crosshair = 1; // Original m_nCrosshair (zoom overrides it with 98/99).
        // Latest SMSG_ITEMCHANGED values, the message and show counters, and the client clock.
        InventoryItems items;
        std::uint64_t item_messages = 0, item_shows = 0;
        float time = 0;
        // A new world hides the weapon tabs (original CNewStatusBar::Reset).
        std::uint64_t world_revision = 0;
    };
    HudState hud_state(const PlayerStats&, bool in_world, bool external_camera, bool dead = false,
                       bool show_crosshair = true, DBYTE crosshair = 1, DBYTE ctf_team = 0, DBYTE ctf_flag = 0);
    bool valid_hud_stats(const PlayerStats&);
    bool valid_hud_items(const InventoryItems&);
    class ClientHud
    {
      public:
        bool begin(std::uint64_t world);
        bool bind(ClientObjectId);
        void unbind()
        {
            owner_.object = {};
            stats = {};
            items = {};
        }
        bool receive(ClientObjectId, std::uint64_t sequence, const PlayerStats&);
        bool receive(ClientObjectId, std::uint64_t sequence, const InventoryItems&);
        ClientObjectId owner() const
        {
            return owner_;
        }
        std::uint64_t sequence() const
        {
            return sequence_;
        }
        PlayerStats stats;
        InventoryItems items;
        std::uint64_t item_messages = 0, item_shows = 0;

      private:
        bool accepts(ClientObjectId, std::uint64_t) const;
        ClientObjectId owner_;
        std::uint64_t sequence_ = 0;
    };
} // namespace OpenBlood2
