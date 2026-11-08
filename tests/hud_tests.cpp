#include "app/control_bindings.h"
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
    // Original CMessageMgr ring, the objectives toggle/scale machine and power-bar requests without
    // art, the Tab action, and the shell's air/throw requests and pickup reply on a fixture player.
    bool panels(const std::filesystem::path& root)
    {
        CMessageMgr m;
        CHECK(!m.GetState() && m.Draw(0).empty() && m.GetMaxMessages() == 4 && m.GetMessageTime() == 5);
        m.Enable(true);
        m.SetMaxMessages(0);
        CHECK(m.GetMaxMessages() == 1);
        m.SetMaxMessages(50);
        CHECK(m.GetMaxMessages() == CMessageMgr::kMaxMessages);
        m.SetMaxMessages(6); // The shell's OnEngineInitialized value.
        for (int i = 0; i < 8; ++i)
            m.AddLine("line " + std::to_string(i), float(i));
        auto lines = m.lines();
        CHECK(lines.size() == 6 && lines.front() == "line 2" && lines.back() == "line 7" && m.added == 8);
        int y = -1;
        auto drawn = m.Draw(6.5f, &y);
        CHECK(drawn.size() == 6 && drawn[0].x == 1 && drawn[0].y == 0 && drawn[5].y == 80 && y == 96);
        drawn = m.Draw(7, &y); // "line 2" expires at its 2 + 5 s, inclusive.
        CHECK(drawn.size() == 5 && drawn[0].text == "line 3" && drawn[0].y == 0 && y == 80 && m.expired == 1);
        CHECK(m.Draw(100, &y).empty() && y == 0 && !m.count());
        for (int i = 0; i < 25; ++i) // The ring wraps past kMaxMessages.
            m.AddLine(std::to_string(i), 200);
        CHECK(m.lines().size() == 6 && m.lines().back() == "24");
        m.Enable(false);
        CHECK(!m.count() && m.Draw(0).empty());

        Resources missing;
        CNewStatusBar bar(missing);
        CHECK(bar.ToggleObjectives(0) && bar.objectives_level() == OBJTAB_SCALE_UP);
        CHECK(!bar.ToggleObjectives(0.05f)); // Ignored while scaling.
        CHECK(!bar.DrawObjectives(0.05f).page && bar.objectives_level() == OBJTAB_SCALE_UP);
        CHECK(!bar.DrawObjectives(0.2f).page && bar.objectives_level() == OBJTAB_STOPPED);
        CHECK(bar.ToggleObjectives(1) && bar.objectives_level() == OBJTAB_SCALE_DOWN);
        bar.DrawObjectives(1.2f);
        CHECK(bar.objectives_level() == OBJTAB_INVISIBLE);
        bar.UpdateObjectives(true, "Title", "Text"); // No page art: nothing to compose or flag.
        bar.SetPowerBarLevel(BARTAB_AIR, .5f, 1);
        CHECK(!bar.DrawObjectives(2).icon && !bar.DrawPowerBar(2).visible && bar.power_bar_type() == BARTAB_NONE);

        const auto bindings = InputBindings::defaults();
        CHECK(info(InputAction::Objectives).name == "objectives" &&
              info(InputAction::Objectives).kind == InputActionKind::Interface && !rebindable(InputAction::Objectives));
        CHECK(bindings.matches({InputSource::Key, SDL_SCANCODE_TAB, true, false}, InputAction::Objectives));

        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto& client = g.client_shell;
        CHECK(p->UpdateStats(true) && g.pump_player_messages(error) && client.stats.air == 1 && !client.air_updates);
        // CBloodClientShell::SetPowerBarLevel: <= 0 or >= 1 turns the bar off.
        client.SetPowerBarLevel(BARTAB_THROW, .5f);
        CHECK(client.power_bar.bar == BARTAB_THROW && client.power_bar.percent == .5f);
        for (float off : {0.f, 1.f, -1.f, std::numeric_limits<float>::quiet_NaN()})
        {
            client.SetPowerBarLevel(BARTAB_THROW, off);
            CHECK(client.power_bar.bar == BARTAB_NONE);
        }
        const auto requests = client.power_bar.requests;
        p->m_fAirLevel = 25; // PLAYERSTATS_AIRLEVEL is m_fAirLevel / MAX_AIR_LEVEL.
        CHECK(p->UpdateStats(false) && g.pump_player_messages(error));
        CHECK(client.air_updates == 1 && client.power_bar.bar == BARTAB_AIR && client.power_bar.percent == .25f &&
              client.power_bar.requests == requests + 1);
        CHECK(p->UpdateStats(false) && g.pump_player_messages(error) && client.air_updates == 1);
        p->m_fAirLevel = 100;
        CHECK(p->UpdateStats(false) && g.pump_player_messages(error));
        CHECK(client.air_updates == 2 && client.power_bar.bar == BARTAB_NONE);
        HudState h;
        hud_panels(h, client);
        CHECK(h.power_requests == client.power_bar.requests && h.power_bar == BARTAB_NONE);
        // CSPrint2 of a typed console line, then the original pickup reply. The fixture has no
        // cres.dll: no line and an explicit error, without a session failure.
        CHECK(g.server.SendToClient(g.local_client, PlayerText{"Hello"}) == DE_OK && g.pump_player_messages(error));
        CHECK(client.m_MessageMgr.lines().back() == "Hello" && client.message_revision == 1);
        const auto failures = g.objects.failure_count();
        auto pickup =
            g.objects.create(g.objects.find_class("BerettaPU"), {}, Creation::World, {{"Pos", DVector{300, 100, 300}}});
        CHECK(pickup && g.objects.engine_message(pickup, MID_TOUCHNOTIFY, client.player) &&
              g.pump_player_messages(error));
        CHECK(p->pickup_messages == 0 && p->pickup_message_error == "Missing pickup message string 260");
        CHECK(p->m_hLastPickupObject == pickup && p->m_nLastPickupResult == CPlayerObj::CHWEAP_OK &&
              g.objects.failure_count() == failures && client.m_MessageMgr.lines().size() == 1);
        CHECK(g.leave_player() && g.unload_world());
        return true;
    }
    // Original objectives page, updated icon and power bar on the retail art, then the original
    // pickup, objective and air values through 01_SubwayA and the typed client messages.
    bool private_panels(GameSession& g, CNewStatusBar& bar)
    {
        CHECK(bar.objectives_error.empty() && bar.power_error.empty());
        const auto* page = bar.objectives_page();
        const auto* icon = bar.objectives_icon();
        const auto* power = bar.power_bar();
        CHECK(page && page->width == 250 && page->height == 150 && icon && icon->width == 38 && icon->height == 66);
        CHECK(power && power->width == 150 && power->height == 16);
        PlayerStats sample;
        sample.weapon = WEAP_BERETTA;
        sample.weapon_slot = 1;
        sample.health = 100;
        sample.ammo = 50;
        sample.ammo_type = sample.alternate_ammo_type = AMMO_BULLET;
        CHECK(bar.Update(sample, 640, 480));
        bar.Reset();
        const auto empty = page->rgba; // "No Current Objective"
        auto revision = bar.revision;
        bar.UpdateObjectives(true, "Title", "An objective long enough to wrap over the page's 200 pixel width.");
        CHECK(page->rgba != empty && bar.revision == revision + 1);
        auto o = bar.DrawObjectives(100);
        CHECK(!o.page && o.icon && o.icon_x == 640 - 38 - 5 && o.icon_y == 5);
        CHECK(bar.ToggleObjectives(100) && bar.objectives_level() == OBJTAB_SCALE_UP && !bar.ToggleObjectives(100));
        o = bar.DrawObjectives(100.05f); // Index 5: x 1.1, y .25 of the half sizes around the centre.
        CHECK(o.page && !o.icon && o.rect.left == 87 && o.rect.right == 362 && o.rect.top == 131 &&
              o.rect.bottom == 168);
        o = bar.DrawObjectives(100.2f);
        CHECK(o.page && bar.objectives_level() == OBJTAB_STOPPED && o.rect.left == 100 && o.rect.top == 75 &&
              o.rect.right == 350 && o.rect.bottom == 225);
        CHECK(bar.DrawObjectives(160).page); // No display timeout.
        bar.UpdateObjectives(true, "Title", "Changed while shown: no icon.");
        CHECK(!bar.DrawObjectives(160).icon);
        CHECK(bar.ToggleObjectives(161) && bar.DrawObjectives(161.05f).page);
        CHECK(!bar.DrawObjectives(161.2f).page && bar.objectives_level() == OBJTAB_INVISIBLE);
        bar.UpdateObjectives(true, "Title", "Text");
        CHECK(bar.DrawObjectives(162).icon);
        bar.Reset(); // OnEnterWorld: the icon hides, the page keeps its text.
        CHECK(!bar.DrawObjectives(162).icon && page->rgba != empty);
        bar.UpdateObjectives(false, {}, {});
        CHECK(page->rgba == empty && !bar.DrawObjectives(162).icon);

        const auto pixels = [&](std::uint8_t r, std::uint8_t g, std::uint8_t b)
        {
            unsigned n = 0;
            for (std::size_t i = 0; i + 3 < power->rgba.size(); i += 4)
                n += power->rgba[i] == r && power->rgba[i + 1] == g && power->rgba[i + 2] == b;
            return n;
        };
        revision = bar.revision;
        bar.SetPowerBarLevel(BARTAB_AIR, .5f, 200);
        CHECK(bar.revision == revision + 1 && bar.power_bar_type() == BARTAB_AIR && pixels(0, 0, 255) > 0);
        bar.SetPowerBarLevel(BARTAB_AIR, .5f, 201); // Same pixels: no recomposition.
        CHECK(bar.revision == revision + 1);
        bar.SetPowerBarLevel(BARTAB_THROW, .5f, 202);
        CHECK(pixels(255, 255, 0) > 0 && !pixels(0, 0, 255));
        const auto half = pixels(255, 255, 0);
        bar.SetPowerBarLevel(BARTAB_THROW, .9f, 202);
        CHECK(pixels(255, 255, 0) > half);
        bar.SetPowerBarLevel(BARTAB_BOSS, .25f, 203);
        CHECK(pixels(255, 0, 0) > 0 && !pixels(255, 255, 0));
        auto pb = bar.DrawPowerBar(203);
        CHECK(pb.visible && pb.x == 245 && pb.y == int(bar.tabs()[2]->locY) - 32);
        CHECK(bar.DrawPowerBar(208.1f).visible && bar.power_bar_type() == BARTAB_NONE &&
              !bar.DrawPowerBar(208.2f).visible);
        std::cout << "Objectives page 250x150, skull 38x66, power bar 150x16 at (" << pb.x << ',' << pb.y << ")\n";

        std::string error;
        CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto& client = g.client_shell;
        std::string format;
        CHECK(g.server.FindClientResourceString(260, format));
        std::cout << "IDS_GENERAL_PICKUP_1: \"" << format << "\"\n";
        const auto create = [&](const char* type, Properties props)
        {
            props.try_emplace("Name", std::string("hud_pickup"));
            props.try_emplace("Pos", DVector{200, 10000, 200});
            return g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
        };
        // The carried Beretta accepts the pickup's ammunition (CHWEAP_OK).
        auto pickup = create("BerettaPU", {});
        CHECK(pickup && g.objects.engine_message(pickup, MID_TOUCHNOTIFY, client.player) &&
              g.pump_player_messages(error));
        auto lines = client.m_MessageMgr.lines();
        // IDS_WEAPON_BERETTA (350) carries a trailing space in the retail cres.dll; kept as is.
        CHECK(p->pickup_messages == 1 && p->pickup_message_error.empty() && !lines.empty() &&
              lines.back().starts_with("Picked Up Beretta"));
        std::cout << "Pickup line: \"" << lines.back() << "\"\n";
        // A key without ResourceNum shows its object name; a second copy is CHWEAP_ALREADYHAVE.
        const Properties key{{"ObjectName", std::string("RedKey")},
                             {"ModelFile", std::string("models/characters/caleb.abc")}};
        pickup = create("KeyPickup", key);
        CHECK(pickup && g.objects.engine_message(pickup, MID_TOUCHNOTIFY, client.player) &&
              g.pump_player_messages(error));
        CHECK(p->pickup_messages == 2 && client.m_MessageMgr.lines().back() == "Picked Up RedKey");
        pickup = create("KeyPickup", key);
        CHECK(pickup && g.objects.engine_message(pickup, MID_TOUCHNOTIFY, client.player) &&
              g.pump_player_messages(error));
        CHECK(p->pickup_messages == 2 && p->m_nLastPickupResult == CPlayerObj::CHWEAP_ALREADYHAVE);
        const auto drawn = client.m_MessageMgr.Draw(float(g.objects.time()));
        CHECK(drawn.size() >= 2 && drawn.back().text == "Picked Up RedKey" && drawn[0].y == 0 && drawn[1].y == 16);
        const auto& weapon_source = drawn[drawn.size() - 2].source;
        const auto& key_source = drawn.back().source;
        CHECK(weapon_source.resource == 260 && weapon_source.argument_resource == 350 &&
              weapon_source.argument.starts_with("Beretta"));
        CHECK(key_source.resource == 260 && key_source.argument_resource == 0 && key_source.argument == "RedKey");
        // SMSG_OBJECTIVES with resource 1 (strings 5000/6000) and the air level.
        const auto updates = client.objective_updates, sounds = client.interface_sounds;
        CHECK(g.server.SendToClient(g.local_client, MissionObjectives{1, {}, {}}) == DE_OK &&
              g.pump_player_messages(error));
        CHECK(client.objective_updates == updates + 1 && client.objectives_resolved &&
              !client.objectives.title.empty() && client.interface_sounds == sounds + 1 &&
              client.interface_sound_error.empty());
        HudState h;
        hud_panels(h, client);
        CHECK(h.objectives_valid && h.objectives_resource == 1 && h.objectives_title == client.objectives.title &&
              h.objective_updates == client.objective_updates);
        bar.UpdateObjectives(true, h.objectives_title, h.objectives_text);
        CHECK(page->rgba != empty);
        std::cout << "Objective 1: \"" << client.objectives.title << "\"\n";
        p->m_fAirLevel = 40;
        CHECK(p->UpdateStats(false) && g.pump_player_messages(error));
        CHECK(client.power_bar.bar == BARTAB_AIR && std::abs(client.power_bar.percent - .4f) < 1e-6f);
        CHECK(g.unload_world() && !g.server.string_count());
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
        // Original weapon tabs (issue #5, F1): real art, highlighted current slot, 0.25 s slide
        // from the right edge, hidden in low detail and on a level change.
        sample.weapon = WEAP_BERETTA;
        sample.weapon_slot = 1;
        sample.weapon_slots = {WEAP_MELEE, WEAP_BERETTA, WEAP_SHOTGUN};
        CHECK(bar.Update(sample, 640, 480) && bar.weapon_error.empty());
        CHECK(bar.weapon_tab(0).hIcon && bar.weapon_tab(1).hIcon && bar.weapon_tab(2).hIcon &&
              !bar.weapon_tab(3).hIcon && bar.weapon_tab(9).value == 10);
        CHECK(bar.weapon_tab(0).locX == 490 && bar.weapon_tab(0).locY == 115 && bar.weapon_tab(9).locY == 340);
        const auto highlighted = bar.weapon_tab(1).hTab->rgba;
        sample.weapon_slot = 2;
        sample.weapon = WEAP_SHOTGUN;
        CHECK(bar.Update(sample, 640, 480) && bar.weapon_tab(1).hTab->rgba != highlighted);
        sample.weapon_slot = 10; // A reserved inventory slot keeps the previous highlight.
        const auto kept = bar.weapon_tab(2).hTab->rgba;
        CHECK(bar.Update(sample, 640, 480) && bar.weapon_tab(2).hTab->rgba == kept);
        CHECK(!bar.DrawWeaponTabs(50).visible);
        bar.ToggleWeapons(50);
        auto weapons = bar.DrawWeaponTabs(50);
        CHECK(weapons.visible && weapons.x == 640 && weapons.y[0] == 115);
        weapons = bar.DrawWeaponTabs(50.125f);
        CHECK(weapons.x > 560 && weapons.x < 570);
        bar.ToggleWeapons(50.2f); // Ignored while scrolling in.
        CHECK(bar.DrawWeaponTabs(50.3f).x == 490 && bar.weapon_level() == WEAPTAB_STOPPED);
        CHECK(bar.DrawWeaponTabs(90).x == 490); // No display timeout.
        bar.ToggleWeapons(90);
        CHECK(bar.DrawWeaponTabs(90.2f).visible && !bar.DrawWeaponTabs(90.3f).visible);
        bar.ToggleWeapons(91);
        CHECK(bar.Update(sample, 511, 384) && !bar.DrawWeaponTabs(92).visible);
        CHECK(bar.Update(sample, 640, 480) && bar.DrawWeaponTabs(92).visible);
        bar.ResetWeapons();
        CHECK(!bar.DrawWeaponTabs(93).visible);
        sample.weapon_slots = {};
        CHECK(bar.Update(sample, 640, 480) && !bar.weapon_tab(1).hIcon);
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
        return private_panels(g, bar);
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
    // The on-screen touch overlay: held controls are filled, the others outlined, and the
    // floating stick drawn where its finger landed.
    bool gpu_touch(const std::filesystem::path& root)
    {
        Runtime runtime;
        std::string error;
        CHECK(runtime.initialize(true, error));
        UiRuntime ui(runtime);
        CHECK(ui.initialize(error));
        constexpr int width = 1200, height = 675;
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), width,
                                 height));
        TouchControls touch;
        touch.visible = true;
        touch.pressed[0] = true; // FIRE.
        touch.stick_active = true;
        touch.origin_x = .4f;
        touch.origin_y = .6f;
        touch.stick_x = .5f;
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::GetIO().DisplaySize = {float(width), float(height)};
        ImGui::GetIO().DisplayFramebufferScale = {1, 1};
        ImGui::NewFrame();
        draw_touch_controls(touch);
        ImGui::Render();
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands);
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
        SDL_GPUColorTargetInfo color{};
        color.texture = capture.texture;
        color.clear_color = {0, 0, 0, 1};
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
        auto* data = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(data);
        std::vector<std::uint8_t> pixels(data, data + width * height * 4);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        const auto lit = [&](float x, float y)
        {
            const auto i = (std::size_t(y) * width + std::size_t(x)) * 4;
            return pixels[i] + pixels[i + 1] + pixels[i + 2] > 0;
        };
        const float aspect = float(width) / height;
        const auto inside = [&](std::size_t i)
        {
            const auto& c = touch_layout[i];
            // Between the label and the outline, on the horizontal through the center.
            return lit((TouchControls::center_x(c, aspect) - c.radius * .8f) * height, (1 - c.y) * height);
        };
        CHECK(inside(0) && !inside(1)); // Held FIRE is filled; ALT is not.
        CHECK(lit(.4f * height, .6f * height) || lit((.4f + .5f * touch_stick_radius) * height, .6f * height));
        CHECK(!lit(width / 2.f, 5)); // Nothing at the top center.
        std::filesystem::create_directories(root);
        CHECK(capture.save(path_text(root / "touch-controls.png").c_str()));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4)
        return 2;
    const std::string mode = argv[1];
    if (mode == "gpu_touch")
        return gpu_touch(argv[2]) ? 0 : 1;
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
    if (mode == "panels")
        return panels(argv[2]) ? 0 : 1;
    if (mode == "private")
        return private_data(argv[2]) ? 0 : 1;
    if (mode == "gpu_ctf")
        return argc == 4 && gpu(argv[2], argv[3], true) ? 0 : 1;
    if (mode == "gpu")
        return argc == 4 && gpu(argv[2], argv[3]) ? 0 : 1;
    return 2;
}
