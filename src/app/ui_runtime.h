#pragma once
#include "engine/runtime.h"
namespace OpenBlood2
{
    // One context shared by the always-available HUD and optional inspector.
    struct UiRuntime
    {
        explicit UiRuntime(Runtime& value) : runtime(value) {}
        ~UiRuntime();
        UiRuntime(const UiRuntime&) = delete;
        UiRuntime& operator=(const UiRuntime&) = delete;
        // target_format: the colour format ImGui renders into; INVALID selects the window swapchain's.
        bool initialize(std::string&, SDL_GPUTextureFormat target_format = SDL_GPU_TEXTUREFORMAT_INVALID);
        Runtime& runtime;
        bool context = false, platform = false, renderer = false;
    };
} // namespace OpenBlood2
