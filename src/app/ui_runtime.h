#pragma once
#include "engine/runtime.h"
struct ImFont;
namespace OpenBlood2
{
    struct CinematicCaption;
    class LocalizedFont;
    enum class CaptionPlacement { Play, Letterbox, Movie };

    // One context shared by the always-available HUD and optional inspector.
    struct UiRuntime
    {
        explicit UiRuntime(Runtime& value) : runtime(value) {}
        ~UiRuntime();
        UiRuntime(const UiRuntime&) = delete;
        UiRuntime& operator=(const UiRuntime&) = delete;
        // target_format: the colour format ImGui renders into; INVALID selects the window swapchain's.
        bool initialize(std::string&, SDL_GPUTextureFormat target_format = SDL_GPU_TEXTUREFORMAT_INVALID);
        // The loaded face must outlive this ImGui context.
        bool set_localized_font(const LocalizedFont&);
        void draw_subtitle(CinematicCaption, CaptionPlacement, const LocalizedFont&) const;
        Runtime& runtime;
        bool context = false, platform = false, renderer = false;
        ImFont* proportional = nullptr; // Droid Sans for the setup screen; the default font stays first.
        ImFont* localized = nullptr;
    };
} // namespace OpenBlood2
