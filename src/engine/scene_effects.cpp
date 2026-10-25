#include "engine/scene_effects.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    void select_effect_lights(const SceneEffects& scene, EffectLightBounds bounds, DVector position, DRotation rotation,
                              bool model, SceneEffectsUniform& output)
    {
        output = {};
        output.fog_color = scene.uniform.fog_color;
        output.fog_eye_near = scene.uniform.fog_eye_near;
        output.fog_forward_far = scene.uniform.fog_forward_far;
        struct Ranked
        {
            std::size_t index;
            double weight;
        };
        std::array<Ranked, SceneEffectsUniform::light_limit> ranked{};
        const auto budget = std::min(scene.light_budget, platform_effect_lights);
        std::size_t count = 0, eligible = 0;
        bounds.bounded = bounds.bounded && physics_vector(bounds.min) && physics_vector(bounds.max) &&
                         bounds.min.x <= bounds.max.x && bounds.min.y <= bounds.max.y && bounds.min.z <= bounds.max.z;
        const auto inverse = inverse_rotation(rotation);
        for (std::size_t index = 0; index < scene.lights.size(); ++index)
        {
            const auto& candidate = scene.lights[index];
            const auto& light = candidate.light;
            // World-only lights must not consume model slots; fog lights are not surface lights.
            if ((candidate.flags & 128u) || (model && (candidate.flags & 32u)) || !(light.radius > 0) ||
                !std::isfinite(light.radius) || !physics_vector(light.position) || !physics_vector(light.color) ||
                light.radius > 1e7f || light.color.x < 0 || light.color.y < 0 || light.color.z < 0 ||
                light.color.x > 1 || light.color.y > 1 || light.color.z > 1)
                continue;
            const double brightness = std::max({light.color.x, light.color.y, light.color.z});
            if (!(brightness > 0))
                continue;
            double distance_squared = 0;
            if (bounds.bounded)
            {
                const auto p = rotate_vector(vector_subtract(light.position, position), inverse);
                const double dx = double(p.x) - std::clamp(p.x, bounds.min.x, bounds.max.x);
                const double dy = double(p.y) - std::clamp(p.y, bounds.min.y, bounds.max.y);
                const double dz = double(p.z) - std::clamp(p.z, bounds.min.z, bounds.max.z);
                distance_squared = dx * dx + dy * dy + dz * dz;
                if (distance_squared >= double(light.radius) * light.radius)
                    continue;
            }
            // Upper bound of the existing linear radial illumination anywhere on the receiver.
            const double weight = brightness * (1 - std::sqrt(distance_squared) / light.radius);
            ++eligible;
            std::size_t slot = 0;
            while (slot < count && ranked[slot].weight >= weight)
                ++slot; // Stable source-order ties.
            if (slot >= budget)
                continue;
            const auto last = std::min(count, budget - 1);
            for (auto i = last; i > slot; --i)
                ranked[i] = ranked[i - 1];
            ranked[slot] = {index, weight};
            count = std::min(count + 1, budget);
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& candidate = scene.lights[ranked[i].index];
            const auto& light = candidate.light;
            output.positions[i] = {light.position.x, light.position.y, light.position.z, light.radius};
            output.colors[i] = {light.color.x, light.color.y, light.color.z, float(candidate.flags)};
        }
        output.light_info[0] = float(count);
        ++scene.selection.receivers;
        scene.selection.selected += count;
        scene.selection.dropped += eligible - count;
        scene.selection.max_selected = std::max(scene.selection.max_selected, count);
    }
} // namespace OpenBlood2
