#include "app/hud.h"
#include "app/movement_controls.h"
#include "app/ui_runtime.h"
#include "engine/capture.h"
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
    bool gpu(const std::filesystem::path& root, const std::filesystem::path& content)
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
                const auto display =
                    frame == 7 ? hud_state({}, true, false)
                               : hud_state(g.client_shell.stats, g.client_shell.m_bInWorld, frame == 4, frame == 3);
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
                CHECK(display.crosshair_visible ? upper > 0 : upper == 0);
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
                if (frame == 0)
                    first = pixels;
                if (frame == 1)
                    CHECK(first != pixels);
                CHECK(capture.save(
                    (root / ("hud-" + std::to_string(width) + "-" + std::to_string(frame) + ".png")).c_str()));
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
    if (mode == "private")
        return private_data(argv[2]) ? 0 : 1;
    if (mode == "gpu")
        return argc == 4 && gpu(argv[2], argv[3]) ? 0 : 1;
    return 2;
}
