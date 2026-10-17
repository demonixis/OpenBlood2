#include "app/ui_runtime.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
namespace OpenBlood2
{
    bool UiRuntime::initialize(std::string& error)
    {
        IMGUI_CHECKVERSION();
        if (!ImGui::CreateContext())
        {
            error = "Cannot create Dear ImGui context";
            return false;
        }
        context = true;
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.FontSizeBase = 17.0f;
        style.WindowRounding = 8.0f;
        style.FrameRounding = 4.0f;
        style.WindowPadding = ImVec2(20, 18);
        style.ItemSpacing = ImVec2(10, 9);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.085f, 0.10f, 0.98f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.38f, 0.14f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TabSelected] = ImVec4(0.42f, 0.16f, 0.18f, 1.0f);
        const float scale = SDL_GetWindowDisplayScale(runtime.window);
        if (scale > 0.0f)
            style.FontScaleDpi = scale;
        platform = ImGui_ImplSDL3_InitForSDLGPU(runtime.window);
        if (!platform)
        {
            error = "Cannot initialize Dear ImGui SDL3 platform backend";
            return false;
        }
        ImGui_ImplSDLGPU3_InitInfo info{};
        info.Device = runtime.gpu;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
        renderer = ImGui_ImplSDLGPU3_Init(&info);
        if (!renderer)
            error = "Cannot initialize Dear ImGui SDL_GPU renderer backend";
        return renderer;
    }

    UiRuntime::~UiRuntime()
    {
        if (runtime.gpu)
            SDL_WaitForGPUIdle(runtime.gpu);
        if (renderer)
            ImGui_ImplSDLGPU3_Shutdown();
        if (platform)
            ImGui_ImplSDL3_Shutdown();
        if (context)
            ImGui::DestroyContext();
    }

} // namespace OpenBlood2
