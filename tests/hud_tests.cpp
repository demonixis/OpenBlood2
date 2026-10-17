#include "app/hud.h"
#include "app/movement_controls.h"
#include "app/ui_runtime.h"
#include "engine/capture.h"
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
        CHECK(h.visible && h.health == 27 && h.armor == 12 && h.ammo == 123 && h.alternate_ammo == 4);
        CHECK(h.uses_ammo && h.alternate_pool && h.slot == 2 && h.slots[2] == WEAP_ASSAULTRIFLE && !h.weapon.empty());
        CHECK(!hud_state(s, false, false).visible && !hud_state(s, true, true).visible);
        s.ammo = 1234;
        s.alternate_ammo = 1000;
        h = hud_state(s, true, false);
        CHECK(h.ammo == 999 && h.alternate_ammo == 999 && s.ammo == 1234);
        s.health = -20;
        s.armor = 70000;
        s.ammo = 0;
        h = hud_state(s, true, false);
        CHECK(h.visible && h.health == 0 && h.armor == 65535 && h.ammo == 0 && h.uses_ammo);
        s.weapon = WEAP_MELEE;
        s.ammo_type = s.alternate_ammo_type = AMMO_NONE;
        h = hud_state(s, true, false);
        CHECK(h.visible && !h.uses_ammo && !h.alternate_pool);
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
        CHECK(!hud_state(g.client_shell.stats, true, false).uses_ammo && g.client_shell.stats.weapon_slot == 0);
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
        CHECK(h.visible && h.health == 21 && h.armor == 15 && h.ammo == 0 && h.uses_ammo && h.slot == 1);
        auto s = g.client_shell.stats;
        for (int variant = 0; variant < 5; ++variant)
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
            CHECK(h.uses_ammo && h.alternate_pool && h.ammo == 31 && h.alternate_ammo == 4 && h.slot == slot);
            CHECK(g.select_weapon(0, error) && p->UpdateStats(false) && g.pump_player_messages(error));
            CHECK(!hud_state(g.client_shell.stats, true, false).uses_ammo);
            CHECK(g.unload_world() && !g.client_shell.m_bInWorld && !g.server.string_count());
        }
        return true;
    }
    bool gpu(const std::filesystem::path& root)
    {
        Runtime runtime;
        std::string error;
        CHECK(runtime.initialize(true, error));
        std::cout << "GPU device: "
                  << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                           "unreported")
                  << '\n';
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            UiRuntime ui(runtime);
            CHECK(ui.initialize(error));
            GameSession g;
            CHECK(test_player::setup(g, root) && g.enter_player(error));
            auto* p = g.server_shell.Player(g.local_client);
            const int width = cycle == 0 ? 1200 : cycle == 1 ? 600 : 320, height = cycle == 0 ? 780 : 400;
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), width,
                                     height));
            std::vector<std::uint8_t> first;
            for (int frame = 0; frame < 4; ++frame)
            {
                if (frame == 1)
                {
                    p->m_damage.SetHitPoints(12);
                    p->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 0);
                }
                if (frame == 2)
                    CHECK(g.command_weapon(0, error));
                if (frame == 3)
                    CHECK(g.leave_player());
                if (frame < 3)
                    CHECK(p->UpdateStats(true) && g.pump_player_messages(error));
                ImGui_ImplSDLGPU3_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                ImGui::GetIO().DisplaySize = {float(width), float(height)};
                ImGui::GetIO().DisplayFramebufferScale = {1, 1};
                ImGui::NewFrame();
                CHECK(draw_hud(hud_state(g.client_shell.stats, g.client_shell.m_bInWorld, false)) == (frame < 3));
                ImGui::Render();
                CHECK(!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse);
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
                        if (pixels[i] || pixels[i + 1] || pixels[i + 2])
                        {
                            ++lit;
                            if (y < height - 150)
                                ++upper;
                        }
                    }
                CHECK(!upper && (frame == 3 ? lit == 0 : lit > 1000));
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
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    if (mode == "values")
        return values() ? 0 : 1;
    if (mode == "input")
        return input() ? 0 : 1;
    if (mode == "session")
        return session(argv[2]) ? 0 : 1;
    if (mode == "private")
        return private_data(argv[2]) ? 0 : 1;
    if (mode == "gpu")
        return gpu(argv[2]) ? 0 : 1;
    return 2;
}
