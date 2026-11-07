#include "engine/gi_preview.h"
#include "engine/collision.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        bool same_name(std::string_view a, std::string_view b)
        {
            return !CaseLess{}(a, b) && !CaseLess{}(b, a);
        }
        DVector unit(DVector v)
        {
            const double length = std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z);
            return length > 0 ? DVector{float(v.x / length), float(v.y / length), float(v.z / length)} : DVector{};
        }
        DVector cross(DVector a, DVector b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }
        // Turns a direction about +Y (yaw) by the given angle in degrees.
        DVector yawed(DVector v, double degrees)
        {
            const double a = degrees * 3.14159265358979323846 / 180, c = std::cos(a), s = std::sin(a);
            return {float(c * v.x + s * v.z), v.y, float(-s * v.x + c * v.z)};
        }
    } // namespace

    std::vector<GiPreviewView> gi_preview_views(const World& world)
    {
        std::vector<GiPreviewView> views;
        std::size_t index = 0;
        for (const auto& object : world.objects)
        {
            if (!same_name(object.class_name, "GameStartPoint") || views.size() >= 6)
                continue;
            const auto* pos = object.find("Pos");
            const auto* position = pos ? std::get_if<DVector>(pos) : nullptr;
            if (!position || !physics_vector(*position))
                continue;
            DRotation rotation{};
            if (const auto* value = object.find("Rotation"))
            {
                if (const auto* euler = std::get_if<EulerAngles>(value))
                    rotation = rotation_from_euler(euler->radians);
                else if (const auto* q = std::get_if<DRotation>(value))
                    rotation = *q;
            }
            const auto forward = unit(rotate_vector({0, 0, 1}, rotation));
            const DVector eye{position->x, position->y + 36, position->z};
            const auto label = "start" + std::to_string(index++);
            if (views.empty())
            {
                views.push_back({eye, forward, label});
                views.push_back({eye, unit(yawed(forward, 120)), label + "+120"});
                views.push_back({eye, unit(yawed(forward, 240)), label + "+240"});
            }
            else
                views.push_back({eye, forward, label});
        }
        return views;
    }

    GiPreviewValues gi_preview_values(const GiReceivers& r, const GiIndirect& indirect, float k, float f)
    {
        GiPreviewValues values;
        const auto n = r.elements();
        values.original.resize(n * 3);
        values.composed.resize(n * 3);
        values.indirect.resize(n * 3);
        const float ambient[3]{r.ambient.x, r.ambient.y, r.ambient.z};
        for (std::size_t e = 0; e < n; ++e)
            for (int c = 0; c < 3; ++c)
            {
                const auto o = r.original[e][c];
                const float energy = indirect.energy.size() == n * 3 ? indirect.energy[e * 3 + c] : 0.f;
                values.original[e * 3 + c] = o / 255.f;
                values.composed[e * 3 + c] = gi_compose(o, ambient[c], energy, k, f, e < r.texels) / 255.f;
                values.indirect[e * 3 + c] = gi_preview_indirect_exposure * k * energy;
            }
        return values;
    }

    bool gi_indirect_from_sections(const GiReceivers& r, const GiLightmaps& maps, const GiVertexLight& vertex,
                                   GiIndirect& out, std::string& error)
    {
        out = {};
        bool match = maps.blocks.size() == r.blocks.size() && vertex.sets.size() == r.sets.size() &&
                     maps.texels.size() == r.texels && vertex.corners.size() == r.corners;
        for (std::size_t b = 0; match && b < r.blocks.size(); ++b)
        {
            const auto& x = maps.blocks[b];
            const auto& y = r.blocks[b].id;
            match = x.model == y.model && x.polygon == y.polygon && x.width == y.width && x.height == y.height;
        }
        for (std::size_t s = 0; match && s < r.sets.size(); ++s)
        {
            const auto& x = vertex.sets[s];
            const auto& y = r.sets[s].id;
            match = x.model == y.model && x.polygon == y.polygon && x.corners == y.corners;
        }
        if (!match)
        {
            error = "The LMAP/VTX sections do not match this world's receivers";
            return false;
        }
        out.energy.resize(r.elements() * 3);
        out.valid.resize(r.elements());
        for (std::size_t e = 0; e < r.elements(); ++e)
        {
            const bool texel = e < r.texels;
            const auto value = gi_rgb9e5_decode(texel ? maps.texels[e] : vertex.corners[e - r.texels]);
            out.energy[e * 3] = value.x, out.energy[e * 3 + 1] = value.y, out.energy[e * 3 + 2] = value.z;
            out.valid[e] = texel ? maps.valid[e] : vertex.valid[e - r.texels];
        }
        return true;
    }

    void render_gi_preview(const GiBakeScene& scene, const GiSky& sky, const GiReceivers& r,
                           const GiPreviewValues& values, const GiPreviewView& view, unsigned width, unsigned height,
                           unsigned jobs, std::vector<std::uint8_t>& rgba)
    {
        rgba.assign(std::size_t(width) * 3 * height * 4, 255);
        const auto forward = unit(view.forward);
        auto right = unit(cross({0, 1, 0}, forward));
        if (!(right.x != 0 || right.y != 0 || right.z != 0))
            right = {1, 0, 0};
        const auto up = cross(forward, right);
        const double aspect = double(height) / width;
        const std::span<const float> panels[3]{values.original, values.composed, values.indirect};
        // What a face shows in one panel.
        auto shade = [&](int panel, std::uint32_t face, float u, float v, DVector direction, double out[3])
        {
            const auto& link = r.links[face];
            const auto albedo = scene.albedo(face, u, v);
            DVector light{};
            switch (link.kind)
            {
            case GiReceivers::Link::Lightmap:
            case GiReceivers::Link::Vertex:
            {
                gi_receiver_sample(r, panels[panel], face, u, v, light);
                if (panel < 2 && link.kind == GiReceivers::Link::Lightmap)
                    light = {world_saturate_scale * std::min(light.x, 1.f),
                             world_saturate_scale * std::min(light.y, 1.f),
                             world_saturate_scale * std::min(light.z, 1.f)};
                break;
            }
            case GiReceivers::Link::Bright:
                light = panel < 2 ? DVector{1, 1, 1} : DVector{};
                break;
            case GiReceivers::Link::Sky:
            {
                const auto s = sky.displayed(direction);
                out[0] = s.x, out[1] = s.y, out[2] = s.z;
                return;
            }
            default:
                // Untextured: the drawn vertex colour over white.
                light = panel < 2 ? scene.baked_light(face, u, v) : DVector{};
                out[0] = light.x, out[1] = light.y, out[2] = light.z;
                return;
            }
            out[0] = albedo[0] * light.x, out[1] = albedo[1] * light.y, out[2] = albedo[2] * light.z;
        };
        run_gi_jobs(height, jobs ? jobs : gi_default_jobs(), nullptr,
                    [&](std::uint64_t y, unsigned)
                    {
                        GiHit hit;
                        GiTraceStats stats;
                        for (unsigned x = 0; x < width; ++x)
                        {
                            double sum[3][3]{};
                            for (int sy = 0; sy < 2; ++sy)
                                for (int sx = 0; sx < 2; ++sx)
                                {
                                    const double px = (2 * (x + .25 + .5 * sx) / width - 1);
                                    const double py = (1 - 2 * (y + .25 + .5 * sy) / height) * aspect;
                                    const auto direction = unit({float(forward.x + px * right.x + py * up.x),
                                                                 float(forward.y + px * right.y + py * up.y),
                                                                 float(forward.z + px * right.z + py * up.z)});
                                    scene.trace(view.eye, direction, FLT_MAX, hit, stats);
                                    for (int panel = 0; panel < 3; ++panel)
                                    {
                                        double color[3]{}, value[3], transmission = 1;
                                        for (std::uint32_t i = 0; i < hit.layer_count; ++i)
                                        {
                                            const auto& layer = hit.layers[i];
                                            shade(panel, layer.face, layer.u, layer.v, direction, value);
                                            for (int c = 0; c < 3; ++c)
                                                color[c] += layer.alpha * transmission * value[c];
                                            transmission *= 1 - layer.alpha;
                                        }
                                        if (hit.kind == GiHit::Kind::Miss || hit.kind == GiHit::Kind::Sky)
                                        {
                                            const auto s = sky.displayed(direction);
                                            value[0] = s.x, value[1] = s.y, value[2] = s.z;
                                        }
                                        else
                                            shade(panel, hit.face, hit.u, hit.v, direction, value);
                                        for (int c = 0; c < 3; ++c)
                                            sum[panel][c] += color[c] + transmission * value[c];
                                    }
                                }
                            for (int panel = 0; panel < 3; ++panel)
                            {
                                const auto at = (std::size_t(y) * width * 3 + std::size_t(panel) * width + x) * 4;
                                auto* pixel = &rgba[at];
                                for (int c = 0; c < 3; ++c)
                                    pixel[c] = std::uint8_t(std::lround(std::clamp(sum[panel][c] / 4, 0.0, 1.0) * 255));
                                pixel[3] = 255;
                            }
                        }
                    });
    }
} // namespace OpenBlood2
