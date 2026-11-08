#include "engine/gi_sky.h"
#include "engine/collision.h"
#include "engine/gi_jobs.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>

namespace OpenBlood2
{
    namespace
    {
        bool same_name(std::string_view a, std::string_view b)
        {
            return !CaseLess{}(a, b) && !CaseLess{}(b, a);
        }
        // GetPropGeneric's numeric views of a DAT property: integers and reals convert, a bool is 0/1.
        bool number(const WorldObject& object, std::string_view name, double& out)
        {
            const auto* value = object.find(name);
            if (!value)
                return false;
            if (const auto* f = std::get_if<float>(value))
                out = *f;
            else if (const auto* i = std::get_if<std::int32_t>(value))
                out = *i;
            else if (const auto* b = std::get_if<bool>(value))
                out = *b;
            else
                return false;
            return true;
        }
        DVector vector_property(const WorldObject& object, std::string_view name, DVector fallback)
        {
            const auto* value = object.find(name);
            const auto* v = value ? std::get_if<DVector>(value) : nullptr;
            return v ? *v : fallback;
        }
        std::string string_property(const WorldObject& object, std::string_view name)
        {
            const auto* value = object.find(name);
            const auto* s = value ? std::get_if<std::string>(value) : nullptr;
            return s ? *s : std::string{};
        }
        // BaseClass precreate: Flags, then Visible overrides FLAG_VISIBLE. Every class that can hold a
        // sky slot defaults to visible (DemoSkyWorldModel Flags 1, the movers and brushes Visible).
        bool visible(const WorldObject& object)
        {
            double value = 0;
            if (const auto* v = object.find("Visible"); v && std::get_if<bool>(v))
                return *std::get_if<bool>(v);
            if (number(object, "Flags", value) && std::isfinite(value))
                return (static_cast<std::int64_t>(value) & 1) != 0;
            return true;
        }
        // The classes whose objects are bound world models (OT_WORLDMODEL), the only non-sprite
        // objects AddSkyObject accepts.
        bool world_model_class(std::string_view name)
        {
            for (std::string_view entry : {"DemoSkyWorldModel", "Door", "RotatingDoor", "RotatingBrush",
                                           "CDestructableBrush", "WorldModelDebris"})
                if (same_name(name, entry))
                    return true;
            return false;
        }
        struct SkyObject
        {
            std::size_t object = 0;
            bool pointer = false;
            std::int32_t index = 0;
            DVector dims{}, inner{.1f, .1f, .1f}, position{};
            std::string target;
        };
        // MID_PRECREATE (world file): the properties and the original refusal.
        bool read_sky_object(const World& world, std::size_t index, SkyObject& out)
        {
            const auto& object = world.objects[index];
            out = {};
            out.object = index;
            out.pointer = same_name(object.class_name, "SkyPointer");
            if (object.invalid_editor_position)
                return false; // An uncreated runtime entry.
            double value = 0;
            if (number(object, "Index", value))
            {
                if (!std::isfinite(value) || value <= -1 || value >= 30)
                    return false;
                out.index = static_cast<std::int32_t>(value);
            }
            out.dims = vector_property(object, "SkyDims", {});
            float* inner[3]{&out.inner.x, &out.inner.y, &out.inner.z};
            const char* names[3]{"InnerPercentX", "InnerPercentY", "InnerPercentZ"};
            for (int k = 0; k < 3; ++k)
                if (number(object, names[k], value))
                    *inner[k] = static_cast<float>(value);
            out.position = vector_property(object, "Pos", {});
            if (out.pointer)
                out.target = string_property(object, "SkyObjectName");
            return out.index >= 0 && out.index < 30 && physics_vector(out.dims) && physics_vector(out.inner) &&
                   out.dims.x >= 0 && out.dims.y >= 0 && out.dims.z >= 0 && out.inner.x >= 0 &&
                   out.inner.y >= 0 && out.inner.z >= 0;
        }
        struct Paint
        {
            double v0[3], e1[3], e2[3], normal[3];
            std::uint32_t face = 0;
            bool translucent = false;
        };
        constexpr double sky_near = .01, sky_far = 65536; // The sky pass projection planes.
    } // namespace

    GiSkySetup gi_sky_setup(const World& world)
    {
        GiSkySetup setup;
        std::map<std::string, std::size_t, CaseLess> first; // First model per name (object binding).
        for (std::size_t i = 1; i < world.models.size(); ++i)
            first.try_emplace(world.models[i].name, i);
        auto model_of = [&](const WorldObject& object) -> std::int32_t
        {
            const auto name = string_property(object, "Name");
            const auto found = name.empty() ? first.end() : first.find(name);
            return found == first.end() ? -1 : static_cast<std::int32_t>(found->second);
        };
        std::vector<SkyObject> created;
        for (std::size_t i = 0; i < world.objects.size(); ++i)
        {
            const auto& object = world.objects[i];
            if (!same_name(object.class_name, "DemoSkyWorldModel") && !same_name(object.class_name, "SkyPointer"))
                continue;
            SkyObject sky;
            if (read_sky_object(world, i, sky))
                created.push_back(std::move(sky));
        }
        // MID_INITIALUPDATE in creation order: definitions, then DemoSkyWorldModel additions. An object
        // whose definition is rejected removes itself (its addition, if any, goes with it).
        std::vector<std::size_t> registered;         // DAT objects in the sky (INSKY).
        std::array<std::int64_t, gi_sky_limits::slots> holder; // DAT object per slot.
        holder.fill(-1);
        std::vector<const SkyObject*> pointers;
        for (const auto& sky : created)
        {
            if (sky.dims.x != 0 && sky.dims.y != 0 && sky.dims.z != 0)
            {
                const DVector v{sky.dims.x * sky.inner.x, sky.dims.y * sky.inner.y, sky.dims.z * sky.inner.z};
                const SkyDefinition d{vector_subtract(sky.position, sky.dims), vector_add(sky.position, sky.dims),
                                      vector_subtract(sky.position, v), vector_add(sky.position, v)};
                if (!valid_sky_definition(d))
                    continue;
                setup.definition = d;
                setup.defined = true;
            }
            if (sky.pointer)
            {
                pointers.push_back(&sky);
                continue;
            }
            if (model_of(world.objects[sky.object]) < 0)
                continue; // Unbound: AddSkyObject refuses it.
            registered.push_back(sky.object);
            holder[sky.index] = static_cast<std::int64_t>(sky.object);
        }
        // First MID_UPDATE of each SkyPointer, in creation order: the first object with the exact
        // Name (FindNamedObjects compares case-sensitively). An object already in the sky keeps its slot.
        for (const auto* sky : pointers)
        {
            ++setup.pointers;
            std::int64_t target = -1;
            for (std::size_t i = 0; i < world.objects.size() && target < 0 && !sky->target.empty(); ++i)
                if (!world.objects[i].invalid_editor_position &&
                    string_property(world.objects[i], "Name") == sky->target)
                    target = static_cast<std::int64_t>(i);
            if (target < 0 || !world_model_class(world.objects[target].class_name) ||
                model_of(world.objects[target]) < 0)
            {
                ++setup.unresolved_pointers;
                continue;
            }
            if (std::find(registered.begin(), registered.end(), std::size_t(target)) != registered.end())
                continue;
            registered.push_back(std::size_t(target));
            holder[sky->index] = target;
        }
        for (std::size_t s = 0; s < gi_sky_limits::slots; ++s)
            if (holder[s] >= 0)
            {
                setup.slots[s] = model_of(world.objects[holder[s]]);
                setup.visible[s] = visible(world.objects[holder[s]]);
            }
        for (auto object : registered)
            setup.models.push_back(static_cast<std::uint32_t>(model_of(world.objects[object])));
        std::sort(setup.models.begin(), setup.models.end());
        setup.models.erase(std::unique(setup.models.begin(), setup.models.end()), setup.models.end());
        // WorldProperties (registration defaults): the last object's values reach the console.
        for (const auto& object : world.objects)
        {
            if (!same_name(object.class_name, "WorldProperties"))
                continue;
            double value = 0;
            auto read = [&](const char* name, double fallback)
            { return number(object, name, value) && std::isfinite(value) ? value : fallback; };
            const bool fog = std::trunc(read("FogEnable", 0)) != 0;
            const double fog_near = std::trunc(read("FogNearZ", 1)), fog_far = std::trunc(read("FogFarZ", 5000));
            const DVector color = vector_property(object, "FogColor", {127, 127, 127});
            const bool sky_fog = std::trunc(read("SkyFog", 0)) != 0;
            const double start = std::trunc(read("SkyFogNearZ", 100)), end = std::trunc(read("SkyFogFarZ", 1000));
            setup.fog = {};
            if (fog && fog_far > fog_near && physics_vector(color) && sky_fog && end > start)
            {
                auto channel = [](float c) { return std::clamp(std::trunc(c), 0.f, 255.f) / 255.f; };
                setup.fog = {true, {channel(color.x), channel(color.y), channel(color.z)}, float(start), float(end)};
            }
        }
        return setup;
    }

    DVector gi_cube_direction(unsigned face, float x, float y)
    {
        const unsigned a = face / 2 % 3;
        float d[3];
        d[a] = face & 1 ? -1.f : 1.f;
        d[(a + 1) % 3] = 2 * x / GiSky::size - 1;
        d[(a + 2) % 3] = 2 * y / GiSky::size - 1;
        return {d[0], d[1], d[2]};
    }

    bool GiSky::build(const World& world, const GiBakeScene& scene, const GiSkySetup& setup, float scale,
                      unsigned jobs, GiSkyReport& report, std::string& error)
    {
        const auto start = std::chrono::steady_clock::now();
        texels_.clear();
        scale_ = scale;
        report = {};
        if (!std::isfinite(scale) || scale < 0)
        {
            error = "Invalid GI sky scale";
            return false;
        }
        // Like the sky pass, the projection is needed (and must be valid) only when a visible object
        // holds a slot; otherwise the sky draws nothing.
        bool visible = false;
        for (std::size_t s = 0; s < gi_sky_limits::slots; ++s)
            visible = visible || (setup.slots[s] >= 0 && setup.visible[s]);
        const DVector centre{(world.min.x + world.max.x) / 2, (world.min.y + world.max.y) / 2,
                             (world.min.z + world.max.z) / 2};
        DVector eye{};
        if (visible && !sky_view_position(setup.definition, world.min, world.max, centre, eye))
        {
            error = "Invalid GI sky definition or world extents";
            return false;
        }
        report.eye = eye;
        // Faces per range in face order, which is the range's submission order.
        const auto faces = scene.faces();
        const auto& mesh = scene.mesh();
        std::map<std::uint32_t, std::vector<std::uint32_t>> by_range;
        for (std::size_t s = 0; s < gi_sky_limits::slots; ++s)
            if (setup.slots[s] >= 0)
                by_range.try_emplace(static_cast<std::uint32_t>(setup.slots[s]));
        for (std::uint32_t f = 0; f < faces.size() && !by_range.empty(); ++f)
            if (const auto found = by_range.find(faces[f].range); found != by_range.end())
                found->second.push_back(f);
        std::vector<Paint> paint;
        for (std::size_t s = 0; s < gi_sky_limits::slots; ++s)
        {
            if (setup.slots[s] < 0 || !setup.visible[s])
                continue;
            const auto found = by_range.find(static_cast<std::uint32_t>(setup.slots[s]));
            if (found == by_range.end() || found->second.empty())
                continue;
            ++report.slots;
            for (const auto f : found->second)
            {
                if (paint.size() >= gi_sky_limits::max_triangles)
                {
                    error = "GI sky exceeds " + std::to_string(gi_sky_limits::max_triangles) + " triangles";
                    return false;
                }
                const auto& face = faces[f];
                const auto& pivot = mesh.ranges[face.range].pivot;
                DVector p[3];
                for (int k = 0; k < 3; ++k)
                    p[k] = vector_add(mesh.vertices[std::size_t(face.triangle) * 3 + k].position, pivot);
                Paint t;
                for (int k = 0; k < 3; ++k)
                {
                    const double a[3]{p[0].x, p[0].y, p[0].z}, b[3]{p[1].x, p[1].y, p[1].z},
                        c[3]{p[2].x, p[2].y, p[2].z}, n[3]{face.normal.x, face.normal.y, face.normal.z};
                    t.v0[k] = a[k];
                    t.e1[k] = b[k] - a[k];
                    t.e2[k] = c[k] - a[k];
                    t.normal[k] = n[k];
                }
                t.face = f;
                t.translucent = (face.flags & GiBakeFace::Translucent) != 0;
                report.translucent += t.translucent;
                paint.push_back(t);
            }
        }
        report.triangles = paint.size();
        constexpr unsigned n = size;
        std::vector<float> coverage(std::size_t(6) * n * n, 0.f);
        texels_.assign(std::size_t(6) * n * n * 3, 0.f);
        const double origin[3]{eye.x, eye.y, eye.z};
        const auto& fog = setup.fog;
        auto sample = [&](const double d[3], double color[3], double& weight)
        {
            color[0] = color[1] = color[2] = 0;
            weight = 0;
            for (const auto& t : paint)
            {
                if (!(d[0] * t.normal[0] + d[1] * t.normal[1] + d[2] * t.normal[2] < 0))
                    continue; // Back faces are culled.
                const double p[3]{d[1] * t.e2[2] - d[2] * t.e2[1], d[2] * t.e2[0] - d[0] * t.e2[2],
                                  d[0] * t.e2[1] - d[1] * t.e2[0]};
                const double det = t.e1[0] * p[0] + t.e1[1] * p[1] + t.e1[2] * p[2];
                if (!(std::fabs(det) > 1e-12))
                    continue;
                const double inv = 1 / det;
                const double s[3]{origin[0] - t.v0[0], origin[1] - t.v0[1], origin[2] - t.v0[2]};
                const double u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
                if (!(u >= 0 && u <= 1))
                    continue;
                const double q[3]{s[1] * t.e1[2] - s[2] * t.e1[1], s[2] * t.e1[0] - s[0] * t.e1[2],
                                  s[0] * t.e1[1] - s[1] * t.e1[0]};
                const double v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
                if (!(v >= 0 && u + v <= 1))
                    continue;
                const double distance = (t.e2[0] * q[0] + t.e2[1] * q[1] + t.e2[2] * q[2]) * inv;
                if (!(distance > sky_near && distance <= sky_far))
                    continue;
                const auto albedo = scene.albedo(t.face, float(u), float(v));
                const auto light = scene.baked_light(t.face, float(u), float(v));
                const bool textured = faces[t.face].material != 0;
                double l[3]{(textured ? albedo[0] : 1.f) * light.x, (textured ? albedo[1] : 1.f) * light.y,
                            (textured ? albedo[2] : 1.f) * light.z};
                if (fog.enabled)
                {
                    const double a = std::clamp((distance - fog.start) / (double(fog.end) - fog.start), 0.0, 1.0);
                    l[0] += (fog.color.x - l[0]) * a;
                    l[1] += (fog.color.y - l[1]) * a;
                    l[2] += (fog.color.z - l[2]) * a;
                }
                if (t.translucent)
                {
                    const double alpha = albedo[3];
                    for (int c = 0; c < 3; ++c)
                        color[c] = alpha * l[c] + (1 - alpha) * color[c];
                    weight = alpha + (1 - alpha) * weight;
                }
                else
                {
                    color[0] = l[0], color[1] = l[1], color[2] = l[2];
                    weight = 1;
                }
            }
        };
        run_gi_jobs(std::uint64_t(6) * n, jobs, nullptr,
                    [&](std::uint64_t row, unsigned)
                    {
                        const auto face = unsigned(row / n), y = unsigned(row % n);
                        for (unsigned x = 0; x < n; ++x)
                        {
                            double sum[3]{}, covered = 0;
                            for (int sy = 0; sy < 2; ++sy)
                                for (int sx = 0; sx < 2; ++sx)
                                {
                                    const auto r = gi_cube_direction(face, x + .25f + .5f * sx, y + .25f + .5f * sy);
                                    const double length = std::sqrt(double(r.x) * r.x + double(r.y) * r.y +
                                                                    double(r.z) * r.z);
                                    const double d[3]{r.x / length, r.y / length, r.z / length};
                                    double color[3], weight;
                                    sample(d, color, weight);
                                    for (int c = 0; c < 3; ++c)
                                        sum[c] += color[c];
                                    covered += weight;
                                }
                            const auto texel = (std::size_t(face) * n + y) * n + x;
                            for (int c = 0; c < 3; ++c)
                                texels_[texel * 3 + c] = float(sum[c] / 4);
                            coverage[texel] = float(covered / 4);
                        }
                    });
        // Fallback: the solid-angle weighted average of the covered colour, in texel order.
        double total[3]{}, weight = 0, area = 0;
        for (unsigned face = 0; face < 6; ++face)
            for (unsigned y = 0; y < n; ++y)
                for (unsigned x = 0; x < n; ++x)
                {
                    const double u = 2 * (x + .5) / n - 1, v = 2 * (y + .5) / n - 1;
                    const double r2 = 1 + u * u + v * v;
                    const double omega = 1 / (r2 * std::sqrt(r2));
                    const auto texel = (std::size_t(face) * n + y) * n + x;
                    for (int c = 0; c < 3; ++c)
                        total[c] += texels_[texel * 3 + c] * omega;
                    weight += coverage[texel] * omega;
                    area += omega;
                }
        DVector fallback{};
        if (weight > 0)
            fallback = {float(total[0] / weight), float(total[1] / weight), float(total[2] / weight)};
        const float f[3]{fallback.x, fallback.y, fallback.z};
        for (std::size_t texel = 0; texel < coverage.size(); ++texel)
            for (int c = 0; c < 3; ++c)
                texels_[texel * 3 + c] += (1 - coverage[texel]) * f[c];
        report.average = fallback;
        report.covered = area > 0 ? weight / area : 0;
        report.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return true;
    }

    void GiSky::fill(DVector displayed, float scale)
    {
        texels_.resize(std::size_t(6) * size * size * 3);
        for (std::size_t i = 0; i < texels_.size(); i += 3)
            texels_[i] = displayed.x, texels_[i + 1] = displayed.y, texels_[i + 2] = displayed.z;
        scale_ = scale;
    }
    DVector GiSky::displayed(DVector direction) const
    {
        if (texels_.empty())
            return {};
        const double c[3]{direction.x, direction.y, direction.z};
        unsigned a = 0;
        double major = std::fabs(c[0]);
        for (unsigned k = 1; k < 3; ++k)
            if (std::fabs(c[k]) > major) // Ties keep the lower axis.
                a = k, major = std::fabs(c[k]);
        if (!(major > 0) || !std::isfinite(major))
            return {};
        const unsigned face = 2 * a + (c[a] < 0);
        const double u = c[(a + 1) % 3] / major, v = c[(a + 2) % 3] / major;
        const double x = std::clamp((u + 1) * .5 * size - .5, 0.0, double(size - 1));
        const double y = std::clamp((v + 1) * .5 * size - .5, 0.0, double(size - 1));
        const unsigned x0 = unsigned(x), y0 = unsigned(y);
        const unsigned x1 = std::min(x0 + 1, size - 1), y1 = std::min(y0 + 1, size - 1);
        const double fx = x - x0, fy = y - y0;
        auto at = [&](unsigned i, unsigned j, int channel)
        { return double(texels_[((std::size_t(face) * size + j) * size + i) * 3 + channel]); };
        float out[3];
        for (int ch = 0; ch < 3; ++ch)
            out[ch] = float((1 - fy) * ((1 - fx) * at(x0, y0, ch) + fx * at(x1, y0, ch)) +
                            fy * ((1 - fx) * at(x0, y1, ch) + fx * at(x1, y1, ch)));
        return {out[0], out[1], out[2]};
    }
    DVector GiSky::radiance(DVector direction) const
    {
        const auto d = displayed(direction);
        return {d.x * scale_, d.y * scale_, d.z * scale_};
    }
} // namespace OpenBlood2
