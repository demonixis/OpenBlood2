#include "app/movement_controls.h"
#include "engine/hud_surface.h"
#include "game/original/ClientCrosshair.h"
#include "game/original/ItemPickups.h"
#include "player_fixture.h"
#include <cmath>
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
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        std::string e;
        CHECK(test_player::setup(g, root, true, true, true) && g.enter_player(e) && g.pump_player_messages(e));
        return true;
    }
    CPlayerObj& player(GameSession& g)
    {
        return *g.server_shell.Player(g.local_client);
    }
    bool press(GameSession& g)
    {
        g.client_shell.binoculars_down = true;
        const bool ok = g.client_shell.UpdateUse(g.server_shell);
        g.client_shell.binoculars_down = false;
        return ok && g.client_shell.UpdateUse(g.server_shell);
    }
    bool pickup(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        for (unsigned i = 0; i < 2; ++i)
        {
            auto h = g.objects.create(g.objects.find_class("BinocularsPU"), {}, Creation::World,
                                      {{"Pos", DVector{300, 100, 300}}});
            auto* r = g.objects.get(h);
            CHECK(r && r->model && r->model_error.empty());
            auto* item = dynamic_cast<BinocularsPU*>(r->game.get());
            CHECK(item && item->resource_id == 304 && item->value.type == 5 && item->value.amount == 1);
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject));
            // Base CInvItem::AddItem refuses a second pair; the pickup stays in the world.
            CHECK(item->picked_up == (i == 0) && p.m_InventoryMgr.binoculars && !p.m_InventoryMgr.binoculars->active);
        }
        CHECK(p.m_InventoryMgr.AddItem(5, 1) == InventoryResult::AlreadyHave);
        auto spawned = g.objects.create(g.objects.find_class("BinocularsPU"), {}, Creation::String, {});
        auto* bounced = spawned ? dynamic_cast<BinocularsPU*>(g.objects.get(spawned)->game.get()) : nullptr;
        CHECK(bounced && bounced->bouncing);
        CHECK(g.server.ValidateSpawnObject("BinocularsPU Name okay"));
        return true;
    }
    bool zoom(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& c = g.client_shell;
        CHECK(!p.OnCommandItem(5)); // No binoculars yet.
        CHECK(p.m_InventoryMgr.AddItem(5, 1) == InventoryResult::Ok);
        auto& b = *p.m_InventoryMgr.binoculars;
        p.command_run = true; // Original run-key exclusion of COMMAND_INVITEM_1.
        CHECK(!p.OnCommandItem(5) && !b.active);
        p.command_run = false;
        CHECK(c.m_fovX == original_fov_normal && c.ViewFovX() == original_fov_normal);
        CHECK(press(g) && b.active && b.actions == 1 && p.m_bZoomView && p.m_nZoomType == 98 && p.zoom_messages == 1);
        std::string e;
        CHECK(g.pump_player_messages(e) && c.m_bZoomView && c.m_bBinocularsActive && c.m_nCrosshair == 98 &&
              c.m_bShowCrosshair && c.zoom_messages == 1 && c.item_actions == 1);
        CHECK(!c.item_audio_error.empty()); // Fixture has no binoculars.wav; zoom state still applies.
        // 8 rad/s toward FOV_ZOOMED, clamped; mouse look is quartered meanwhile.
        CHECK(c.UpdateZoom(.1) && std::abs(c.m_fovX - (original_fov_normal - .8f)) < 1e-5f);
        CHECK(c.UpdateZoom(.1) && c.m_fovX == original_fov_zoomed && c.UpdateZoom(.1) &&
              c.m_fovX == original_fov_zoomed);
        CHECK(std::abs(original_fov_scale(c.ViewFovX()) - 1 / std::tan(original_fov_zoomed / 2)) < 1e-4f);
        CHECK(original_fov_scale(original_fov_normal) > .9999f && original_fov_scale(original_fov_normal) < 1.0001f);
        CHECK(original_crosshair_path(98) == "interface/crosshairs/crosshair98.pcx" &&
              original_crosshair_path(1) == original_crosshair_bitmap);
        // Toggle off: action, zoom off, original crosshair restored, FOV widens back.
        CHECK(press(g) && !b.active && b.actions == 2 && !p.m_bZoomView && p.zoom_messages == 2);
        CHECK(g.pump_player_messages(e) && !c.m_bZoomView && !c.m_bBinocularsActive && c.m_nCrosshair == 1);
        CHECK(c.UpdateZoom(.1) && c.UpdateZoom(.1) && c.m_fovX == original_fov_normal);
        // Already zoomed by something else: Activate ignores the request.
        p.SetZoomMode(true, 99);
        CHECK(!p.OnCommandItem(5) && !b.active && b.actions == 2);
        CHECK(g.pump_player_messages(e) && c.m_nCrosshair == 99);
        p.SetZoomMode(false);
        CHECK(g.pump_player_messages(e) && c.m_nCrosshair == 1 && !c.m_bZoomView);
        // Repeated "off" requests coalesce instead of filling the bounded queue.
        const auto sent = p.zoom_messages;
        for (unsigned i = 0; i < 300; ++i)
            p.SetZoomMode(false);
        CHECK(p.zoom_messages == sent && p.zoom_messages_coalesced >= 300 && !p.zoom_message_failures);
        return true;
    }
    bool weapons(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& c = g.client_shell;
        auto& inv = p.m_InventoryMgr;
        CHECK(inv.AddItem(5, 1) == InventoryResult::Ok && g.step(.01));
        auto& b = *inv.binoculars;
        // Client-side CViewWeapon::Fire refuses while binoculars are active.
        CHECK(press(g) && b.active && g.step(.01) && c.m_bBinocularsActive);
        const auto ammo = inv.GetAmmoCount(1);
        c.weapon_input.primary = true;
        for (int i = 0; i < 100; ++i)
            CHECK(g.step(.01));
        CHECK(c.weapons[0].binocular_refusals > 0 && inv.GetAmmoCount(1) == ammo);
        c.weapon_input.primary = false;
        for (int i = 0; i < 100; ++i)
            CHECK(g.step(.01));
        // Any in-range slot request ends zoom; an unavailable slot keeps the item flagged.
        CHECK(!inv.GetWeapon(5) && inv.ChangeWeaponSlot(5) == InventoryResult::NotAvailable && !p.m_bZoomView &&
              b.active);
        CHECK(press(g) && !b.active && press(g) && b.active && p.m_bZoomView);
        // A real weapon change: zoom ends at once, Update's gun tracking toggles the item off.
        CHECK(inv.GetWeapon(0) && inv.ChangeWeaponSlot(0) == InventoryResult::Ok && !p.m_bZoomView && b.active);
        const auto actions = b.actions;
        CHECK(g.step(.01) && !b.active && b.actions == actions + 1 && p.m_CurGun == inv.CurrentWeapon()->data.m_nType);
        CHECK(!c.m_bBinocularsActive && !c.m_bZoomView);
        c.weapon_input.primary = true;
        for (int i = 0; i < 100; ++i)
            CHECK(g.step(.01));
        CHECK(c.weapons[0].shots > 0);
        return true;
    }
    bool death(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        CHECK(p.m_InventoryMgr.AddItem(5, 1) == InventoryResult::Ok && press(g) && p.m_bZoomView);
        const auto actions = p.m_InventoryMgr.binoculars->actions;
        p.m_InventoryMgr.RemoveDeathItems();
        CHECK(!p.m_InventoryMgr.binoculars && !p.m_bZoomView && p.zoom_messages == 2);
        std::string e;
        CHECK(g.pump_player_messages(e) && !g.client_shell.m_bZoomView && !g.client_shell.m_bBinocularsActive &&
              g.client_shell.item_actions >= actions + 1);
        return true;
    }
    bool restore(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& inv = player(g).m_InventoryMgr;
        CHECK(inv.AddItem(5, 1) == InventoryResult::Ok && press(g) && player(g).m_bZoomView);
        PlayerRestoreState s;
        std::string e;
        CHECK(g.capture_player(s, e) && s.inventory.binoculars);
        CHECK(g.leave_player() && g.restore_player(s, e));
        auto& restored = player(g);
        // InvItem::Load clears the active flag; player Load clears zoom; the client starts unzoomed.
        CHECK(restored.m_InventoryMgr.binoculars && !restored.m_InventoryMgr.binoculars->active &&
              !restored.m_bZoomView && !g.client_shell.m_bZoomView && g.client_shell.m_fovX == original_fov_normal);
        auto absent = s;
        absent.inventory.binoculars = false;
        CHECK(g.leave_player() && g.restore_player(absent, e) && !player(g).m_InventoryMgr.binoculars);
        return true;
    }
    bool input(const std::filesystem::path&)
    {
        MovementControls c;
        c.focused = true;
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = 7;
        e.key.scancode = SDL_SCANCODE_Z;
        CHECK(c.consumes(e, false));
        c.event(e, 7);
        CHECK(c.sample_binoculars(false));
        CHECK(!c.sample_binoculars(true) && !c.sample_binoculars(false));
        e.type = SDL_EVENT_KEY_UP;
        c.event(e, 7);
        CHECK(!c.sample_binoculars(false));
        e.type = SDL_EVENT_KEY_DOWN;
        c.event(e, 7);
        c.player_changed();
        CHECK(!c.sample_binoculars(false));
        // Zoomed mouse look is a quarter of the normal offset; keys are unchanged.
        c.mouse_captured = true;
        PlayerRotation normal, zoomed;
        c.mouse_x = 100;
        c.look(normal, .01, false);
        c.mouse_x = 100;
        c.look(zoomed, .01, false, true);
        CHECK(std::abs(normal.yaw - 4 * zoomed.yaw) < 1e-6f && normal.yaw > 0);
        return true;
    }
    bool private_game(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string e;
        CHECK(g.resources.mount_game(cfg, e));
        g.server.audio.inspect_pcm = true;
        CHECK(g.request_world("01_subwaya", e) && g.enter_player(e) && g.pump_player_messages(e));
        auto& p = player(g);
        auto h = g.objects.create(g.objects.find_class("BinocularsPU"), {}, Creation::World,
                                  {{"Pos", g.client_shell.position}});
        CHECK(h && g.objects.get(h)->model && g.objects.get(h)->model_error.empty());
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && p.m_InventoryMgr.binoculars);
        // The SubwayA intro camera blocks item commands; activate the inventory item directly.
        CHECK(p.m_InventoryMgr.SetActiveItem(5) == InventoryResult::Ok && g.pump_player_messages(e));
        auto* v = g.server.audio.get(g.client_shell.item_voice);
        CHECK(v && v->clip && v->request.space == SoundSpace::Local && v->request.priority == 4);
        CHECK(g.client_shell.m_nCrosshair == 98);
        HudCanvas canvas(g.resources);
        const auto* cursor = canvas.CreateSurfaceFromBitmap(original_crosshair_path(98).c_str());
        CHECK(cursor && cursor->width == 97 && cursor->height == 41);
        std::cout << "Binoculars frames=" << v->clip->frames() << " cursor=" << cursor->width << 'x' << cursor->height
                  << '\n';
        CHECK(g.leave_player() && g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string test = argv[1];
    const std::filesystem::path root = argv[2];
    if (test == "pickup")
        return pickup(root) ? 0 : 1;
    if (test == "zoom")
        return zoom(root) ? 0 : 1;
    if (test == "weapons")
        return weapons(root) ? 0 : 1;
    if (test == "death")
        return death(root) ? 0 : 1;
    if (test == "restore")
        return restore(root) ? 0 : 1;
    if (test == "input")
        return input(root) ? 0 : 1;
    if (test == "private")
        return private_game(root) ? 0 : 1;
    return 2;
}
