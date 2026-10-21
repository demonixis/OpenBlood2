#include "app/hud.h"
#include "app/movement_controls.h"
#include "app/ui_runtime.h"
#include "engine/capture.h"
#include "engine/config.h"
#include "game/original/ClientCrosshair.h"
#include "game/original/Weapon.h"
#include "player_fixture.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool bitmaps()
    {
        std::vector<std::uint8_t> pcx(128, 0);
        pcx[0] = 10;
        pcx[1] = 5;
        pcx[2] = 1;
        pcx[3] = 8;
        pcx[8] = 1;
        pcx[65] = 1;
        pcx[66] = 2;
        pcx.insert(pcx.end(), {1, 2, 12});
        pcx.resize(pcx.size() + 768);
        const unsigned palette = 131;
        pcx[palette + 3] = 255;
        pcx[palette + 5] = 255;
        pcx[palette + 7] = 200;
        HudSurface image;
        std::string error;
        CHECK(read_pcx(pcx, image, error) && image.width == 2 && image.height == 1);
        CHECK(image.rgba == std::vector<std::uint8_t>({255, 0, 255, 255, 0, 200, 0, 255}));
        for (unsigned variant = 0; variant < 8; ++variant)
        {
            auto bad = pcx;
            if (variant == 0)
                bad.resize(127);
            if (variant == 1)
                bad[66] = 1;
            if (variant == 2)
                bad[128] = 0xc0; // zero run
            if (variant == 3)
                bad[128] = 0xc3; // row overrun
            if (variant == 4)
                bad[130] = 0; // missing palette marker
            if (variant == 5)
                bad[65] = 4;
            if (variant == 6)
                bad.erase(bad.begin() + 129); // truncated row
            if (variant == 7)
                bad.insert(bad.begin() + 130, 0); // unexpected image bytes
            CHECK(!read_pcx(bad, image, error) && image.rgba.empty() && !error.empty());
        }
        CHECK(read_pcx(pcx, image, error));
        HudSurface dest{2, 1, std::vector<std::uint8_t>(8, 17)};
        hud_blit(dest, image, nullptr, 0, 0, true, hud_magenta);
        CHECK(dest.rgba[0] == 17 && dest.rgba[5] == 200);
        hud_blit(dest, image, nullptr, -1, 0, false, 0);
        CHECK(dest.rgba[1] == 200); // negative destination clips without wrapping
        HudRect oversized{0, 0, 1000, 1000};
        hud_blit(dest, image, &oversized, 0, 0, false, 0);
        CHECK(dest.rgba == image.rgba);
        HudFont font;
        std::vector<std::uint8_t> widths(190, 0);
        widths[0] = 1;
        widths[2 + ('0' - '!') * 2] = 2;
        CHECK(font.read_widths(widths, image, error));
        CHECK(font.offsets['0' - '!'] == 0 && font.widths['0' - '!'] == 2);
        widths[2] = 3;
        CHECK(!font.read_widths(widths, image, error) && !font.surface);
        Resources missing;
        CNewStatusBar bar(missing);
        CHECK(!bar.Init() && !bar.canvas.error.empty());
        return true;
    }
    bool values()
    {
        PlayerStats s;
        s.health = 27.9f;
        s.armor = 12.8f;
        s.ammo = 123.8f;
        s.alternate_ammo = 4;
        s.weapon = WEAP_ASSAULTRIFLE;
        s.ammo_type = AMMO_BULLET;
        s.alternate_ammo_type = AMMO_DIEBUGDIE;
        s.weapon_slot = 2;
        s.weapon_slots[2] = s.weapon;
        auto h = hud_state(s, true, false);
        CHECK(h.crosshair_visible);
        CHECK(!hud_state(s, false, false).crosshair_visible && !hud_state(s, true, true).crosshair_visible);
        CHECK(!hud_state(s, true, false, true).crosshair_visible && hud_state(s, true, false, true).visible);
        // Client m_bShowCrosshair/m_nCrosshair: zoom overrides with its cursor type.
        CHECK(!hud_state(s, true, false, false, false).crosshair_visible && h.crosshair == 1);
        CHECK(hud_state(s, true, false, false, true, 98).crosshair == 98);
        const auto rect = original_crosshair_rect({10, 20, 651, 501}, 15, 13);
        CHECK(rect.left == 323 && rect.top == 254 && rect.right == 338 && rect.bottom == 267);
        CHECK(h.visible && h.stats == s); // Presentation owns raw values; original tab code formats them.
        CHECK(!hud_state(s, false, false).visible && !hud_state(s, true, true).visible);
        s.ammo = 1234;
        s.health = -20;
        s.armor = 70000;
        h = hud_state(s, true, false);
        CHECK(h.visible && h.stats == s);
        s.weapon = WEAP_MELEE;
        s.ammo_type = s.alternate_ammo_type = AMMO_NONE;
        CHECK(hud_state(s, true, false).visible);
        s.health = std::numeric_limits<float>::quiet_NaN();
        CHECK(!hud_state(s, true, false).visible);
        CHECK(!hud_state({}, true, false).visible);
        return true;
    }
    SDL_Event key(SDL_Scancode code, bool repeat = false)
    {
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = 42;
        e.key.scancode = code;
        e.key.repeat = repeat;
        return e;
    }
    bool input()
    {
        MovementControls c;
        c.focused = true;
        for (int i = 0; i < 10; ++i)
            for (auto base : {SDL_SCANCODE_1, SDL_SCANCODE_KP_1})
            {
                auto e = key(static_cast<SDL_Scancode>(int(base) + i));
                CHECK(c.consumes(e, false) && !c.consumes(e, true));
                c.event(e, 42);
                CHECK(c.sample_weapon_slot(false) == i && c.sample_weapon_slot(false) == -1);
                e.key.repeat = true;
                c.event(e, 42);
                CHECK(c.sample_weapon_slot(false) == -1);
            }
        c.event(key(SDL_SCANCODE_1), 42);
        c.event(key(SDL_SCANCODE_0), 42);
        CHECK(c.sample_weapon_slot(false) == 9);
        c.event(key(SDL_SCANCODE_1), 99);
        CHECK(c.sample_weapon_slot(false) == -1);
        c.event(key(SDL_SCANCODE_2), 42);
        CHECK(c.sample_weapon_slot(true) == -1 && c.sample_weapon_slot(false) == -1);
        c.enabled = false;
        c.event(key(SDL_SCANCODE_3), 42);
        CHECK(c.sample_weapon_slot(false) == -1);
        c.enabled = true;
        CHECK(c.sample_weapon_slot(false) == -1);
        c.enabled = false;
        c.event(key(SDL_SCANCODE_3), 42);
        c.enabled = true;
        CHECK(c.sample_weapon_slot(false) == -1); // Disabled at event time, then enabled in the same frame.
        c.event(key(SDL_SCANCODE_4), 42, true);
        CHECK(c.sample_weapon_slot(false) == -1); // Editing then hiding the inspector cannot leak a key.

        c.event(key(SDL_SCANCODE_4), 42);
        c.player_changed();
        CHECK(c.sample_weapon_slot(false) == -1);
        c.event(key(SDL_SCANCODE_5), 42);
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        focus.window.windowID = 42;
        c.event(focus, 42);
        CHECK(c.sample_weapon_slot(false) == -1);
        c.focused = true;
        c.event(key(SDL_SCANCODE_5, true), 42);
        CHECK(c.sample_weapon_slot(false) == -1);
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && !g.command_weapon(0, error) && g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto& inv = p->m_InventoryMgr;
        CHECK(p->UpdateStats(true) && g.pump_player_messages(error));
        CHECK(g.client_shell.stats.weapon == WEAP_BERETTA && g.client_shell.stats.ammo == 50);
        CHECK(g.client_shell.stats.weapon_slots[0] == WEAP_MELEE &&
              g.client_shell.stats.weapon_slots[1] == WEAP_BERETTA);
        CHECK(g.command_weapon(0, error) && inv.m_nCurWeaponSlot == 0 && g.client_shell.weapons[0].type == WEAP_MELEE);
        // Original COMMAND_NEXTWEAPON / COMMAND_PREVWEAPON wrap over the carried slots 0..9.
        CHECK(g.cycle_weapon(true, error) && inv.m_nCurWeaponSlot == 1 &&
              g.client_shell.weapons[0].type == WEAP_BERETTA);
        CHECK(g.cycle_weapon(true, error) && inv.m_nCurWeaponSlot == 0 && g.client_shell.weapons[0].type == WEAP_MELEE);
        CHECK(g.cycle_weapon(false, error) && inv.m_nCurWeaponSlot == 1);
        CHECK(g.cycle_weapon(false, error) && inv.m_nCurWeaponSlot == 0);
        CHECK(inv.CycleWeaponSlot(true) == DBYTE(1) && inv.CycleWeaponSlot(false) == DBYTE(1));
        p->movement_blocked = true;
        CHECK(!g.cycle_weapon(true, error) && inv.m_nCurWeaponSlot == 0);
        p->movement_blocked = false;
        CHECK(p->UpdateStats(false) && g.pump_player_messages(error));
        CHECK(hud_state(g.client_shell.stats, true, false).stats.ammo_type == AMMO_NONE &&
              g.client_shell.stats.weapon_slot == 0);
        CHECK(!g.command_weapon(9, error) && !g.command_weapon(10, error) && inv.m_nCurWeaponSlot == 0);
        p->movement_blocked = true;
        CHECK(!g.command_weapon(1, error));
        p->movement_blocked = false;
        p->camera_movement_blocked = true;
        CHECK(!g.command_weapon(1, error));
        p->camera_movement_blocked = false;
        p->m_bDead = DTRUE;
        CHECK(!g.command_weapon(1, error));
        p->m_bDead = DFALSE;
        p->m_damage.m_bDead = DTRUE;
        CHECK(!g.command_weapon(1, error));
        p->m_damage.m_bDead = DFALSE;
        CHECK(g.command_weapon(1, error));
        g.client_shell.weapon_input.primary = true;
        const auto revision = inv.revision;
        CHECK(g.command_weapon(1, error) && g.client_shell.weapon_input.primary && inv.revision == revision);
        CHECK(g.command_weapon(0, error) && !g.client_shell.weapon_input.primary);
        CHECK(g.command_weapon(1, error));
        inv.SetAmmoCount(AMMO_BULLET, 0);
        p->m_damage.SetHitPoints(21);
        p->m_damage.SetArmorPoints(15);
        CHECK(p->UpdateStats(false) && g.pump_player_messages(error));
        auto h = hud_state(g.client_shell.stats, true, false);
        CHECK(h.visible && h.stats.health == 21 && h.stats.armor == 15 && h.stats.ammo == 0 &&
              h.stats.ammo_type == AMMO_BULLET && h.stats.weapon_slot == 1);
        auto s = g.client_shell.stats;
        for (int variant = 0; variant < 7; ++variant)
        {
            auto bad = s;
            if (variant == 0)
                bad.ammo = std::numeric_limits<float>::quiet_NaN();
            if (variant == 1)
                bad.alternate_ammo = -1;
            if (variant == 2)
                bad.weapon_slots[0] = 255;
            if (variant == 3)
                bad.ammo_type = 13;
            if (variant == 4)
                bad.weapon_slot = 13;
            if (variant == 5)
                bad.max_health = std::numeric_limits<float>::infinity();
            if (variant == 6)
                bad.max_armor = -1;
            CHECK(g.server.SendToClient(g.local_client, bad) == DE_INVALIDPARAMS);
        }
        CHECK(g.client_shell.stats == s && g.leave_player() && !g.command_weapon(0, error));
        CHECK(!hud_state(g.client_shell.stats, g.client_shell.m_bInWorld, false).visible);
        CHECK(g.unload_world() && !g.server.string_count() && !g.server.message_count());
        return true;
    }
    // Original item cursor (SelectNextItem/SelectPrevItem, USEINVENTORY, UpdateClient) on a
    // fixture player, through the client command path and the typed SMSG_ITEMCHANGED value.
    bool inventory(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto& inv = p->m_InventoryMgr;
        CHECK(g.pump_player_messages(error));
        CHECK(inv.CarriedItems().empty() && !inv.GetCurrentItem());
        inv.SelectNextItem();
        auto view = inv.ItemView();
        CHECK(!inv.GetCurrentItem() && !view.current && !view.next && !view.previous && !view.show);
        CHECK(inv.AddItem(5, 1) == InventoryResult::Ok && inv.AddItem(1, 100) == InventoryResult::Ok);
        auto items = inv.CarriedItems(); // Type order: medkit before binoculars.
        CHECK(items.size() == 2 && items[0].type == 1 && items[1].type == 5);
        // The original add-time rule leaves the cursor at the list head: nothing selected.
        CHECK(!inv.GetCurrentItem());
        view = inv.ItemView(); // No cursor: the head's neighbours, first and last.
        CHECK(!view.current && view.next && view.next->type == 1 && view.previous && view.previous->type == 5);
        CHECK(view.next->icon == "medkit.pcx" && view.next->name_id == 302 && view.next->charges == 100);
        inv.SelectNextItem();
        view = inv.ItemView();
        CHECK(view.show && view.current && view.current->type == 1 && view.current->highlighted_icon == "medkit_h.pcx");
        CHECK(view.next && view.next->type == 5 && !view.previous); // No wrap past the head for display.
        CHECK(!inv.ItemView().show);                                // A one-shot request.
        inv.SelectNextItem();
        CHECK(inv.GetCurrentItem()->type == 5 && !inv.ItemView().next);
        inv.SelectNextItem(); // Selection wraps.
        CHECK(inv.GetCurrentItem()->type == 1);
        inv.SelectPrevItem();
        CHECK(inv.GetCurrentItem()->type == 5);
        KeyItem key{"RedKey", "Red key", "key_red.pcx", "key_red_h.pcx", 2};
        CHECK(inv.AddKey(key) == InventoryResult::Ok);
        items = inv.CarriedItems();
        CHECK(items.size() == 3 && items[2].type == 6 && items[2].key == "RedKey" && inv.GetItemCharges(items[2]) == 2);
        view = inv.ItemView();
        CHECK(view.next && view.next->type == 6 && view.next->name == "Red key" && !view.next->name_id);
        // RemoveItem rule: the removed current item hands the cursor to the next one.
        inv.binoculars.reset();
        CHECK(inv.GetCurrentItem() && inv.GetCurrentItem()->type == 6);
        CHECK(inv.QueryKey("RedKey") == InventoryResult::Ok && inv.GetCurrentItem()->type == 6);
        CHECK(inv.QueryKey("RedKey") == InventoryResult::Ok);
        CHECK(inv.GetCurrentItem() && inv.GetCurrentItem()->type == 1); // Wraps to the first.
        inv.medkit->count = 0; // Zero charges: absent, as the original Update removes it.
        CHECK(!inv.GetCurrentItem() && inv.CarriedItems().empty());
        inv.medkit->count = 100;
        CHECK(inv.AddItem(5, 1) == InventoryResult::Ok);
        // Typed save keeps the current type; Load selects the first item of it.
        inv.SelectItem(5);
        InventoryState state;
        CHECK(inv.Save(state, error) && state.current_item == 5);
        // Client commands: one queued press each, through the original guards.
        g.client_shell.next_item_requested = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell) && inv.GetCurrentItem()->type == 1);
        CHECK(p->UpdateInventoryClient() && g.pump_player_messages(error));
        const auto& client = g.client_shell;
        CHECK(client.inventory_items.current && client.inventory_items.current->type == 1 &&
              client.inventory_show_requests == 1 && !client.inventory_items.show);
        const auto messages = client.inventory_item_messages;
        CHECK(p->UpdateInventoryClient() && g.pump_player_messages(error) &&
              client.inventory_item_messages == messages);
        g.client_shell.prev_item_requested = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell) && inv.GetCurrentItem()->type == 5);
        CHECK(!g.client_shell.prev_item_requested);
        // USEINVENTORY activates the current item (binoculars toggle zoom on).
        g.client_shell.use_item_requested = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell) && inv.binoculars->active);
        CHECK(p->OnCommandUseInventory() && !inv.binoculars->active);
        p->movement_blocked = true;
        CHECK(!p->OnCommandItemCursor(true) && !p->OnCommandUseInventory() && inv.GetCurrentItem()->type == 5);
        p->movement_blocked = false;
        p->m_damage.m_bDead = DTRUE;
        CHECK(!p->OnCommandItemCursor(true));
        p->m_damage.m_bDead = DFALSE;
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        CNewStatusBar bar(g.resources);
        if (!bar.Init())
            std::cerr << bar.canvas.error << '\n';
        CHECK(bar.Init());
        PlayerStats sample;
        sample.weapon = WEAP_BERETTA;
        sample.weapon_slot = 1;
        sample.health = 100;
        sample.ammo = 50;
        sample.ammo_type = sample.alternate_ammo_type = AMMO_BULLET;
        CHECK(bar.Update(sample, 640, 480) && bar.high_graphics());
        auto tabs = bar.tabs();
        CHECK(tabs[0]->locX == 0 && tabs[0]->locY == 425 && tabs[1]->locX == 185);
        CHECK(tabs[2]->locX == 505 && tabs[2]->locY == 405 && tabs[3]->locX == 542 && tabs[3]->locY == 382);
        const auto before = tabs[0]->hTab->rgba;
        const auto rev = bar.revision;
        CHECK(bar.Update(sample, 640, 480) && bar.revision == rev);
        sample.max_health = 200;
        CHECK(bar.Update(sample, 640, 480) && tabs[0]->hTab->rgba != before);
        CHECK(bar.Update(sample, 511, 384) && !bar.high_graphics() && tabs[1]->locX == 92);
        CHECK(bar.Update(sample, 512, 383) && !bar.high_graphics());
        CHECK(bar.Update(sample, 512, 384) && bar.high_graphics());
        for (auto weapon : {WEAP_MELEE, WEAP_BERETTA, WEAP_ASSAULTRIFLE, WEAP_FLAREGUN, WEAP_SUBMACHINEGUN})
        {
            sample.weapon = weapon;
            CHECK(bar.Update(sample, 640, 480) && tabs[2]->hIcon);
        }
        sample.weapon = WEAP_ASSAULTRIFLE;
        sample.alternate_ammo_type = AMMO_DIEBUGDIE;
        sample.alternate_ammo = 4;
        CHECK(bar.Update(sample, 640, 480) && tabs[3]->value == 4);
        sample.alternate_ammo = 0;
        CHECK(bar.Update(sample, 640, 480) && tabs[3]->value == 0);
        sample.health = -20;
        sample.armor = 70000;
        sample.ammo = 1234;
        sample.alternate_ammo = 1000;
        CHECK(bar.Update(sample, 640, 480));
        CHECK(tabs[0]->value == 0 && tabs[1]->value == 65535 && tabs[2]->value == 999 && tabs[3]->value == 999);
        sample.max_health = 0;
        sample.max_armor = 0;
        CHECK(bar.Update(sample, 640, 480));
        sample.weapon = WEAP_PROXIMITYBOMB;
        sample.weapon_slot = 10;
        CHECK(bar.Update(sample, 640, 480) && !tabs[2]->hIcon);
        // Original UpdateInv/DrawInvTabs: real item art, 0.15 s slide, 5 s display.
        CHECK(bar.item_error.empty());
        InventoryItems shown;
        shown.current = InventoryItemView{1, "medkit.pcx", "medkit_h.pcx", "Medkit", 302, 100};
        shown.next = InventoryItemView{5, "binoculars.pcx", "binoculars_h.pcx", "", 304, 1};
        bar.UpdateInv(shown, true, 10);
        auto items = bar.item_tabs();
        CHECK(!items[0]->hIcon && items[1]->hIcon && items[2]->hIcon && items[3]->hIcon && items[1]->value == 100);
        CHECK(bar.item_error.empty() && bar.canvas.error.empty());
        auto layout = bar.DrawInvTabs(10);
        CHECK(!layout.visible[0] && layout.visible[1] && layout.visible[2] && layout.x[1] == -75 && layout.y[1] == 215);
        CHECK(layout.current_item && layout.name && layout.y[2] == 265);
        layout = bar.DrawInvTabs(10.075f);
        CHECK(layout.x[1] > -40 && layout.x[1] < -36);
        layout = bar.DrawInvTabs(10.2f);
        CHECK(layout.x[1] == 0 && layout.x[2] == -15);
        bar.UpdateInv(shown, false, 14); // Any message while stopped restarts the 5 s display.
        CHECK(bar.DrawInvTabs(18.5f).x[1] == 0);
        layout = bar.DrawInvTabs(19.1f); // Display over: scroll out from here.
        CHECK(layout.visible[1] && layout.x[1] == 0);
        layout = bar.DrawInvTabs(19.175f);
        CHECK(layout.x[1] > -40 && layout.x[1] < -36);
        bar.UpdateInv(shown, false, 19.2f); // A message while scrolling out scrolls back in.
        CHECK(bar.DrawInvTabs(19.5f).x[1] == 0);
        CHECK(bar.DrawInvTabs(24.6f).visible[1] && bar.DrawInvTabs(24.8f).visible[1] &&
              !bar.DrawInvTabs(25).visible[1]);
        CHECK(bar.DrawInvTabs(25).current_item && !bar.DrawInvTabs(25).name);
        bool name_pixels = false;
        for (std::size_t i = 0; i + 3 < bar.item_name()->rgba.size(); i += 4)
            name_pixels |= !(bar.item_name()->rgba[i] == 255 && !bar.item_name()->rgba[i + 1] &&
                             bar.item_name()->rgba[i + 2] == 255);
        CHECK(name_pixels);
        bar.UpdateInv(shown, false, 30); // No show request: stays hidden.
        CHECK(!bar.DrawInvTabs(30).visible[1]);
        CHECK(bar.Update(sample, 511, 384)); // Low detail: no tabs, the name beside the current item.
        bar.UpdateInv(shown, true, 31);
        layout = bar.DrawInvTabs(31);
        CHECK(!layout.visible[1] && layout.current_item && layout.name && layout.name_x == 80);
        CHECK(bar.Update(sample, 640, 480));
        // A missing picture leaves its tab empty without failing the status bar.
        shown.next->icon = "missing_item.pcx";
        bar.UpdateInv(shown, true, 40);
        CHECK(!bar.item_error.empty() && !items[2]->hIcon && bar.canvas.error.empty());
        CHECK(bar.Update(sample, 640, 480));
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error) && g.step(.016));
            auto* p = g.server_shell.Player(g.local_client);
            auto& inv = p->m_InventoryMgr;
            CHECK(g.client_shell.stats.weapon == WEAP_BERETTA && g.client_shell.stats.ammo == 50);
            CHECK(g.client_shell.view.object && !hud_state(g.client_shell.stats, true, true).visible);
            // Diagnostic grant/switch during the intro, to measure both real M16 pools.
            CHECK(inv.ObtainWeapon(WEAP_ASSAULTRIFLE) == InventoryResult::Ok);
            int slot = -1;
            for (unsigned i = 0; i < 10; ++i)
                if (inv.GetWeapon(i) && inv.GetWeapon(i)->data.m_nType == WEAP_ASSAULTRIFLE)
                    slot = int(i);
            CHECK(slot >= 0 && g.select_weapon(DBYTE(slot), error));
            inv.SetAmmoCount(AMMO_BULLET, 31);
            inv.SetAmmoCount(AMMO_DIEBUGDIE, 4);
            CHECK(p->UpdateStats(false) && g.pump_player_messages(error));
            auto h = hud_state(g.client_shell.stats, true, false);
            CHECK(h.stats.ammo_type == AMMO_BULLET && h.stats.alternate_ammo_type == AMMO_DIEBUGDIE &&
                  h.stats.ammo == 31 && h.stats.alternate_ammo == 4 && h.stats.weapon_slot == slot);
            CHECK(g.select_weapon(0, error) && p->UpdateStats(false) && g.pump_player_messages(error));
            CHECK(hud_state(g.client_shell.stats, true, false).stats.ammo_type == AMMO_NONE);
            CHECK(g.unload_world() && !g.client_shell.m_bInWorld && !g.server.string_count());
        }
        return true;
    }
    bool gpu(const std::filesystem::path& root, const std::filesystem::path& content, bool ctf = false)
    {
        Runtime runtime;
        std::string error;
        Resources artwork;
        Config config;
        config.data_dir = content;
        CHECK(artwork.mount_game(config, error));
        HudCanvas canvas(artwork);
        const auto* crosshair = canvas.CreateSurfaceFromBitmap(original_crosshair_bitmap);
        CHECK(crosshair && crosshair->width == 15 && crosshair->height == 13);
        CHECK(runtime.initialize(true, error));
        std::cout << "GPU device: "
                  << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                           "unreported")
                  << '\n';
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            UiRuntime ui(runtime);
            CHECK(ui.initialize(error));
            HudRenderer renderer(runtime.gpu);
            GameSession g;
            CHECK(test_player::setup(g, root) && g.enter_player(error));
            auto* p = g.server_shell.Player(g.local_client);
            const int width = cycle == 0 ? 1200 : cycle == 1 ? 601 : 320, height = cycle == 0 ? 780 : 401;
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), width,
                                     height));
            std::vector<std::uint8_t> first;
            for (int frame = 0; frame < 8; ++frame)
            {
                if (frame == 1)
                {
                    p->m_damage.SetHitPoints(12);
                    p->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 0);
                }
                if (frame == 2)
                    CHECK(g.command_weapon(0, error));
                if (frame == 6)
                    CHECK(g.leave_player());
                if (frame < 6)
                    CHECK(p->UpdateStats(true) && g.pump_player_messages(error));
                ImGui_ImplSDLGPU3_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                ImGui::GetIO().DisplaySize = {float(width), float(height)};
                ImGui::GetIO().DisplayFramebufferScale = {1, 1};
                ImGui::NewFrame();
                auto display = frame == 7
                                   ? hud_state({}, true, false)
                                   : hud_state(g.client_shell.stats, g.client_shell.m_bInWorld, frame == 4, frame == 3);
                if (ctf && frame < 6)
                {
                    display.ctf_team = frame == 1 ? 2 : 1;
                    display.ctf_flag = frame < 2 ? display.ctf_team : 0;
                }
                CHECK(renderer.draw(display, artwork, error));
                ImGui::Render();
                CHECK(!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse);
                auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
                CHECK(commands);
                ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
                SDL_GPUColorTargetInfo color{};
                color.texture = capture.texture;
                color.clear_color = frame == 5 ? SDL_FColor{.1f, .2f, .3f, 1} : SDL_FColor{0, 0, 0, 1};
                color.load_op = SDL_GPU_LOADOP_CLEAR;
                color.store_op = SDL_GPU_STOREOP_STORE;
                auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
                CHECK(pass);
                ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
                SDL_EndGPURenderPass(pass);
                auto* copy = SDL_BeginGPUCopyPass(commands);
                CHECK(copy);
                SDL_GPUTextureRegion region{};
                region.texture = capture.texture;
                region.w = width;
                region.h = height;
                region.d = 1;
                SDL_GPUTextureTransferInfo target{};
                target.transfer_buffer = capture.transfer;
                target.pixels_per_row = width;
                target.rows_per_layer = height;
                SDL_DownloadFromGPUTexture(copy, &region, &target);
                SDL_EndGPUCopyPass(copy);
                CHECK(SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(runtime.gpu));
                auto* data =
                    static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
                CHECK(data);
                std::vector<std::uint8_t> pixels(data, data + width * height * 4);
                SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
                unsigned lit = 0, upper = 0;
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                    {
                        const auto i = (y * width + x) * 4;
                        if (pixels[i] != pixels[0] || pixels[i + 1] != pixels[1] || pixels[i + 2] != pixels[2])
                        {
                            ++lit;
                            if (y < height - 150)
                                ++upper;
                        }
                    }
                CHECK(display.crosshair_visible || (display.visible && display.ctf_team) ? upper > 0 : upper == 0);
                CHECK(frame == 4 || frame == 6 ? lit == 0 : frame == 7 ? lit > 0 && lit < 1000 : lit > 1000);
                const auto rect = original_crosshair_rect({0, 0, width, height}, crosshair->width, crosshair->height);
                const bool bgra = SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window) ==
                                  SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
                for (unsigned y = 0; y < crosshair->height; ++y)
                    for (unsigned x = 0; x < crosshair->width; ++x)
                    {
                        const auto src = (y * crosshair->width + x) * 4;
                        const auto dst = ((rect.top + y) * width + rect.left + x) * 4;
                        const bool black =
                            !crosshair->rgba[src] && !crosshair->rgba[src + 1] && !crosshair->rgba[src + 2];
                        for (unsigned c = 0; c < 3; ++c)
                        {
                            const int expected = !display.crosshair_visible || black
                                                     ? pixels[c]
                                                     : crosshair->rgba[src + (bgra ? 2 - c : c)];
                            CHECK(std::abs(int(pixels[dst + c]) - expected) <= 1);
                        }
                    }
                if (ctf && display.visible && display.ctf_team)
                {
                    const auto* bar = renderer.status_bar();
                    CHECK(bar && bar->team_icon);
                    for (auto* icon : {bar->team_icon, bar->flag_icon})
                    {
                        if (!icon)
                            continue;
                        const unsigned left = width - icon->width - 5;
                        const unsigned top = icon == bar->team_icon ? 5 : bar->team_icon->height + 10;
                        unsigned opaque = 0;
                        for (unsigned y = 0; y < icon->height; ++y)
                            for (unsigned x = 0; x < icon->width; ++x)
                            {
                                const auto src = (y * icon->width + x) * 4, dst = ((top + y) * width + left + x) * 4;
                                const bool transparent =
                                    icon->rgba[src] == 255 && icon->rgba[src + 1] == 0 && icon->rgba[src + 2] == 255;
                                opaque += !transparent;
                                for (unsigned c = 0; c < 3; ++c)
                                {
                                    const int expected = transparent ? pixels[c] : icon->rgba[src + (bgra ? 2 - c : c)];
                                    CHECK(std::abs(int(pixels[dst + c]) - expected) <= 1);
                                }
                            }
                        CHECK(opaque > 0);
                    }
                }
                if (frame == 0)
                    first = pixels;
                if (frame == 1)
                    CHECK(first != pixels);
                CHECK(capture.save(
                    path_text(root / ("hud-" + std::to_string(width) + "-" + std::to_string(frame) + ".png")).c_str()));
                std::cout << "cycle " << cycle << " frame " << frame << " HUD pixels " << lit << "\n";
            }
            CHECK(g.unload_world());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4)
        return 2;
    const std::string mode = argv[1];
    if (mode == "bitmaps")
        return bitmaps() ? 0 : 1;
    if (mode == "values")
        return values() ? 0 : 1;
    if (mode == "input")
        return input() ? 0 : 1;
    if (mode == "session")
        return session(argv[2]) ? 0 : 1;
    if (mode == "inventory")
        return inventory(argv[2]) ? 0 : 1;
    if (mode == "private")
        return private_data(argv[2]) ? 0 : 1;
    if (mode == "gpu_ctf")
        return argc == 4 && gpu(argv[2], argv[3], true) ? 0 : 1;
    if (mode == "gpu")
        return argc == 4 && gpu(argv[2], argv[3]) ? 0 : 1;
    return 2;
}
