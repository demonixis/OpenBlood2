#include "engine/world_lights.h"
#include "engine/collision.h"
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        template <typename T> const T* get(const WorldObject& object, std::string_view name)
        {
            const auto* value = object.find(name);
            return value ? std::get_if<T>(value) : nullptr;
        }
    } // namespace
    bool world_static_lights(const World& world, std::vector<StaticLight>& out, StaticLightStats& stats,
                             std::string& error)
    {
        out.clear();
        stats = {};
        error.clear();
        for (const auto& object : world.objects)
        {
            const auto& name = object.class_name;
            const bool point = name == "Light" || name == "ObjectLight", cone = name == "DirLight";
            if (!point && !cone)
                continue;
            if (const auto* objects = get<bool>(object, "LightObjects"); objects && !*objects)
            {
                ++stats.without_objects;
                continue;
            }
            const auto* position = get<DVector>(object, "Pos");
            const auto* radius = get<float>(object, "LightRadius");
            const auto* color = get<DVector>(object, cone ? "InnerColor" : "LightColor");
            const auto* bright = get<float>(object, "BrightScale");
            StaticLight light;
            if (!position || !radius || !color || !physics_vector(*position) || !physics_vector(*color) ||
                !std::isfinite(*radius) || *radius <= 0 || *radius > 1e6f || (bright && !std::isfinite(*bright)))
            {
                ++stats.invalid;
                continue;
            }
            const float scale = (bright ? *bright : 1.f) / 255;
            light.position = *position;
            light.radius = *radius;
            light.color = {std::max(0.f, color->x * scale), std::max(0.f, color->y * scale),
                           std::max(0.f, color->z * scale)};
            const auto* clip = get<bool>(object, "ClipLight");
            light.clip = !clip || *clip;
            if (const auto* outer = get<DVector>(object, "OuterColor"); outer && (outer->x || outer->y || outer->z))
                ++stats.outer;
            if (cone)
            {
                const auto* fov = get<float>(object, "FOV");
                const auto* rotation = get<EulerAngles>(object, "Rotation");
                if (!fov || !std::isfinite(*fov) || *fov <= 0 || (rotation && !physics_vector(rotation->radians)))
                {
                    ++stats.invalid;
                    continue;
                }
                light.direction =
                    rotate_vector({0, 0, 1}, rotation_from_euler(rotation ? rotation->radians : DVector{}));
                // A FOV of 360 degrees or more covers every direction.
                light.cos_half = *fov >= 360 ? -2 : std::min(std::cos(*fov * 3.14159265358979f / 360), .9999f);
            }
            out.push_back(light);
        }
        stats.lights = out.size();
        if (out.size() > 65536)
        {
            out.clear();
            error = "World static light budget exceeded";
            return false;
        }
        return true;
    }
    DVector static_light_at(const StaticLight& light, DVector point)
    {
        const auto delta = vector_subtract(light.position, point);
        const float distance = std::sqrt(vector_dot(delta, delta));
        if (distance >= light.radius)
            return {};
        float factor = 1 - distance / light.radius;
        if (light.cos_half >= -1 && distance > 0)
        {
            const float cosine = -vector_dot(light.direction, delta) / distance;
            if (cosine < light.cos_half)
                return {};
            factor *= (cosine - light.cos_half) / (1 - light.cos_half);
        }
        return vector_scale(light.color, factor);
    }
} // namespace OpenBlood2
