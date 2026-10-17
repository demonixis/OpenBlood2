#include "engine/world_view.h"
#include "engine/collision.h"
#include <algorithm>
#include <charconv>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        DVector cross(DVector a, DVector b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }
        bool valid_rotation(DRotation q)
        {
            const double n = double(q.m_Spin) * q.m_Spin + double(q.m_Vec.x) * q.m_Vec.x +
                             double(q.m_Vec.y) * q.m_Vec.y + double(q.m_Vec.z) * q.m_Vec.z;
            return std::isfinite(n) && std::abs(n - 1) < .001;
        }
        std::array<float, 4> packed(DVector v)
        {
            return {v.x, v.y, v.z, 0};
        }
    } // namespace
    bool world_surface_effect(const Surface& surface, WorldSurfaceEffect& out)
    {
        out = {};
        auto fail = [&](const char* why)
        {
            out.error = why;
            out.speed = {};
            out.origin_velocity = {};
            out.texel_rate = {};
            return false;
        };
        if (surface.effect_name.size() > 4096 || surface.effect_parameters.size() > 4096)
            return fail("Surface effect text exceeds limits");
        out.name = surface.effect_name;
        out.parameters = surface.effect_parameters;
        if (!surface.has_effect || CaseLess{}(out.name, "Pan") || CaseLess{}("Pan", out.name))
            return fail("Unsupported surface effect");
        std::string_view remaining = out.parameters;
        for (std::size_t axis = 0; !remaining.empty();)
        {
            const auto start = remaining.find_first_not_of(" \t\r\n\v\f");
            if (start == std::string_view::npos)
                break;
            remaining.remove_prefix(start);
            const auto length = remaining.find_first_of(" \t\r\n\v\f");
            auto token = remaining.substr(0, length);
            remaining.remove_prefix(token.size());
            if (axis == 2)
                return fail("Pan supports at most two parameters");
            // Retail uses atof, not directional keywords. These authored labels
            // convert to zero; they must never select an axis or change the sign.
            bool label = false;
            for (auto name : {"up", "down", "left", "right"})
                label |= !CaseLess{}(token, name) && !CaseLess{}(name, token);
            if (!label)
            {
                if (!token.empty() && token.front() == '+')
                {
                    token.remove_prefix(1);
                    if (!token.empty() && token.front() == '-')
                        return fail("Invalid Pan sign");
                }
                float value = 0;
                const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
                if (token.empty() || result.ec != std::errc{} || result.ptr != token.data() + token.size() ||
                    !std::isfinite(value) || std::abs(value) > 65536)
                    return fail("Unsupported or excessive Pan parameter");
                out.speed[axis] = value;
            }
            ++axis;
        }
        if (!physics_vector(surface.u) || !physics_vector(surface.v))
            return fail("Invalid Pan basis");
        out.origin_velocity = vector_add(vector_scale(surface.u, out.speed[0]), vector_scale(surface.v, out.speed[1]));
        const auto velocity = out.origin_velocity;
        for (std::size_t axis = 0; axis < 2; ++axis)
        {
            const auto basis = axis ? surface.v : surface.u;
            const double rate =
                -(double(velocity.x) * basis.x + double(velocity.y) * basis.y + double(velocity.z) * basis.z);
            if (!std::isfinite(rate) || std::abs(rate) > 1e9)
                return fail("Pan coordinate rate exceeds limits");
            out.texel_rate[axis] = rate;
        }
        return true;
    }
    bool world_texture_offset(std::array<double, 2> rate, double time, std::array<float, 4>& out)
    {
        out = {};
        if (!std::isfinite(time) || time < 0)
            return false;
        for (std::size_t axis = 0; axis < 2; ++axis)
        {
            const double distance = rate[axis] * time;
            if (!std::isfinite(rate[axis]) || std::abs(rate[axis]) > 1e9 || !std::isfinite(distance))
            {
                out = {};
                return false;
            }
            out[axis] = static_cast<float>(std::fmod(distance, 1.));
        }
        return true;
    }
    bool WorldMesh::build(const World& world, std::string& error, const Resources* resources)
    {
        *this = {};
        error.clear();
        auto fail = [&](const char* why)
        {
            *this = {};
            error = why;
            return false;
        };
        if (world.models.empty() || world.models.size() > 4097)
            return fail("Diagnostic mesh requires a bounded primary world");
        constexpr std::size_t max_vertices = 3000000;
        materials.push_back({"", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}}});
        texture_bytes = 4;
        std::map<std::string, std::uint32_t> material_ids;
        std::size_t effect_bytes = 0;
        for (const auto& model : world.models)
        {
            if (model.polygons.size() > 65536 || model.points.size() > 65536 || !physics_vector(model.translation))
                return fail("Diagnostic model exceeds geometry limits");
            if (model.surfaces.size() > 2000000)
                return fail("World surface count exceeds limits");
            std::vector<std::uint32_t> effect_ids(model.surfaces.size(), UINT32_MAX);
            for (std::size_t i = 0; i < model.surfaces.size(); ++i)
            {
                const auto& surface = model.surfaces[i];
                if (!surface.has_effect)
                    continue;
                const auto bytes = surface.effect_name.size() + surface.effect_parameters.size();
                if (surface.effect_name.size() > 4096 || surface.effect_parameters.size() > 4096 ||
                    bytes > 8 * 1024 * 1024 - effect_bytes || effects.size() >= 65536)
                    return fail("World surface effect budget exceeded");
                effect_bytes += bytes;
                WorldSurfaceEffect effect;
                world_surface_effect(surface, effect);
                effect.model = static_cast<std::uint32_t>(ranges.size());
                effect.surface = static_cast<std::uint32_t>(i);
                effect_ids[i] = static_cast<std::uint32_t>(effects.size());
                effects.push_back(std::move(effect));
            }
            WorldRange range;
            range.first = static_cast<std::uint32_t>(vertices.size());
            range.name = model.name;
            range.pivot = ranges.empty() ? DVector{} : model.translation;
            for (const auto& polygon : model.polygons)
            {
                const auto count = polygon.base_points == UINT16_MAX ? polygon.points.size() : polygon.base_points;
                if (polygon.surface >= model.surfaces.size() || count < 3 || count > 512 ||
                    count > polygon.points.size())
                    return fail("Invalid diagnostic polygon/ring");
                const auto& surface = model.surfaces[polygon.surface];
                if (surface.plane >= model.planes.size())
                    return fail("Invalid diagnostic surface plane");
                for (auto index : polygon.points)
                    if (index >= model.points.size() || !physics_vector(model.points[index]))
                        return fail("Invalid diagnostic vertex");
                if (surface.flags & (1u << 2))
                {
                    ++invisible;
                    continue;
                }
                transparent += (surface.flags & (1u << 3)) != 0;
                sky += (surface.flags & (1u << 4)) != 0;
                const auto normal = model.planes[surface.plane].normal;
                const auto length = std::sqrt(vector_dot(normal, normal));
                if (!std::isfinite(length) || length < .001f)
                    return fail("Invalid diagnostic face normal");
                // Orientation colors, not an inferred sun or a light service.
                DVector color{.18f + .5f * std::abs(normal.x / length), .18f + .5f * std::abs(normal.y / length),
                              .18f + .5f * std::abs(normal.z / length)};
                if (surface.flags & (1u << 4))
                    color = {.25f, .4f, .65f};
                if ((surface.flags & (1u << 3)) && !(surface.flags & (1u << 4)))
                    color = {.65f, .25f, .5f};
                std::uint32_t material = 0;
                if (resources && !model.textures.empty())
                {
                    if (surface.texture >= model.textures.size() || !physics_vector(surface.origin) ||
                        !physics_vector(surface.u) || !physics_vector(surface.v))
                        return fail("Invalid world texture mapping");
                    const auto& path = model.textures[surface.texture];
                    std::string key;
                    if (!resource_path(path, key))
                        return fail("Invalid world texture resource path");
                    auto found = material_ids.find(key);
                    if (found == material_ids.end())
                    {
                        if (materials.size() >= 4097)
                            return fail("World texture count budget exceeded");
                        WorldMaterial entry;
                        entry.path = key;
                        read_texture_resource(*resources, key, entry.image, entry.error);
                        if (entry.image.rgba.size() > 128 * 1024 * 1024 - texture_bytes)
                            return fail("World texture memory budget exceeded");
                        texture_bytes += entry.image.rgba.size();
                        const auto id = static_cast<std::uint32_t>(materials.size());
                        materials.push_back(std::move(entry));
                        found = material_ids.emplace(key, id).first;
                    }
                    if (!(surface.flags & (1u << 4)))
                    {
                        if (materials[found->second].error.empty())
                        {
                            material = found->second;
                            color = {1, 1, 1}; // Unlit authored albedo.
                        }
                        else
                            color = {1, 0, 1}; // Explicit unresolved material marker.
                    }
                }
                std::array<double, 2> uv_rate{};
                if (material && effect_ids[polygon.surface] != UINT32_MAX)
                {
                    const auto& effect = effects[effect_ids[polygon.surface]];
                    const auto& image = materials[material].image;
                    if (effect.error.empty())
                        uv_rate = {effect.texel_rate[0] / image.width, effect.texel_rate[1] / image.height};
                }
                if ((count - 2) * 3 > max_vertices - vertices.size())
                    return fail("Diagnostic triangle budget exceeded");
                const bool translucent = material && (surface.flags & (1u << 3)) && !(surface.flags & (1u << 4));
                DVector center{};
                if (translucent)
                    for (std::size_t i = 0; i < count; ++i)
                        center = vector_add(
                            center,
                            vector_scale(vector_subtract(model.points[polygon.points[i]], range.pivot), 1.f / count));
                bool first_triangle = true;
                const auto a = model.points[polygon.points[0]];
                for (std::size_t i = 1; i + 1 < count; ++i)
                {
                    const auto b = model.points[polygon.points[i]], c = model.points[polygon.points[i + 1]];
                    const auto area = cross(vector_subtract(b, a), vector_subtract(c, a));
                    if (vector_dot(area, area) < 1e-10f)
                    {
                        ++degenerate;
                        continue;
                    }
                    if (range.batches.empty() || range.batches.back().material != material ||
                        range.batches.back().translucent != translucent ||
                        range.batches.back().sky_portal != bool(surface.flags & (1u << 4)) ||
                        range.batches.back().uv_rate != uv_rate || (translucent && first_triangle))
                        range.batches.push_back({static_cast<std::uint32_t>(vertices.size()), 0, material, translucent,
                                                 center, uv_rate, bool(surface.flags & (1u << 4))});
                    if (translucent && first_triangle)
                        ++blended;
                    first_triangle = false;
                    range.batches.back().count += 3;
                    for (auto p : {a, b, c})
                    {
                        WorldVertex vertex{vector_subtract(p, range.pivot), color};
                        if (material)
                        {
                            const auto delta = vector_subtract(p, surface.origin);
                            const auto& image = materials[material].image;
                            vertex.uv = {vector_dot(delta, surface.u) / image.width,
                                         vector_dot(delta, surface.v) / image.height};
                            if (!std::isfinite(vertex.uv[0]) || !std::isfinite(vertex.uv[1]) ||
                                std::abs(vertex.uv[0]) > 1e6f || std::abs(vertex.uv[1]) > 1e6f)
                                return fail("World texture coordinates exceed limits");
                        }
                        vertices.push_back(vertex);
                    }
                }
            }
            range.count = static_cast<std::uint32_t>(vertices.size()) - range.first;
            ranges.push_back(std::move(range));
        }
        return true;
    }
    bool world_alpha_order(const WorldMesh& mesh, std::span<const WorldDraw> draws, DVector eye, DRotation view,
                           std::vector<WorldAlphaBatch>& order, std::string& error)
    {
        order.clear();
        error.clear();
        auto fail = [&]()
        {
            order.clear();
            error = "Invalid or excessive transparent world draw";
            return false;
        };
        if (!physics_vector(eye) || !valid_rotation(view) || draws.size() > 65536)
            return fail();
        const auto forward = rotate_vector({0, 0, 1}, view);
        for (std::size_t i = 0; i < draws.size(); ++i)
        {
            const auto& d = draws[i];
            if (d.range >= mesh.ranges.size() || !physics_vector(d.position) || !valid_rotation(d.rotation))
                return fail();
            const auto& range = mesh.ranges[d.range];
            for (std::size_t j = 0; j < range.batches.size(); ++j)
            {
                const auto& b = range.batches[j];
                if (!b.translucent)
                    continue;
                if (!b.count || b.count % 3 || b.first < range.first || b.first > mesh.vertices.size() ||
                    b.count > mesh.vertices.size() - b.first || b.first > std::size_t(range.first) + range.count ||
                    b.count > std::size_t(range.first) + range.count - b.first || !physics_vector(b.center) ||
                    order.size() >= 1000000)
                    return fail();
                const auto center = vector_add(d.position, rotate_vector(b.center, d.rotation));
                const auto delta = vector_subtract(center, eye);
                const double depth =
                    double(delta.x) * forward.x + double(delta.y) * forward.y + double(delta.z) * forward.z;
                if (!std::isfinite(depth))
                    return fail();
                order.push_back({i, j, depth});
            }
        }
        // Explicit index tie-break preserves source order without stable_sort's temporary allocation.
        std::sort(order.begin(), order.end(),
                  [](const auto& a, const auto& b)
                  {
                      if (a.depth != b.depth)
                          return a.depth > b.depth;
                      if (a.draw != b.draw)
                          return a.draw < b.draw;
                      return a.batch < b.batch;
                  });
        return true;
    }
    bool world_draws(const WorldMesh& mesh, const Objects& objects, std::vector<WorldDraw>& draws,
                     WorldViewStats& stats, std::string& error)
    {
        draws.clear();
        stats.triangles = stats.draws = stats.hidden = stats.unbound = stats.containers = stats.batches = 0;
        error.clear();
        if (mesh.ranges.empty())
            return true;
        draws.push_back({0, {}, {}});
        std::vector<bool> bound(mesh.ranges.size());
        bound[0] = true;
        for (auto id : objects.snapshot())
        {
            const auto* r = objects.get(id);
            if (!r || r->life != Life::Alive)
                continue;
            if (r->state.m_ObjectType == OT_CONTAINER)
            {
                ++stats.containers;
                continue;
            }
            if (r->state.m_ObjectType != OT_WORLDMODEL)
                continue;
            std::size_t match = 0;
            for (std::size_t i = 1; i < mesh.ranges.size(); ++i)
                if (!CaseLess{}(mesh.ranges[i].name, r->state.m_Filename) &&
                    !CaseLess{}(r->state.m_Filename, mesh.ranges[i].name))
                {
                    if (match)
                    {
                        error = "Ambiguous diagnostic world model";
                        draws.clear();
                        return false;
                    }
                    match = i;
                }
            if (!match || !r->world_model)
            {
                error = "Missing diagnostic world model binding";
                draws.clear();
                return false;
            }
            bound[match] = true;
            if (r->state.m_Flags & FLAG_SKYOBJECT)
                continue;
            if (!(r->state.m_Flags & FLAG_VISIBLE))
            {
                ++stats.hidden;
                continue;
            }
            if (!physics_vector(r->state.m_Pos) || !valid_rotation(r->state.m_Rotation))
            {
                error = "Invalid diagnostic world model transform";
                draws.clear();
                return false;
            }
            draws.push_back({static_cast<std::uint32_t>(match), r->state.m_Pos, r->state.m_Rotation});
        }
        stats.unbound = std::count(bound.begin(), bound.end(), false);
        for (const auto& draw : draws)
        {
            stats.triangles += mesh.ranges[draw.range].count / 3;
            stats.batches += mesh.ranges[draw.range].batches.empty() ? 1 : mesh.ranges[draw.range].batches.size();
        }
        stats.draws = draws.size();
        return true;
    }
    bool world_view_uniform(DVector eye, DRotation view, float aspect, const WorldDraw& draw, WorldViewUniform& out)
    {
        out = {};
        if (!physics_vector(eye) || !physics_vector(draw.position) || !valid_rotation(view) ||
            !valid_rotation(draw.rotation) || !std::isfinite(aspect) || aspect <= 0 || aspect > 128)
            return false;
        out.eye = packed(eye);
        out.right = packed(rotate_vector(DVector{1, 0, 0}, view));
        out.up = packed(rotate_vector(DVector{0, 1, 0}, view));
        out.forward = packed(rotate_vector(DVector{0, 0, 1}, view));
        constexpr float near = 1, far = 65536;
        out.projection = {1, aspect, far / (far - near), -near * far / (far - near)};
        out.position = packed(draw.position);
        out.quaternion = {draw.rotation.m_Vec.x, draw.rotation.m_Vec.y, draw.rotation.m_Vec.z, draw.rotation.m_Spin};
        return true;
    }
} // namespace OpenBlood2
