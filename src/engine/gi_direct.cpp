#include "engine/gi_direct.h"
#include "engine/gi_jobs.h"
#include "engine/properties.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>

namespace OpenBlood2
{
    namespace
    {
        namespace L = gi_direct_limits;
        template <typename T> const T* get(const WorldObject& object, std::string_view name)
        {
            const auto* value = object.find(name);
            return value ? std::get_if<T>(value) : nullptr;
        }
        bool finite(DVector v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }
        std::array<double, 3> scaled(DVector color, double scale)
        {
            return {color.x * scale, color.y * scale, color.z * scale};
        }
        bool nonzero(const DVector* v)
        {
            return v && (v->x != 0 || v->y != 0 || v->z != 0);
        }
    } // namespace

    void gi_sin_cos(double angle, double& sine, double& cosine)
    {
        constexpr double half_pi = 1.57079632679489661923;
        const double turns = std::nearbyint(angle / half_pi);
        double s, c;
        gi_quarter_sin_cos(angle - turns * half_pi, s, c);
        const auto quadrant = static_cast<long long>(std::fmod(turns, 4.0) + 4) % 4;
        switch (quadrant)
        {
        case 0:
            sine = s, cosine = c;
            break;
        case 1:
            sine = c, cosine = -s;
            break;
        case 2:
            sine = -s, cosine = -c;
            break;
        default:
            sine = -c, cosine = s;
            break;
        }
    }

    bool gi_direct_lights(const World& world, std::vector<GiDirectLight>& out, GiDirectLightReport& report,
                          std::string& error)
    {
        out.clear();
        report = {};
        error.clear();
        const WorldModel* root = world.models.empty() ? nullptr : &world.models[0];
        for (std::size_t index = 0; index < world.objects.size(); ++index)
        {
            const auto& object = world.objects[index];
            const auto& name = object.class_name;
            if (name == "ObjectLight")
                ++report.object_lights;
            if (name == "GlobalDirLight")
                ++report.global_dir_lights;
            const bool cone = name == "DirLight";
            if (name != "Light" && !cone)
                continue;
            const auto* position = get<DVector>(object, "Pos");
            const auto* radius = get<float>(object, "LightRadius");
            const auto* color = get<DVector>(object, cone ? "InnerColor" : "LightColor");
            const auto* outer = get<DVector>(object, "OuterColor");
            const auto* bright = get<float>(object, "BrightScale");
            if (!position || !radius || !color || !finite(*position) || !finite(*color) || !std::isfinite(*radius) ||
                *radius <= 0 || *radius > 1e6f || (bright && !std::isfinite(*bright)) || (outer && !finite(*outer)))
            {
                ++report.invalid;
                continue;
            }
            GiDirectLight light;
            light.object = std::uint32_t(index);
            light.position = *position;
            light.radius = *radius;
            const double scale = bright ? *bright : 1.0;
            report.negative += scale < 0;
            light.inner = scaled(*color, scale);
            const auto* clip = get<bool>(object, "ClipLight");
            light.clip = !clip || *clip;
            if (cone)
            {
                const auto* fov = get<float>(object, "FOV");
                const auto* rotation = get<EulerAngles>(object, "Rotation");
                if (!fov || !std::isfinite(*fov) || *fov <= 0 || (rotation && !finite(rotation->radians)))
                {
                    ++report.invalid;
                    continue;
                }
                if (nonzero(outer))
                {
                    light.outer = scaled(*outer, scale);
                    ++report.cone_outer;
                }
                // rotation_from_euler, then +Z rotated by the quaternion (rotate_vector's rule).
                const DVector r = rotation ? rotation->radians : DVector{};
                double sx, cx, sy, cy, sz, cz;
                gi_sin_cos(r.x / 2.0, sx, cx);
                gi_sin_cos(r.y / 2.0, sy, cy);
                gi_sin_cos(r.z / 2.0, sz, cz);
                const double qx = cy * sx * cz + sy * cx * sz, qy = sy * cx * cz - cy * sx * sz;
                const double qz = cy * cx * sz - sy * sx * cz, qw = cy * cx * cz + sy * sx * sz;
                light.direction = {2 * (qx * qz + qy * qw), 2 * (qy * qz - qx * qw), 1 - 2 * (qx * qx + qy * qy)};
                if (*fov < 360)
                {
                    double s, c;
                    gi_sin_cos(double(*fov) * (3.14159265358979323846 / 360), s, c);
                    light.cos_half = std::min(c, .9999);
                }
                ++report.cones;
            }
            else
            {
                if (nonzero(outer))
                {
                    light.outer = scaled(*outer, scale);
                    ++report.omni_outer;
                }
                ++report.omni;
            }
            report.unclipped += !light.clip;
            light.in_solid = root && point_contents(*root, light.position) == PointContents::Solid;
            report.in_solid += light.in_solid;
            out.push_back(light);
            if (out.size() > L::max_lights)
            {
                out.clear();
                error = "World direct light budget exceeded";
                return false;
            }
        }
        report.lights = out.size();
        return true;
    }

    std::array<double, 3> gi_direct_term(const GiDirectLight& light, DVector point)
    {
        const double dx = double(light.position.x) - point.x, dy = double(light.position.y) - point.y,
                     dz = double(light.position.z) - point.z;
        const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!(distance < light.radius))
            return {};
        const double falloff = 1 - distance / light.radius;
        std::array<double, 3> out;
        if (light.cos_half < -1)
        {
            for (int k = 0; k < 3; ++k)
                out[k] = light.inner[k] * falloff + light.outer[k] * (1 - falloff);
            return out;
        }
        double a = 1;
        if (distance > 0)
        {
            const auto& d = light.direction;
            const double c = -(d[0] * dx + d[1] * dy + d[2] * dz) / distance;
            if (c < light.cos_half)
                return {};
            a = (c - light.cos_half) / (1 - light.cos_half);
        }
        for (int k = 0; k < 3; ++k)
            out[k] = (light.inner[k] * a + light.outer[k] * (1 - a)) * falloff;
        return out;
    }

    bool GiDirectOccluders::build(const World& world, std::string& error)
    {
        models.assign(world.models.size(), GiBvh{});
        triangles = 0;
        for (std::size_t m = 0; m < world.models.size(); ++m)
        {
            const auto& model = world.models[m];
            std::vector<GiBvh::Triangle> list;
            for (std::uint32_t i = 0; i < model.polygons.size(); ++i)
            {
                const auto& polygon = model.polygons[i];
                if (polygon.surface >= model.surfaces.size() || polygon.points.size() < 3)
                {
                    error = "Direct-light occluders of model " + std::to_string(m) + ": invalid polygon " +
                            std::to_string(i);
                    models.clear();
                    return false;
                }
                // SURF_SOLID polygons cast shadows, SURF_INVISIBLE ones do not.
                const auto flags = model.surfaces[polygon.surface].flags;
                if (!(flags & 1) || (flags & 4))
                    continue;
                for (const auto index : polygon.points)
                    if (index >= model.points.size() || !finite(model.points[index]))
                    {
                        error = "Direct-light occluders of model " + std::to_string(m) + ": invalid point";
                        models.clear();
                        return false;
                    }
                const auto& p0 = model.points[polygon.points[0]];
                for (std::size_t j = 1; j + 1 < polygon.points.size(); ++j)
                {
                    const auto& p1 = model.points[polygon.points[j]];
                    const auto& p2 = model.points[polygon.points[j + 1]];
                    GiBvh::Triangle t{{p0.x, p0.y, p0.z},
                                      {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z},
                                      {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z},
                                      i};
                    const float cx = t.e1[1] * t.e2[2] - t.e1[2] * t.e2[1], cy = t.e1[2] * t.e2[0] - t.e1[0] * t.e2[2],
                                cz = t.e1[0] * t.e2[1] - t.e1[1] * t.e2[0];
                    if (cx * cx + cy * cy + cz * cz < 1e-12f)
                        continue;
                    if (list.size() + triangles >= gi_bake_limits::max_triangles)
                    {
                        error = "Direct-light occluders: too many triangles";
                        models.clear();
                        return false;
                    }
                    list.push_back(t);
                }
            }
            triangles += list.size();
            if (!models[m].build(std::move(list), error))
            {
                error = "Direct-light occluders of model " + std::to_string(m) + ": " + error;
                models.clear();
                return false;
            }
        }
        return true;
    }

    bool gi_segment_blocked(const GiBvh& tree, DVector from, DVector to)
    {
        const auto nodes = tree.nodes();
        const auto triangles = tree.triangles();
        if (nodes.empty())
            return false;
        const float o[3]{from.x, from.y, from.z}, d[3]{to.x - from.x, to.y - from.y, to.z - from.z};
        const float length = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (!(length > 0))
            return false;
        // A start touching a triangle does not block (the old zero-box sweep's touching rule).
        const float skip = float(L::touch) / length;
        float inv[3];
        for (int a = 0; a < 3; ++a)
            inv[a] = d[a] != 0 ? 1 / d[a] : std::copysign(1e30f, d[a]);
        std::uint32_t stack[gi_bake_limits::max_depth + 2];
        std::uint32_t top = 0, index = 0;
        for (;;)
        {
            const auto& node = nodes[index];
            float near = 0, far = 1;
            for (int a = 0; a < 3; ++a)
            {
                float t0 = (node.min[a] - o[a]) * inv[a], t1 = (node.max[a] - o[a]) * inv[a];
                if (t0 > t1)
                    std::swap(t0, t1);
                near = std::max(near, t0);
                far = std::min(far, t1);
            }
            if (near <= far)
            {
                if (!node.count)
                {
                    stack[top++] = node.index + 1;
                    index = node.index;
                    continue;
                }
                for (std::uint32_t i = node.index; i < node.index + node.count; ++i)
                {
                    const auto& t = triangles[i];
                    // Two-sided Moller-Trumbore, the bake tracer's arithmetic, any hit in [0, 1].
                    const float px = d[1] * t.e2[2] - d[2] * t.e2[1], py = d[2] * t.e2[0] - d[0] * t.e2[2],
                                pz = d[0] * t.e2[1] - d[1] * t.e2[0];
                    const float det = t.e1[0] * px + t.e1[1] * py + t.e1[2] * pz;
                    if (std::abs(det) < 1e-12f)
                        continue;
                    const float sx = o[0] - t.v0[0], sy = o[1] - t.v0[1], sz = o[2] - t.v0[2];
                    const float u = (sx * px + sy * py + sz * pz) / det;
                    if (u < 0 || u > 1)
                        continue;
                    const float qx = sy * t.e1[2] - sz * t.e1[1], qy = sz * t.e1[0] - sx * t.e1[2],
                                qz = sx * t.e1[1] - sy * t.e1[0];
                    const float v = (d[0] * qx + d[1] * qy + d[2] * qz) / det;
                    if (v < 0 || u + v > 1)
                        continue;
                    const float f = (t.e2[0] * qx + t.e2[1] * qy + t.e2[2] * qz) / det;
                    if (f > skip && f <= 1)
                        return true;
                }
            }
            if (!top)
                return false;
            index = stack[--top];
        }
    }

    bool gi_direct_visible(const GiDirectOccluders& occluders, std::uint16_t model, DVector sample,
                           const GiDirectLight& light)
    {
        if (!light.clip)
            return true;
        if (model >= occluders.models.size())
            return false;
        const double dx = double(light.position.x) - sample.x, dy = double(light.position.y) - sample.y,
                     dz = double(light.position.z) - sample.z;
        const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance <= L::margin)
            return true;
        const double f = (distance - L::margin) / distance;
        const DVector end{float(sample.x + dx * f), float(sample.y + dy * f), float(sample.z + dz * f)};
        return !gi_segment_blocked(occluders.models[model], sample, end);
    }

    double gi_direct_visibility(const GiDirectOccluders& occluders, std::uint16_t model, DVector sample,
                                const GiDirectLight& light, double radius, std::uint32_t rays, double s1, double s2)
    {
        if (!light.clip)
            return 1;
        if (model >= occluders.models.size())
            return 0;
        auto hard = [&] { return gi_direct_visible(occluders, model, sample, light) ? 1.0 : 0.0; };
        const double dx = double(light.position.x) - sample.x, dy = double(light.position.y) - sample.y,
                     dz = double(light.position.z) - sample.z;
        const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!(radius > 0) || !rays || distance <= L::margin)
            return hard();
        // The disk faces the sample: an orthonormal basis around the light direction (Duff et al.).
        const double nx = dx / distance, ny = dy / distance, nz = dz / distance;
        const double sign = std::copysign(1.0, nz);
        const double c = -1 / (sign + nz), d = nx * ny * c;
        const double t[3]{1 + sign * nx * nx * c, sign * d, -sign * nx}, b[3]{d, sign + ny * ny * c, -ny};
        std::uint32_t counted = 0, reached = 0;
        for (std::uint32_t m = 0; m < rays; ++m)
        {
            double u1 = (m + .5) / rays + s1, u2 = m * 0.6180339887498949 + s2;
            u1 -= std::floor(u1);
            u2 -= std::floor(u2);
            double x = 0, y = 0;
            gi_concentric_disk(2 * u1 - 1, 2 * u2 - 1, x, y);
            const DVector target{float(light.position.x + radius * (x * t[0] + y * b[0])),
                                 float(light.position.y + radius * (x * t[1] + y * b[1])),
                                 float(light.position.z + radius * (x * t[2] + y * b[2]))};
            // Only the part of the disk the light's centre sees belongs to the light.
            if (gi_segment_blocked(occluders.models[model], light.position, target))
                continue;
            ++counted;
            const double ex = double(target.x) - sample.x, ey = double(target.y) - sample.y,
                         ez = double(target.z) - sample.z;
            const double length = std::sqrt(ex * ex + ey * ey + ez * ez);
            if (length <= L::margin)
            {
                ++reached;
                continue;
            }
            const double f = (length - L::margin) / length;
            const DVector end{float(sample.x + ex * f), float(sample.y + ey * f), float(sample.z + ez * f)};
            reached += !gi_segment_blocked(occluders.models[model], sample, end);
        }
        return counted ? double(reached) / counted : hard();
    }

    std::uint8_t GiDirectTexels::value(std::size_t t, int c) const
    {
        const double v = std::floor(double(light[t * 3 + c]) + .5);
        return std::uint8_t(std::clamp(v, 0.0, 255.0));
    }

    bool gi_direct_texels(const World& world, const GiReceivers& r, std::span<const GiDirectLight> lights,
                          const GiDirectOccluders& occluders, unsigned jobs, GiDirectTexels& out, std::string& error,
                          const std::atomic<bool>* cancel, const GiProgress& progress, const GiDirectOptions& options)
    {
        const auto start = std::chrono::steady_clock::now();
        error.clear();
        out = {};
        const std::uint32_t grid = r.texel_grid;
        const std::size_t per_texel = std::size_t(grid) * grid;
        if (r.samples.size() < r.texels || occluders.models.size() != world.models.size() || grid < 1 ||
            (grid > 1 && (r.texel_points.size() != r.texels * per_texel || r.texel_solid.size() != r.texels * per_texel)))
        {
            error = "Direct light: receivers or occluders do not match the world";
            return false;
        }
        const bool soft = options.shadow_radius > 0 && options.shadow_rays > 0;
        out.light.assign(r.texels * 3, 0.f);
        out.reaching.assign(r.texels, 0);
        out.visible.assign(r.texels, 0);
        out.placed.assign(r.texels, 0);
        const std::uint64_t chunks = (r.blocks.size() + L::chunk - 1) / L::chunk;
        std::vector<std::uint64_t> tests(chunks, 0);
        std::atomic<std::uint64_t> done{0};
        std::uint64_t reported = 0;
        std::mutex report_lock;
        const std::uint64_t total = r.blocks.size();
        const auto run = run_gi_jobs(
            chunks, jobs ? jobs : gi_default_jobs(), cancel,
                    [&](std::uint64_t chunk, unsigned)
                    {
                        std::vector<const GiDirectLight*> near;
                        std::vector<std::uint8_t> seen, lit; // Lot GQ1, per texel.
                        const std::size_t last = std::min<std::size_t>(r.blocks.size(), (chunk + 1) * L::chunk);
                        for (std::size_t b = chunk * L::chunk; b < last; ++b)
                        {
                            const auto& block = r.blocks[b];
                            const auto& frame = block.frame;
                            // The lights that can reach the block: its corner sphere against R.
                            DVector corners[4];
                            frame.position(0, 0, corners[0]);
                            frame.position(frame.width, 0, corners[1]);
                            frame.position(0, frame.height, corners[2]);
                            frame.position(frame.width, frame.height, corners[3]);
                            double centre[3]{};
                            for (const auto& p : corners)
                                centre[0] += p.x / 4.0, centre[1] += p.y / 4.0, centre[2] += p.z / 4.0;
                            double reach = 0;
                            for (const auto& p : corners)
                            {
                                const double x = p.x - centre[0], y = p.y - centre[1], z = p.z - centre[2];
                                reach = std::max(reach, std::sqrt(x * x + y * y + z * z));
                            }
                            near.clear();
                            for (const auto& light : lights)
                            {
                                const double x = light.position.x - centre[0], y = light.position.y - centre[1],
                                             z = light.position.z - centre[2];
                                if (std::sqrt(x * x + y * y + z * z) < light.radius + reach + 1)
                                    near.push_back(&light);
                            }
                            const DVector lift{block.normal.x * float(L::lift), block.normal.y * float(L::lift),
                                               block.normal.z * float(L::lift)};
                            for (unsigned j = 0; j < frame.height; ++j)
                                for (unsigned i = 0; i < frame.width; ++i)
                                {
                                    const std::size_t e = block.first + std::size_t(j) * frame.width + i;
                                    if (grid > 1 || soft)
                                    {
                                        // Lot GQ1: the texel's points (or its centre) with soft or hard shadows.
                                        double sum[3]{};
                                        std::uint64_t hash = 1469598103934665603ull;
                                        seen.assign(near.size(), 0);
                                        lit.assign(near.size(), 0);
                                        std::uint32_t used = 0;
                                        for (std::size_t k = 0; k < per_texel; ++k)
                                        {
                                            DVector term_point, at;
                                            if (grid > 1)
                                            {
                                                const auto slot = e * per_texel + k;
                                                if (r.texel_solid[slot])
                                                    continue;
                                                const auto offset = gi_texel_point_offset(std::uint32_t(k), grid);
                                                if (!frame.position(i + offset[0], j + offset[1], term_point))
                                                    continue;
                                                at = r.texel_points[slot];
                                            }
                                            else
                                            {
                                                if (!frame.position(i, j, term_point))
                                                    continue;
                                                at = r.samples[e];
                                            }
                                            const DVector sample{at.x + lift.x, at.y + lift.y, at.z + lift.z};
                                            ++used;
                                            for (std::size_t l = 0; l < near.size(); ++l)
                                            {
                                                const auto* light = near[l];
                                                const auto term = gi_direct_term(*light, term_point);
                                                if (term[0] == 0 && term[1] == 0 && term[2] == 0)
                                                    continue;
                                                seen[l] = 1;
                                                tests[chunk] += light->clip ? (soft ? options.shadow_rays : 1) : 0;
                                                double share = 1;
                                                if (soft)
                                                {
                                                    GiRandom random(options.seed, std::uint64_t(e) << 4 | k, light->object);
                                                    const double s1 = random.uniform(), s2 = random.uniform();
                                                    share = gi_direct_visibility(occluders, block.id.model, sample, *light,
                                                                                 options.shadow_radius,
                                                                                 options.shadow_rays, s1, s2);
                                                }
                                                else if (!gi_direct_visible(occluders, block.id.model, sample, *light))
                                                    share = 0;
                                                if (!(share > 0))
                                                    continue;
                                                lit[l] = 1;
                                                for (int c = 0; c < 3; ++c)
                                                    sum[c] += term[c] * share;
                                            }
                                        }
                                        if (!used)
                                            continue;
                                        unsigned reaching = 0;
                                        for (std::size_t l = 0; l < near.size(); ++l)
                                        {
                                            reaching += seen[l];
                                            if (lit[l])
                                                hash = (hash ^ near[l]->object) * 1099511628211ull;
                                        }
                                        for (int c = 0; c < 3; ++c)
                                            out.light[e * 3 + c] = float(double(c == 0   ? r.ambient.x
                                                                                : c == 1 ? r.ambient.y
                                                                                         : r.ambient.z) +
                                                                         sum[c] / used);
                                        out.reaching[e] = std::uint8_t(std::min(reaching, 255u));
                                        out.visible[e] = hash;
                                        out.placed[e] = 1;
                                        continue;
                                    }
                                    DVector centre_point;
                                    if (!frame.position(i, j, centre_point))
                                        continue;
                                    const auto& at = r.samples[e];
                                    const DVector sample{at.x + lift.x, at.y + lift.y, at.z + lift.z};
                                    double sum[3]{r.ambient.x, r.ambient.y, r.ambient.z};
                                    std::uint64_t hash = 1469598103934665603ull;
                                    unsigned reaching = 0;
                                    for (const auto* light : near)
                                    {
                                        const auto term = gi_direct_term(*light, centre_point);
                                        if (term[0] == 0 && term[1] == 0 && term[2] == 0)
                                            continue;
                                        ++reaching;
                                        tests[chunk] += light->clip;
                                        if (!gi_direct_visible(occluders, block.id.model, sample, *light))
                                            continue;
                                        hash = (hash ^ light->object) * 1099511628211ull;
                                        for (int c = 0; c < 3; ++c)
                                            sum[c] += term[c];
                                    }
                                    for (int c = 0; c < 3; ++c)
                                        out.light[e * 3 + c] = float(sum[c]);
                                    out.reaching[e] = std::uint8_t(std::min(reaching, 255u));
                                    out.visible[e] = hash;
                                    out.placed[e] = 1;
                                }
                        }
                        const std::uint64_t count = last - chunk * L::chunk;
                        const auto finished = done.fetch_add(count) + count;
                        if (progress && total && finished * 20 / total != (finished - count) * 20 / total)
                        {
                            std::lock_guard lock(report_lock);
                            if (finished > reported)
                            {
                                reported = finished;
                                progress("direct", finished, total);
                            }
                        }
                    });
        for (const auto t : tests)
            out.tests += t;
        out.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (run.cancelled || (cancel && cancel->load()))
        {
            out.cancelled = true;
            error = "cancelled";
            return false;
        }
        return true;
    }

    float gi_direct_value(float light, std::uint8_t original, float ambient)
    {
        const double o = original;
        const double a = ambient > 0 ? std::min(o, double(ambient)) : 0.0; // NaN and negative: no fill.
        const double t = light >= 0 ? std::min(double(light), 255.0) : 0.0;  // NaN: 0.
        return float(std::max(0.0, t - a) / 255);
    }

    bool write_gi_direct(const GiDirectSection& section, std::vector<std::uint8_t>& out, std::string& error)
    {
        out.clear();
        if (section.texels.size() > gi_lightmap_limits::max_texels)
        {
            error = "DIR: too many texels";
            return false;
        }
        auto put = [&](std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                out.push_back(std::uint8_t(v >> (8 * i)));
        };
        out.reserve(8 + 4 * section.texels.size());
        put(L::format);
        put(std::uint32_t(section.texels.size()));
        for (const auto v : section.texels)
            put(v);
        return true;
    }
    bool read_gi_direct(std::span<const std::uint8_t> payload, GiDirectSection& out, std::string& error)
    {
        auto get32 = [&](std::size_t at)
        {
            return std::uint32_t(payload[at]) | std::uint32_t(payload[at + 1]) << 8 |
                   std::uint32_t(payload[at + 2]) << 16 | std::uint32_t(payload[at + 3]) << 24;
        };
        if (payload.size() < 8 || get32(0) != L::format)
        {
            error = "DIR: unsupported payload format";
            return false;
        }
        const std::uint64_t count = get32(4);
        if (count > gi_lightmap_limits::max_texels || payload.size() != 8 + 4 * count)
        {
            error = "DIR: payload size does not match its count";
            return false;
        }
        GiDirectSection section;
        section.texels.resize(count);
        for (std::size_t i = 0; i < count; ++i)
            section.texels[i] = get32(8 + 4 * i);
        out = std::move(section);
        return true;
    }

    bool gi_rebake_direct(const World& world, const GiReceivers& r, unsigned jobs, const std::atomic<bool>* cancel,
                          const GiProgress& progress, GiDirectBake& out, std::string& error,
                          const GiDirectOptions& options)
    {
        const auto start = std::chrono::steady_clock::now();
        out = {};
        if (r.original.size() != r.elements() || r.solid.size() != r.elements())
        {
            error = "Direct light: invalid receivers";
            return false;
        }
        std::vector<GiDirectLight> lights;
        if (!gi_direct_lights(world, lights, out.lights, error))
            return false;
        GiDirectOccluders occluders;
        if (!occluders.build(world, error))
            return false;
        out.occluder_triangles = occluders.triangles;
        GiDirectTexels texels;
        if (!gi_direct_texels(world, r, lights, occluders, jobs, texels, error, cancel, progress, options))
        {
            out.cancelled = texels.cancelled;
            return false;
        }
        out.tests = texels.tests;
        const float ambient[3]{r.ambient.x, r.ambient.y, r.ambient.z};
        out.direct.resize(r.texels * 3);
        out.section.texels.resize(r.texels);
        for (std::size_t t = 0; t < r.texels; ++t)
        {
            // With texel points (lot GQ1) the points decide: a texel is kept only when none is placed.
            const bool keep = (r.texel_grid > 1 ? false : r.solid[t] != 0) || !texels.placed[t];
            out.kept += keep;
            float v[3];
            for (int c = 0; c < 3; ++c)
            {
                const auto o = r.original[t][c];
                const double a = ambient[c] > 0 ? std::min(double(o), double(ambient[c])) : 0.0;
                v[c] = keep ? float((o - a) / 255) : gi_direct_value(texels.light[t * 3 + c], o, ambient[c]);
            }
            out.saturated += !keep && (texels.light[t * 3] > 255 || texels.light[t * 3 + 1] > 255 ||
                                       texels.light[t * 3 + 2] > 255);
            out.section.texels[t] = gi_rgb9e5({v[0], v[1], v[2]});
            const auto decoded = gi_rgb9e5_decode(out.section.texels[t]);
            out.direct[t * 3] = decoded.x, out.direct[t * 3 + 1] = decoded.y, out.direct[t * 3 + 2] = decoded.z;
        }
        out.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return true;
    }

    void GiDirectFit::add(const std::array<std::uint8_t, 3>& rebaked, const std::array<std::uint8_t, 3>& original)
    {
        ++texels;
        bool ok = true, step = true;
        int signed_sum = 0, abs_total = 0;
        constexpr int steps[3]{8, 4, 8};
        for (int c = 0; c < 3; ++c)
        {
            const int d = int(rebaked[c]) - int(original[c]);
            ok = ok && std::abs(d) <= gi_direct_limits::tolerance[c];
            step = step && std::abs(d) <= steps[c];
            signed_sum += d;
            abs_total += std::abs(d);
        }
        fit += ok;
        fit_step += step;
        if (!ok)
            (signed_sum > 0 ? brighter : darker) += 1;
        abs_sum += abs_total / 3.0;
    }

    void gi_direct_audit(const World& world, const GiReceivers& r, const GiDirectTexels& d, GiDirectAudit& audit)
    {
        audit = {};
        std::map<std::uint32_t, GiDirectFit> flags;
        for (const auto& block : r.blocks)
        {
            const auto& model = world.models[block.id.model];
            const auto& polygon = model.polygons[block.id.polygon];
            const std::uint32_t surface_flags =
                polygon.surface < model.surfaces.size() ? model.surfaces[polygon.surface].flags : 0;
            const unsigned w = block.frame.width, h = block.frame.height;
            for (unsigned j = 0; j < h; ++j)
                for (unsigned i = 0; i < w; ++i)
                {
                    const std::size_t t = std::size_t(j) * w + i, e = block.first + t;
                    if (r.solid[e])
                    {
                        ++audit.solid_texels;
                        continue;
                    }
                    const std::array<std::uint8_t, 3> v{d.value(e, 0), d.value(e, 1), d.value(e, 2)};
                    const auto& o = r.original[e];
                    bool edge = false;
                    const int neighbours[4][2]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& n : neighbours)
                    {
                        const int x = int(i) + n[0], y = int(j) + n[1];
                        if (x >= 0 && y >= 0 && x < int(w) && y < int(h) &&
                            d.visible[block.first + std::size_t(y) * w + std::size_t(x)] != d.visible[e])
                            edge = true;
                    }
                    audit.all.add(v, o);
                    (edge ? audit.edge : audit.interior).add(v, o);
                    (block.id.model == 0 ? audit.primary : audit.world_models).add(v, o);
                    const unsigned n = d.reaching[e];
                    audit.by_lights[n == 0 ? 0 : n == 1 ? 1 : n < 4 ? 2 : 3].add(v, o);
                    flags[surface_flags].add(v, o);
                }
        }
        audit.by_flags.assign(flags.begin(), flags.end());
    }
} // namespace OpenBlood2
