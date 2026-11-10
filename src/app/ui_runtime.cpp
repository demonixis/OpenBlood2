#include "app/ui_runtime.h"
#include "game/cinematic_captions.h"
#include "game/localized_font.h"
#include "ui_font.h"
#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
namespace OpenBlood2
{
    bool UiRuntime::initialize(std::string& error, SDL_GPUTextureFormat target_format)
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
        // The default font stays the HUD/inspector font; the setup screen pushes Droid Sans.
        io.Fonts->AddFontDefault();
        ImFontConfig embedded;
        embedded.FontDataOwnedByAtlas = false;
        proportional =
            io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(ui_font), int(sizeof ui_font), 0.0f, &embedded);
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
        // An offscreen target (the XR UI canvas) has fixed pixels; only the window follows the display scale.
        const float scale = SDL_GetWindowDisplayScale(runtime.window);
        if (scale > 0.0f && target_format == SDL_GPU_TEXTUREFORMAT_INVALID)
            style.FontScaleDpi = scale;
        platform = ImGui_ImplSDL3_InitForSDLGPU(runtime.window);
        if (!platform)
        {
            error = "Cannot initialize Dear ImGui SDL3 platform backend";
            return false;
        }
        ImGui_ImplSDLGPU3_InitInfo info{};
        info.Device = runtime.gpu;
        info.ColorTargetFormat = target_format != SDL_GPU_TEXTUREFORMAT_INVALID
                                     ? target_format
                                     : SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
        renderer = ImGui_ImplSDLGPU3_Init(&info);
        if (!renderer)
            error = "Cannot initialize Dear ImGui SDL_GPU renderer backend";
        return renderer;
    }

    bool UiRuntime::set_localized_font(const LocalizedFont& font)
    {
        if (!context || !font)
            return false;
        if (!localized)
        {
            const auto bytes = font.bytes();
            ImFontConfig config;
            config.FontDataOwnedByAtlas = false;
            localized = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(bytes.data()),
                                                                 int(bytes.size()), 0.0f, &config);
        }
        return localized != nullptr;
    }

    void UiRuntime::draw_subtitle(CinematicCaption caption, CaptionPlacement placement, const LocalizedFont& font) const
    {
        if (caption.english.empty())
            return;
        const bool translated = localized && !caption.translated.empty() && font.supports(caption.translated);
        const auto text = translated ? caption.translated : caption.english;
        auto* face = proportional ? proportional : ImGui::GetFont();
        if (localized)
        {
            auto* cursor = text.data();
            auto remaining = text.size();
            while (remaining)
            {
                const auto codepoint = SDL_StepUTF8(&cursor, &remaining);
                if (codepoint > IM_UNICODE_CODEPOINT_MAX || !face->IsGlyphInFont(ImWchar(codepoint)))
                {
                    if (translated || font.supports(text))
                        face = localized;
                    break;
                }
            }
        }

        const auto& viewport = *ImGui::GetMainViewport();
        const float scale = std::min(viewport.Size.x / 640.f, viewport.Size.y / 480.f);
        if (scale <= 0)
            return;
        const bool letterbox = placement == CaptionPlacement::Letterbox;
        const float wrap = std::min(viewport.Size.x - 16 * scale, 544 * scale);
        const float available = letterbox ? viewport.Size.y / 6 - 8 * scale : 96 * scale;
        const auto end = text.data() + text.size();
        float size = 16 * scale;
        auto bounds = face->CalcTextSizeA(size, FLT_MAX, wrap, text.data(), end);
        while (bounds.y > available && size > 11 * scale)
        {
            size -= scale;
            bounds = face->CalcTextSizeA(size, FLT_MAX, wrap, text.data(), end);
        }
        const float bottom = placement == CaptionPlacement::Movie ? (viewport.Size.y + 480 * scale) / 2
                                                                   : viewport.Size.y;
        const float inset = placement == CaptionPlacement::Movie ? 12 * scale : 74 * scale;
        const float top = bottom - (letterbox ? (viewport.Size.y / 6 + bounds.y) / 2 : inset + bounds.y);
        const ImVec2 pos{viewport.Pos.x + (viewport.Size.x - bounds.x) / 2, viewport.Pos.y + top};
        auto* draw = ImGui::GetForegroundDrawList();
        draw->AddText(face, size, {pos.x + scale, pos.y + scale}, IM_COL32(0, 0, 0, 255), text.data(), end, wrap);
        draw->AddText(face, size, pos, IM_COL32(255, 255, 255, 255), text.data(), end, wrap);
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
