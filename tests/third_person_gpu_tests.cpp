#include "app/control_actions.h"
#include "app/hud.h"
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "app/ui_runtime.h"
#include "engine/capture.h"
#include "engine/world_shaders.h"
#include "game/original/ClientCrosshair.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << " " << SDL_GetError() << '\n';                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    bool frame(SDL_GPUDevice* gpu, ScenePresenter& presenter, GameSession& game, Capture& capture, HudRenderer& hud,
               std::vector<Uint8>& pixels, std::string& error)
    {
        if (!presenter.update(game, error) || !presenter.render_world)
            return false;
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::GetIO().DisplaySize = {float(capture.width), float(capture.height)};
        ImGui::GetIO().DisplayFramebufferScale = {1, 1};
        ImGui::NewFrame();
        const auto& c = game.client_shell;
        if (!hud.draw(hud_state(c.stats, c.m_bInWorld, c.view.object != nullptr, c.death_camera.active,
                                c.m_bShowCrosshair, c.m_nCrosshair),
                      game.resources, error))
            return false;
        ImGui::Render();
        auto* commands = SDL_AcquireGPUCommandBuffer(gpu);
        if (!commands || !presenter.record(game, commands, capture.texture, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                           capture.width, capture.height, error))
            return false;
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
        SDL_GPUColorTargetInfo color{};
        color.texture = capture.texture;
        color.load_op = SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;
        auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
        if (!pass)
            return false;
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
        SDL_EndGPURenderPass(pass);
        auto* copy = SDL_BeginGPUCopyPass(commands);
        if (!copy)
            return false;
        SDL_GPUTextureRegion source{};
        source.texture = capture.texture;
        source.w = capture.width;
        source.h = capture.height;
        source.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = capture.transfer;
        destination.pixels_per_row = capture.width;
        destination.rows_per_layer = capture.height;
        SDL_DownloadFromGPUTexture(copy, &source, &destination);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(gpu))
            return false;
        auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu, capture.transfer, false));
        if (!bytes)
            return false;
        pixels.assign(bytes, bytes + std::size_t(capture.width) * capture.height * 4);
        SDL_UnmapGPUTransferBuffer(gpu, capture.transfer);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string error;
    if (argc != 3)
        return 2;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    auto* gpu = runtime.gpu;
    std::cout << "GPU=" << SDL_GetGPUDeviceDriver(gpu) << '\n';
    {
        UiRuntime ui(runtime);
        CHECK(ui.initialize(error, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM));
        HudRenderer hud(gpu);
        Config config;
        config.data_dir = argv[1];
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize() && game->resources.mount_game(config, error) &&
              game->request_world("02_CityHub", error) && game->run_world(error));
        for (int i = 0; i < 60; ++i)
            CHECK(step_app_session(*game, 1.0 / 60));
        Capture capture;
        CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 960, 540));
        ScenePresenter presenter(gpu);
        presenter.model_shadows = false;
        const std::filesystem::path root = argv[2];
        std::filesystem::create_directories(root);
        auto& c = game->client_shell;
        MovementControls controls;
        controls.focused = controls.enabled = true;
        const auto toggle = [&]
        {
            SDL_Event key{};
            key.type = SDL_EVENT_KEY_DOWN;
            key.key.windowID = 1;
            key.key.scancode = SDL_SCANCODE_C;
            controls.event(key, 1);
            apply_control_actions(controls, c, false);
            key.type = SDL_EVENT_KEY_UP;
            controls.event(key, 1);
        };
        std::vector<Uint8> first, third, restored;
        CHECK(frame(gpu, presenter, *game, capture, hud, first, error));
        toggle();
        CHECK(c.third_person && c.UpdateCameraSelection() && c.third_person_active);
        CHECK(frame(gpu, presenter, *game, capture, hud, third, error));
        CHECK(capture.save((root / "third-person.png").c_str()) && first != third);
        bool body = false, held = false;
        for (const auto& entry : presenter.model_scene.entries)
        {
            if (entry.close)
                CHECK(!entry.visible);
            const auto* r = game->objects.get(entry.object);
            if (!r || !entry.visible || !entry.triangles)
                continue;
            body |= r->handle == c.player;
            if (r->attachment)
            {
                const auto relation = game->objects.attachments().find(r->attachment);
                held |= relation != game->objects.attachments().end() && relation->second.parent == c.player;
            }
        }
        CHECK(body && held && c.m_bShowCrosshair &&
              original_crosshair_visible(c.m_bInWorld, c.view.object != nullptr, c.death_camera.active));
        // No simulation step between captures: toggling back must recover exactly the original frame.
        toggle();
        CHECK(!c.third_person && c.UpdateCameraSelection());
        CHECK(frame(gpu, presenter, *game, capture, hud, restored, error) && first == restored);
        CHECK(capture.save((root / "first-person.png").c_str()));
        CHECK(game->unload_world());
    }
    return 0;
}
