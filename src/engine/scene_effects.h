#pragma once
#include "engine/point_light.h"
#include <array>
#include <cstddef>
namespace OpenBlood2
{
    // Fragment slot 0 tail, shared by original and enhanced scene shaders (std140).
    struct SceneEffectsUniform
    {
        static constexpr std::size_t light_limit = 16;
        std::array<float, 4> light_info{};                                   // x: selected light count.
        std::array<std::array<float, 4>, light_limit> positions{}, colors{}; // colors.w: original light flags.
        std::array<float, 4> fog_color{};                                    // w: enabled.
        std::array<float, 4> fog_eye_near{}, fog_forward_far{};
    };
    struct SceneEffects
    {
        SceneEffectsUniform uniform;
        std::size_t light_candidates = 0, light_dropped = 0;
        std::size_t fog_lights_unsupported = 0, shadow_lights_unshadowed = 0;
        bool invalid_fog = false;
    };
} // namespace OpenBlood2
