#include "engine/gi_world.h"
#include "engine/collision.h"
#include "engine/world_view.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>

namespace OpenBlood2
{
    namespace
    {
        bool same_name(std::string_view a, std::string_view b)
        {
            return !CaseLess{}(a, b) && !CaseLess{}(b, a);
        }
        bool in(std::string_view name, std::initializer_list<std::string_view> list)
        {
            return std::any_of(list.begin(), list.end(),
                               [&](std::string_view entry) { return same_name(name, entry); });
        }
        // Unbound for classes that never create a world model or container.
        WorldModelRole class_role(std::string_view name)
        {
            if (in(name, {"DemoSkyWorldModel"}))
                return WorldModelRole::Sky;
            if (in(name, {"VolumeBrush", "Water", "Blood", "Acid", "Ladder", "Conveyor", "Damage", "FreeFall",
                          "Minefield", "Container"}))
                return WorldModelRole::Container;
            if (in(name, {"Door", "RotatingDoor", "RotatingBrush"}))
                return WorldModelRole::Mover;
            if (in(name, {"CDestructableBrush", "WorldModelDebris"}))
                return WorldModelRole::Destructible;
            return WorldModelRole::Unbound;
        }
        std::array<int, 3> expand_565(std::uint16_t texel)
        {
            const unsigned r = texel >> 11 & 31, g = texel >> 5 & 63, b = texel & 31;
            return {int(r << 3 | r >> 2), int(g << 2 | g >> 4), int(b << 3 | b >> 2)};
        }
    } // namespace

    std::array<double, 2> LightmapFrame::coordinate(DVector point) const
    {
        // Same arithmetic as the mesh's vertex lightmap coordinates (float dot, double offset).
        return {(vector_dot(p, point) - origin[0]) / grid + .5, (vector_dot(q, point) - origin[1]) / grid + .5};
    }
    bool LightmapFrame::position(double sx, double sy, DVector& out) const
    {
        // Solve [p; q; normal] x = [origin_p + 20 sx, origin_q + 20 sy, distance] (Cramer).
        const double a[3][3]{{p.x, p.y, p.z}, {q.x, q.y, q.z}, {normal.x, normal.y, normal.z}};
        const double b[3]{origin[0] + grid * sx, origin[1] + grid * sy, distance};
        auto det = [](const double m[3][3])
        {
            return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                   m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                   m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        };
        const double d = det(a);
        if (!std::isfinite(d) || std::abs(d) < 1e-12)
            return false;
        double x[3];
        for (int column = 0; column < 3; ++column)
        {
            double m[3][3];
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    m[r][c] = c == column ? b[r] : a[r][c];
            x[column] = det(m) / d;
        }
        if (!std::isfinite(x[0]) || !std::isfinite(x[1]) || !std::isfinite(x[2]))
            return false;
        out = {float(x[0]), float(x[1]), float(x[2])};
        return true;
    }
    bool lightmap_frame(const WorldModel& model, const Polygon& polygon, std::size_t count, LightmapFrame& frame)
    {
        frame = {};
        if (!model.lightmaps || polygon.surface >= model.surfaces.size() || !polygon.lightmap_width ||
            !polygon.lightmap_height || count < 3 || count > polygon.points.size())
            return false;
        const auto& surface = model.surfaces[polygon.surface];
        if (!(surface.flags & (1u << 7)) || surface.plane >= model.planes.size() ||
            std::size_t(polygon.lightmap_offset) + std::size_t(polygon.lightmap_width) * polygon.lightmap_height >
                model.lightmap_texels.size())
            return false;
        for (std::size_t i = 0; i < count; ++i)
            if (polygon.points[i] >= model.points.size())
                return false;
        frame.p = surface.lightmap_p;
        frame.q = surface.lightmap_q;
        frame.normal = model.planes[surface.plane].normal;
        frame.distance = model.planes[surface.plane].distance;
        frame.width = polygon.lightmap_width;
        frame.height = polygon.lightmap_height;
        const std::array<DVector, 2> axes{frame.p, frame.q};
        const std::array<unsigned, 2> texels{frame.width, frame.height};
        for (int k = 0; k < 2; ++k)
        {
            double lo = std::numeric_limits<double>::max(), hi = -lo;
            for (std::size_t i = 0; i < count; ++i)
            {
                const double d = vector_dot(axes[k], model.points[polygon.points[i]]);
                lo = std::min(lo, d);
                hi = std::max(hi, d);
            }
            frame.unfitted += !lightmap_origin(lo, hi, texels[k], frame.origin[k]);
        }
        return true;
    }

    PointContents point_contents(const WorldModel& model, DVector point)
    {
        if (model.bsp_nodes.empty())
            return PointContents::Unknown;
        std::int32_t n = model.root_node;
        // The original descent (WorldVisibility::find_leaf): back side below -0.0001.
        for (std::size_t steps = 0; n >= 0 && steps <= model.bsp_nodes.size(); ++steps)
        {
            if (std::size_t(n) >= model.bsp_nodes.size())
                return PointContents::Unknown;
            const auto& node = model.bsp_nodes[n];
            if (node.plane >= model.planes.size())
                return PointContents::Unknown;
            const auto& plane = model.planes[node.plane];
            const double d = double(plane.normal.x) * point.x + double(plane.normal.y) * point.y +
                             double(plane.normal.z) * point.z - plane.distance;
            n = node.sides[d < -0.0001f ? 0 : 1];
        }
        return n == BspNode::inside    ? PointContents::Empty
               : n == BspNode::outside ? PointContents::Solid
                                       : PointContents::Unknown;
    }

    const char* world_model_role_name(WorldModelRole role)
    {
        switch (role)
        {
        case WorldModelRole::Primary: return "primary";
        case WorldModelRole::Sky: return "sky";
        case WorldModelRole::Container: return "container";
        case WorldModelRole::Mover: return "mover";
        case WorldModelRole::Destructible: return "destructible";
        case WorldModelRole::Unbound: return "unbound";
        }
        return "unknown";
    }
    std::vector<WorldModelRoleInfo> world_model_roles(const World& world)
    {
        std::vector<WorldModelRoleInfo> roles(world.models.size());
        if (roles.empty())
            return roles;
        roles[0].role = WorldModelRole::Primary;
        // First model in file order for each name, as object creation binds it.
        std::map<std::string, std::size_t, CaseLess> first;
        for (std::size_t i = 1; i < world.models.size(); ++i)
            first.try_emplace(world.models[i].name, i);
        for (const auto& object : world.objects)
        {
            const auto* name = object.find("Name");
            const auto* text = name ? std::get_if<std::string>(name) : nullptr;
            if (!text || text->empty())
                continue;
            const auto role = class_role(object.class_name);
            const auto found = first.find(*text);
            if (role == WorldModelRole::Unbound || found == first.end())
                continue;
            auto& info = roles[found->second];
            if (!info.objects++)
            {
                info.object_class = object.class_name;
                info.role = role;
            }
        }
        return roles;
    }

    std::vector<WorldModelBounds> world_model_bounds(const World& world)
    {
        std::vector<WorldModelBounds> bounds(world.models.size());
        for (std::size_t i = 0; i < world.models.size(); ++i)
        {
            const auto& points = world.models[i].points;
            if (points.empty())
                continue;
            auto& b = bounds[i];
            b.min = b.max = points.front();
            for (const auto& p : points)
            {
                b.min = {std::min(b.min.x, p.x), std::min(b.min.y, p.y), std::min(b.min.z, p.z)};
                b.max = {std::max(b.max.x, p.x), std::max(b.max.y, p.y), std::max(b.max.z, p.z)};
            }
        }
        return bounds;
    }
    std::int32_t world_model_containing(std::span<const WorldModelBounds> bounds,
                                        std::span<const WorldModelRoleInfo> roles, DVector point, float margin)
    {
        for (std::size_t i = 1; i < bounds.size() && i < roles.size(); ++i)
        {
            if (roles[i].role == WorldModelRole::Sky)
                continue;
            const auto& b = bounds[i];
            if (b.min.x > b.max.x)
                continue;
            if (point.x >= b.min.x - margin && point.x <= b.max.x + margin && point.y >= b.min.y - margin &&
                point.y <= b.max.y + margin && point.z >= b.min.z - margin && point.z <= b.max.z + margin)
                return static_cast<std::int32_t>(i);
        }
        return -1;
    }
    bool point_clear(const CollisionWorld& collision, DVector point, float half, bool& clear)
    {
        CollisionHit hit;
        const auto result = collision.sweep(point, point, {half, half, half}, hit);
        if (result == CollisionQuery::Invalid)
            return false;
        clear = result == CollisionQuery::Miss;
        return true;
    }
    bool world_info_ambient_any_case(std::string_view info, DVector& ambient)
    {
        // The records and number rules of world_info_ambient, with a case-insensitive key.
        while (!info.empty())
        {
            const auto end = std::min(info.find(';'), info.size());
            auto record = info.substr(0, end);
            info.remove_prefix(std::min(end + 1, info.size()));
            while (!record.empty() && std::isspace(static_cast<unsigned char>(record.front())))
                record.remove_prefix(1);
            constexpr std::string_view key = "AmbientLight";
            if (record.size() <= key.size() || !same_name(record.substr(0, key.size()), key) ||
                !std::isspace(static_cast<unsigned char>(record[key.size()])))
                continue;
            record.remove_prefix(key.size());
            float channels[3]{};
            for (auto& channel : channels)
            {
                while (!record.empty() && std::isspace(static_cast<unsigned char>(record.front())))
                    record.remove_prefix(1);
                const auto [next, error] = std::from_chars(record.data(), record.data() + record.size(), channel);
                if (error != std::errc{} || !std::isfinite(channel) || channel < 0 || channel > 255)
                    return false;
                record.remove_prefix(static_cast<std::size_t>(next - record.data()));
            }
            ambient = {channels[0], channels[1], channels[2]};
            return true;
        }
        return false;
    }
    AmbientAudit audit_ambient(const World& world)
    {
        AmbientAudit audit;
        audit.has_ambient = world_info_ambient_any_case(world.info, audit.ambient);
        if (audit.has_ambient)
        {
            // Mirror of the original renderer's case-sensitive key test.
            bool exact = false;
            for (const auto& command : world_info_commands(world.info))
                exact |= !command.empty() && command[0] == "AmbientLight";
            audit.case_mismatch = !exact;
        }
        const float a[3]{audit.ambient.x, audit.ambient.y, audit.ambient.z};
        constexpr int step[3]{8, 4, 8}; // RGB565 quantization of each expanded channel.
        for (const auto& model : world.models)
            for (const auto& polygon : model.polygons)
            {
                if (polygon.surface >= model.surfaces.size())
                    continue;
                const auto flags = model.surfaces[polygon.surface].flags;
                if (flags & ((1u << 2) | (1u << 4) | (1u << 5)))
                    continue; // Invisible, sky and SURF_BRIGHT faces carry no baked light.
                for (const auto& color : polygon.colors)
                {
                    bool below = false;
                    for (int k = 0; k < 3; ++k)
                    {
                        audit.darkest_vertex[k] = std::min<int>(audit.darkest_vertex[k], color[k]);
                        below |= color[k] + .5f < a[k];
                    }
                    ++audit.vertices;
                    audit.vertices_below += below;
                }
                if (!model.lightmaps || !(flags & (1u << 7)) || !polygon.lightmap_width || !polygon.lightmap_height)
                    continue;
                const std::size_t n = std::size_t(polygon.lightmap_width) * polygon.lightmap_height;
                if (std::size_t(polygon.lightmap_offset) + n > model.lightmap_texels.size())
                    continue;
                for (std::size_t i = 0; i < n; ++i)
                {
                    const auto rgb = expand_565(model.lightmap_texels[polygon.lightmap_offset + i]);
                    bool below = false;
                    for (int k = 0; k < 3; ++k)
                    {
                        audit.darkest_texel[k] = std::min(audit.darkest_texel[k], rgb[k]);
                        below |= rgb[k] + step[k] < a[k];
                    }
                    ++audit.texels;
                    audit.texels_below += below;
                }
            }
        return audit;
    }

    bool world_dat_digest(const Resources& resources, std::string_view world_path, Sha256Digest& digest,
                          std::string& error)
    {
        std::string path;
        if (!resource_path(world_path, path) || !resources.find(path))
        {
            error = "World " + std::string(world_path) + " is not in the mounted resources";
            return false;
        }
        std::vector<std::uint8_t> bytes;
        if (!resources.read(path, bytes, error))
            return false;
        digest = sha256(bytes);
        return true;
    }
} // namespace OpenBlood2
