#pragma once
#include "engine/point_light.h"
#include <SDL3/SDL_platform_defines.h>
#include <array>
#include <cstddef>
#include <vector>
namespace OpenBlood2
{
    // One shader ABI on every backend; mobile submissions only fill/visit twelve slots.
    inline constexpr std::size_t mobile_effect_lights = 12, desktop_effect_lights = 32;
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
    inline constexpr std::size_t platform_effect_lights = mobile_effect_lights;
#else
    inline constexpr std::size_t platform_effect_lights = desktop_effect_lights;
#endif
    struct SceneEffectsUniform
    {
        static constexpr std::size_t light_limit = desktop_effect_lights;
        std::array<float, 4> light_info{};                                   // x: selected light count.
        std::array<std::array<float, 4>, light_limit> positions{}, colors{}; // colors.w: original light flags.
        std::array<float, 4> fog_color{};                                    // w: enabled.
        std::array<float, 4> fog_eye_near{}, fog_forward_far{};
    };
    struct SceneEffectLight
    {
        PointLight light;
        DDWORD flags = 0;
    };
    // Local bounds of the receiving geometry, before its rigid draw transform.
    struct EffectLightBounds
    {
        DVector min{}, max{};
        bool bounded = false;
    };
    struct EffectLightSelectionStats
    {
        std::size_t receivers = 0, selected = 0, dropped = 0, max_selected = 0;
    };
    struct SceneEffects
    {
        SceneEffectsUniform uniform;          // View fog only; lights are selected at each draw.
        std::vector<SceneEffectLight> lights; // All live candidates, in source/creation order.
        std::size_t light_budget = platform_effect_lights;
        std::size_t fog_lights_unsupported = 0, shadow_lights_unshadowed = 0;
        bool invalid_fog = false;
        // Diagnostic references per submitted receiver, not a count of unique lights.
        mutable EffectLightSelectionStats selection;
    };
    // Reject out-of-range lights, then keep the strongest bounded subset. No eye dependency,
    // allocations, RNG or handles. An unknown bound is conservative, never an invented point.
    void select_effect_lights(const SceneEffects&, EffectLightBounds, DVector position, DRotation rotation, bool model,
                              SceneEffectsUniform&);
} // namespace OpenBlood2
