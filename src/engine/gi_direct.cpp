#include "engine/gi_direct.h"
#include "engine/gi_jobs.h"
#include "engine/properties.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>

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
        models.assign(world.models.size(), CollisionWorld{});
        for (std::size_t m = 0; m < world.models.size(); ++m)
        {
            // SURF_INVISIBLE faces cast no shadow: a copy without their SURF_SOLID bit.
            const auto& source = world.models[m];
            const bool invisible = std::any_of(source.surfaces.begin(), source.surfaces.end(),
                                               [](const Surface& s) { return (s.flags & 5) == 5; });
            WorldModel copy;
            if (invisible)
            {
                copy = source;
                for (auto& s : copy.surfaces)
                    if (s.flags & 4)
                        s.flags &= ~1u;
            }
            if (!models[m].build(invisible ? copy : source, error))
            {
                error = "Direct-light occluders of model " + std::to_string(m) + ": " + error;
                models.clear();
                return false;
            }
        }
        return true;
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
        CollisionHit hit;
        return occluders.models[model].sweep(sample, end, {0, 0, 0}, hit) != CollisionQuery::Hit;
    }

    std::uint8_t GiDirectTexels::value(std::size_t t, int c) const
    {
        const double v = std::floor(double(light[t * 3 + c]) + .5);
        return std::uint8_t(std::clamp(v, 0.0, 255.0));
    }

    bool gi_direct_texels(const World& world, const GiReceivers& r, std::span<const GiDirectLight> lights,
                          const GiDirectOccluders& occluders, unsigned jobs, GiDirectTexels& out, std::string& error)
    {
        const auto start = std::chrono::steady_clock::now();
        error.clear();
        out = {};
        if (r.samples.size() < r.texels || occluders.models.size() != world.models.size())
        {
            error = "Direct light: receivers or occluders do not match the world";
            return false;
        }
        out.light.assign(r.texels * 3, 0.f);
        out.reaching.assign(r.texels, 0);
        out.visible.assign(r.texels, 0);
        const std::uint64_t chunks = (r.blocks.size() + L::chunk - 1) / L::chunk;
        std::vector<std::uint64_t> tests(chunks, 0);
        run_gi_jobs(chunks, jobs ? jobs : gi_default_jobs(), nullptr,
                    [&](std::uint64_t chunk, unsigned)
                    {
                        std::vector<const GiDirectLight*> near;
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
                            const DVector lift = vector_scale(block.normal, float(L::lift));
                            for (unsigned j = 0; j < frame.height; ++j)
                                for (unsigned i = 0; i < frame.width; ++i)
                                {
                                    const std::size_t e = block.first + std::size_t(j) * frame.width + i;
                                    DVector centre_point;
                                    if (!frame.position(i, j, centre_point))
                                        continue;
                                    const DVector sample = vector_add(r.samples[e], lift);
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
                                }
                        }
                    });
        for (const auto t : tests)
            out.tests += t;
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
