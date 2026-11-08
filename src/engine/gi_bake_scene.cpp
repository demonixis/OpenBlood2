#include "engine/gi_bake_scene.h"
#include "engine/gi_sky.h"
#include "engine/collision.h"
#include "engine/surface_materials.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <map>

namespace OpenBlood2
{
    namespace L = gi_bake_limits;
    namespace
    {
        struct Box
        {
            float min[3]{FLT_MAX, FLT_MAX, FLT_MAX}, max[3]{-FLT_MAX, -FLT_MAX, -FLT_MAX};
            void grow(const Box& b)
            {
                for (int i = 0; i < 3; ++i)
                {
                    min[i] = std::min(min[i], b.min[i]);
                    max[i] = std::max(max[i], b.max[i]);
                }
            }
            void grow(const float p[3])
            {
                for (int i = 0; i < 3; ++i)
                {
                    min[i] = std::min(min[i], p[i]);
                    max[i] = std::max(max[i], p[i]);
                }
            }
            double area() const
            {
                if (min[0] > max[0])
                    return 0;
                const double x = double(max[0]) - min[0], y = double(max[1]) - min[1], z = double(max[2]) - min[2];
                return x * y + y * z + z * x;
            }
        };
        struct Item
        {
            Box box;
            float centroid[3];
            std::uint32_t triangle; // Index into the input triangles.
        };
        std::uint32_t bin(const Item& item, int axis, float lo, float scale)
        {
            const auto b = static_cast<std::uint32_t>((item.centroid[axis] - lo) * scale);
            return std::min(b, L::sah_bins - 1);
        }
        struct Builder
        {
            std::vector<Item>& items;
            std::vector<GiBvh::Node>& nodes;
            std::uint32_t depth = 0;
            bool ok = true;
            void leaf(std::uint32_t index, std::uint32_t begin, std::uint32_t end)
            {
                if (end - begin > UINT16_MAX)
                    ok = false;
                nodes[index].index = begin;
                nodes[index].count = static_cast<std::uint16_t>(end - begin);
                nodes[index].axis = 0;
            }
            void build(std::uint32_t index, std::uint32_t begin, std::uint32_t end, std::uint32_t level)
            {
                depth = std::max(depth, level);
                Box bounds, centroids;
                for (auto i = begin; i < end; ++i)
                {
                    bounds.grow(items[i].box);
                    centroids.grow(items[i].centroid);
                }
                for (int a = 0; a < 3; ++a)
                {
                    nodes[index].min[a] = bounds.min[a];
                    nodes[index].max[a] = bounds.max[a];
                }
                const auto count = end - begin;
                if (count <= L::leaf_size || level >= L::max_depth)
                    return leaf(index, begin, end);
                int axis = -1;
                std::uint32_t middle = begin;
                if (level < L::sah_depth)
                {
                    // Binned SAH over the centroid bounds; cost in triangle tests, one per node visit.
                    double best = double(count) * bounds.area();
                    std::uint32_t split = 0; // First bin of the second child.
                    for (int a = 0; a < 3; ++a)
                    {
                        const float lo = centroids.min[a], extent = centroids.max[a] - lo;
                        if (!(extent > 0))
                            continue;
                        Box bins[L::sah_bins];
                        std::uint32_t counts[L::sah_bins]{};
                        const float scale = L::sah_bins / extent;
                        for (auto i = begin; i < end; ++i)
                        {
                            const auto b = bin(items[i], a, lo, scale);
                            bins[b].grow(items[i].box);
                            ++counts[b];
                        }
                        double right_area[L::sah_bins];
                        std::uint32_t right_count[L::sah_bins];
                        Box right;
                        std::uint32_t n = 0;
                        for (auto b = L::sah_bins; b-- > 1;)
                        {
                            right.grow(bins[b]);
                            n += counts[b];
                            right_area[b] = right.area();
                            right_count[b] = n;
                        }
                        Box left;
                        n = 0;
                        for (std::uint32_t b = 0; b + 1 < L::sah_bins; ++b)
                        {
                            left.grow(bins[b]);
                            n += counts[b];
                            if (!n || !right_count[b + 1])
                                continue;
                            // One traversal step costs about one triangle test.
                            const double sah =
                                bounds.area() + left.area() * n + right_area[b + 1] * right_count[b + 1];
                            if (sah < best)
                            {
                                best = sah;
                                axis = a;
                                split = b + 1;
                            }
                        }
                    }
                    if (axis >= 0)
                    {
                        const float lo = centroids.min[axis], scale = L::sah_bins / (centroids.max[axis] - lo);
                        auto it = std::stable_partition(items.begin() + begin, items.begin() + end,
                                                        [&](const Item& item)
                                                        { return bin(item, axis, lo, scale) < split; });
                        middle = static_cast<std::uint32_t>(it - items.begin());
                        if (middle == begin || middle == end)
                            axis = -1;
                    }
                    else if (count <= 4 * L::leaf_size)
                        return leaf(index, begin, end); // No split beats testing every triangle.
                }
                if (axis < 0)
                {
                    // Median of the centroids along the widest centroid axis; ties by input order.
                    axis = 0;
                    for (int a = 1; a < 3; ++a)
                        if (centroids.max[a] - centroids.min[a] > centroids.max[axis] - centroids.min[axis])
                            axis = a;
                    middle = begin + count / 2;
                    std::nth_element(items.begin() + begin, items.begin() + middle, items.begin() + end,
                                     [&](const Item& x, const Item& y)
                                     {
                                         return x.centroid[axis] < y.centroid[axis] ||
                                                (x.centroid[axis] == y.centroid[axis] && x.triangle < y.triangle);
                                     });
                }
                const auto first = static_cast<std::uint32_t>(nodes.size());
                nodes.resize(nodes.size() + 2);
                nodes[index].index = first;
                nodes[index].count = 0;
                nodes[index].axis = static_cast<std::uint16_t>(axis);
                build(first, begin, middle, level + 1);
                build(first + 1, middle, end, level + 1);
            }
        };
        bool equal_names(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                                      [](unsigned char x, unsigned char y)
                                                      { return std::tolower(x) == std::tolower(y); });
        }
        std::size_t wrap(float s, std::uint32_t size)
        {
            auto i = static_cast<long long>(std::floor(double(s) * size)) % static_cast<long long>(size);
            return static_cast<std::size_t>(i < 0 ? i + size : i);
        }
    } // namespace

    bool GiBvh::build(std::vector<Triangle> triangles, std::string& error)
    {
        nodes_.clear();
        triangles_.clear();
        depth_ = 0;
        if (triangles.empty())
            return true;
        std::vector<Item> items(triangles.size());
        for (std::size_t i = 0; i < triangles.size(); ++i)
        {
            const auto& t = triangles[i];
            auto& item = items[i];
            float p1[3], p2[3];
            for (int a = 0; a < 3; ++a)
            {
                p1[a] = t.v0[a] + t.e1[a];
                p2[a] = t.v0[a] + t.e2[a];
            }
            item.box.grow(t.v0);
            item.box.grow(p1);
            item.box.grow(p2);
            for (int a = 0; a < 3; ++a)
                item.centroid[a] = .5f * (item.box.min[a] + item.box.max[a]);
            item.triangle = static_cast<std::uint32_t>(i);
        }
        nodes_.reserve(2 * triangles.size() / L::leaf_size + 1);
        nodes_.resize(1);
        Builder builder{items, nodes_};
        builder.build(0, 0, static_cast<std::uint32_t>(items.size()), 0);
        if (!builder.ok)
        {
            nodes_.clear();
            error = "GI BVH leaf exceeds limits";
            return false;
        }
        depth_ = builder.depth;
        triangles_.reserve(items.size());
        for (const auto& item : items)
            triangles_.push_back(triangles[item.triangle]);
        return true;
    }

    bool gi_range_uses(const World& world, std::span<const std::string> excluded, std::vector<GiRangeUse>& use,
                       GiSceneReport& report, std::string& error)
    {
        use.clear();
        const auto roles = world_model_roles(world);
        // Sky models are the ones FLAG_SKYOBJECT marks: DemoSkyWorldModel models and SkyPointer targets
        // (doors and brushes drawn in the sky pass, never in the world). A DemoSkyWorldModel model whose
        // object the original refuses is drawn by nothing.
        const auto sky = gi_sky_setup(world);
        for (std::size_t m = 0; m < roles.size(); ++m)
            if (m && std::binary_search(sky.models.begin(), sky.models.end(), std::uint32_t(m)))
                use.push_back(GiRangeUse::Sky);
            else
                switch (roles[m].role)
                {
                case WorldModelRole::Primary:
                case WorldModelRole::Mover:
                case WorldModelRole::Destructible:
                    use.push_back(GiRangeUse::Main);
                    break;
                default:
                    use.push_back(GiRangeUse::Skip);
                }
        report.excluded_ranges = 0;
        for (const auto& name : excluded)
        {
            bool found = false;
            for (std::size_t m = 1; m < world.models.size() && m < use.size(); ++m)
                if (equal_names(world.models[m].name, name))
                {
                    found = true;
                    report.excluded_ranges += use[m] != GiRangeUse::Skip;
                    use[m] = GiRangeUse::Skip;
                }
            if (!found)
            {
                use.clear();
                error = "Excluded model '" + name + "' matches no world model";
                return false;
            }
        }
        return true;
    }

    std::uint8_t gi_albedo_mip(const std::array<DVector, 3>& p, const std::array<std::array<float, 2>, 3>& uv,
                               std::uint32_t width, std::uint32_t height, std::uint32_t levels)
    {
        if (levels <= 1)
            return 0;
        const double ax = double(p[1].x) - p[0].x, ay = double(p[1].y) - p[0].y, az = double(p[1].z) - p[0].z;
        const double bx = double(p[2].x) - p[0].x, by = double(p[2].y) - p[0].y, bz = double(p[2].z) - p[0].z;
        const double cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
        const double world = std::sqrt(cx * cx + cy * cy + cz * cz);
        const double su = (double(uv[1][0]) - uv[0][0]) * width, sv = (double(uv[1][1]) - uv[0][1]) * height;
        const double tu = (double(uv[2][0]) - uv[0][0]) * width, tv = (double(uv[2][1]) - uv[0][1]) * height;
        const double texels = std::abs(su * tv - sv * tu);
        const double density = std::sqrt(texels / world);
        if (!std::isfinite(density) || !(density > 0))
            return 0;
        const double level = std::round(std::log2(L::albedo_grid * density));
        return static_cast<std::uint8_t>(std::clamp(level, 0.0, double(std::min(levels, 255u) - 1)));
    }

    bool GiBakeScene::build(WorldMesh mesh, std::span<const GiRangeUse> use, GiSceneReport& report,
                            std::string& error)
    {
        const auto start = std::chrono::steady_clock::now();
        mesh_ = {};
        faces_.clear();
        pages_.clear();
        main_ = {};
        sky_ = {};
        const auto excluded = report.excluded_ranges;
        report = {};
        report.excluded_ranges = excluded;
        auto fail = [&](std::string message)
        {
            faces_.clear();
            pages_.clear();
            main_ = {};
            sky_ = {};
            error = std::move(message);
            return false;
        };
        if (use.size() != mesh.ranges.size())
            return fail("GI scene range uses do not match the world mesh");
        report.ranges = mesh.ranges.size();
        // Images must be complete: the albedo and alpha lookups index them without checks.
        std::vector<std::uint8_t> levels(mesh.materials.size(), 0);
        for (std::size_t m = 0; m < mesh.materials.size(); ++m)
        {
            const auto& material = mesh.materials[m];
            if (!material.error.empty() || !material.frames.empty())
                continue;
            const auto& image = material.image;
            if (!image.width || !image.height ||
                image.rgba.size() != std::size_t(image.width) * image.height * 4)
                continue;
            levels[m] = image.mipmaps > 1 && image.mip_rgba.size() == texture_mip_bytes(image)
                            ? static_cast<std::uint8_t>(std::min(image.mipmaps, 255u))
                            : 1;
        }
        std::map<std::uint32_t, std::uint32_t> pages;
        std::vector<GiBvh::Triangle> main, sky;
        for (std::size_t r = 0; r < mesh.ranges.size(); ++r)
        {
            const auto& range = mesh.ranges[r];
            if (use[r] == GiRangeUse::Skip)
            {
                ++report.skipped_ranges;
                continue;
            }
            ++(use[r] == GiRangeUse::Main ? report.main_ranges : report.sky_ranges);
            for (const auto& batch : range.batches)
            {
                if (batch.count % 3 || batch.first % 3 || batch.first > mesh.vertices.size() ||
                    batch.count > mesh.vertices.size() - batch.first ||
                    batch.count / 3 > L::max_triangles - faces_.size() || batch.material >= mesh.materials.size())
                    return fail("GI scene geometry exceeds limits");
                GiBakeFace face;
                face.range = static_cast<std::uint32_t>(r);
                face.material = batch.material;
                if (batch.lightmap)
                {
                    if (batch.lightmap >= mesh.materials.size() || !mesh.materials[batch.lightmap].error.empty())
                        return fail("Invalid GI scene lightmap page");
                    auto [found, added] = pages.emplace(batch.lightmap, static_cast<std::uint32_t>(pages_.size()));
                    if (added)
                    {
                        const auto& image = mesh.materials[batch.lightmap].image;
                        if (!image.width || !image.height ||
                            image.rgba.size() != std::size_t(image.width) * image.height * 4)
                            return fail("Invalid GI scene lightmap page");
                        pages_.push_back(image);
                    }
                    face.page = found->second;
                    face.flags |= GiBakeFace::Lightmapped;
                }
                const Texture* image = nullptr;
                std::uint32_t image_levels = 0;
                if (batch.material)
                {
                    const auto& material = mesh.materials[batch.material];
                    const auto source = material.frames.empty() ? batch.material : material.frames.front();
                    if (source >= mesh.materials.size() || !levels[source])
                        return fail("Invalid GI scene albedo image: " + material.path);
                    image = &mesh.materials[source].image;
                    image_levels = levels[source];
                    if (batch.translucent)
                        face.flags |= GiBakeFace::Translucent;
                }
                else
                    face.flags |= batch.sky_portal ? GiBakeFace::SkyPortal : GiBakeFace::Unresolved;
                for (auto i = batch.first; i < batch.first + batch.count; i += 3)
                {
                    std::array<DVector, 3> points;
                    for (int k = 0; k < 3; ++k)
                    {
                        const auto& vertex = mesh.vertices[i + k];
                        points[k] = vector_add(vertex.position, range.pivot);
                        face.uv[k] = vertex.uv;
                        face.lightmap[k] = vertex.lightmap;
                        face.colors[k] = vertex.color;
                    }
                    face.normal = mesh.vertices[i].normal;
                    face.triangle = i / 3;
                    face.mip = image ? gi_albedo_mip(points, face.uv, image->width, image->height, image_levels) : 0;
                    GiBvh::Triangle t;
                    const auto e1 = vector_subtract(points[1], points[0]);
                    const auto e2 = vector_subtract(points[2], points[0]);
                    t.v0[0] = points[0].x, t.v0[1] = points[0].y, t.v0[2] = points[0].z;
                    t.e1[0] = e1.x, t.e1[1] = e1.y, t.e1[2] = e1.z;
                    t.e2[0] = e2.x, t.e2[1] = e2.y, t.e2[2] = e2.z;
                    t.face = static_cast<std::uint32_t>(faces_.size());
                    (use[r] == GiRangeUse::Main ? main : sky).push_back(t);
                    faces_.push_back(face);
                    auto& counts = report;
                    ++(use[r] == GiRangeUse::Main ? counts.main_faces : counts.sky_faces);
                    counts.translucent += (face.flags & GiBakeFace::Translucent) != 0;
                    counts.sky_portals += (face.flags & GiBakeFace::SkyPortal) != 0;
                    counts.unresolved += (face.flags & GiBakeFace::Unresolved) != 0;
                    counts.lightmapped += (face.flags & GiBakeFace::Lightmapped) != 0;
                }
            }
        }
        if (!main_.build(std::move(main), error) || !sky_.build(std::move(sky), error))
            return fail(error);
        mesh_ = std::move(mesh);
        report.nodes = main_.nodes_.size();
        report.sky_nodes = sky_.nodes_.size();
        report.depth = std::max(main_.depth_, sky_.depth_);
        report.build_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return true;
    }

    const Texture* GiBakeScene::face_image(const GiBakeFace& face) const
    {
        const auto& material = mesh_.materials[face.material];
        return &mesh_.materials[material.frames.empty() ? face.material : material.frames.front()].image;
    }
    std::uint8_t GiBakeScene::alpha_byte(const GiBakeFace& face, float u, float v) const
    {
        const auto& image = *face_image(face);
        const float w = 1 - u - v;
        const float s = w * face.uv[0][0] + u * face.uv[1][0] + v * face.uv[2][0];
        const float t = w * face.uv[0][1] + u * face.uv[1][1] + v * face.uv[2][1];
        return image.rgba[(wrap(t, image.height) * image.width + wrap(s, image.width)) * 4 + 3];
    }
    std::array<float, 4> GiBakeScene::albedo(std::uint32_t index, float u, float v) const
    {
        const auto& face = faces_[index];
        if (face.flags & (GiBakeFace::Unresolved | GiBakeFace::SkyPortal))
            return {.5f, .5f, .5f, 1};
        const auto& image = *face_image(face);
        const float w = 1 - u - v;
        const float s = w * face.uv[0][0] + u * face.uv[1][0] + v * face.uv[2][0];
        const float t = w * face.uv[0][1] + u * face.uv[1][1] + v * face.uv[2][1];
        const std::uint8_t* pixels = image.rgba.data();
        std::uint32_t width = image.width, height = image.height;
        if (face.mip)
        {
            std::size_t offset = 0;
            for (std::uint32_t level = 1; level <= face.mip; ++level)
            {
                width = std::max(1u, image.width >> level);
                height = std::max(1u, image.height >> level);
                if (level < face.mip)
                    offset += std::size_t(width) * height * 4;
            }
            pixels = image.mip_rgba.data() + offset;
        }
        const auto* texel = pixels + (wrap(t, height) * width + wrap(s, width)) * 4;
        const float alpha = face.flags & GiBakeFace::Translucent ? alpha_byte(face, u, v) / 255.f : 1.f;
        return {texel[0] / 255.f, texel[1] / 255.f, texel[2] / 255.f, alpha};
    }
    bool GiBakeScene::prepare_emission(const SurfaceManifest& manifest, std::uint32_t scale,
                                       GiEmissionReport& report, std::string& error)
    {
        const auto start = std::chrono::steady_clock::now();
        report = {};
        emission_.clear();
        emission_index_.assign(mesh_.materials.size(), UINT32_MAX);
        if (!scale)
        {
            emission_index_.clear();
            return true;
        }
        const double factor = scale / 100.0;
        for (std::size_t m = 0; m < mesh_.materials.size(); ++m)
        {
            const auto& material = mesh_.materials[m];
            const auto& path = material.path;
            // SurfaceMaterials::refresh: only DTX paths read the manifest.
            const bool texture = path.size() > 4 && (path.ends_with(".dtx") || path.ends_with(".DTX"));
            if (!texture || !material.error.empty() || !material.frames.empty())
                continue;
            const auto& surface = manifest.resolve(path);
            if (!(surface.emissive > 0))
                continue;
            const auto& image = material.image;
            if (!image.width || !image.height || image.rgba.size() != std::size_t(image.width) * image.height * 4)
                return error = "Invalid GI emission image: " + path, false;
            const std::uint32_t levels = image.mipmaps > 1 && image.mip_rgba.size() == texture_mip_bytes(image)
                                             ? std::min(image.mipmaps, 255u)
                                             : 1;
            Emission chain;
            chain.width = image.width;
            chain.height = image.height;
            std::size_t total = 0;
            for (std::uint32_t level = 0; level < levels; ++level)
            {
                chain.offsets.push_back(total);
                total += std::size_t(std::max(1u, image.width >> level)) * std::max(1u, image.height >> level) * 3;
            }
            chain.rgb.resize(total);
            // The shader's emitted(): strength, threshold and BT.601 luminance, in float like the GPU.
            const float strength = surface.emissive, threshold = surface.emissive_threshold;
            const float ramp = std::max(1.0f - threshold, 1e-3f);
            std::uint64_t glowing = 0;
            for (std::size_t t = 0; t < std::size_t(image.width) * image.height; ++t)
            {
                const float c[3]{image.rgba[t * 4] / 255.f, image.rgba[t * 4 + 1] / 255.f,
                                 image.rgba[t * 4 + 2] / 255.f};
                const float luminance = c[0] * .299f + c[1] * .587f + c[2] * .114f;
                const float weight = strength * std::clamp((luminance - threshold) / ramp, 0.f, 1.f);
                for (int k = 0; k < 3; ++k)
                    chain.rgb[t * 3 + k] = float(c[k] * weight * factor);
                glowing += weight > 0 && (c[0] > 0 || c[1] > 0 || c[2] > 0);
            }
            if (!glowing)
                continue;
            // Box filter: each texel of a level averages the (up to) 2 x 2 texels above it.
            for (std::uint32_t level = 1; level < levels; ++level)
            {
                const std::uint32_t pw = std::max(1u, image.width >> (level - 1)),
                                    ph = std::max(1u, image.height >> (level - 1));
                const std::uint32_t w = std::max(1u, image.width >> level), h = std::max(1u, image.height >> level);
                const float* above = chain.rgb.data() + chain.offsets[level - 1];
                float* out = chain.rgb.data() + chain.offsets[level];
                for (std::uint32_t y = 0; y < h; ++y)
                    for (std::uint32_t x = 0; x < w; ++x)
                    {
                        double sum[3]{};
                        int count = 0;
                        for (std::uint32_t sy = 2 * y; sy < std::min(2 * y + 2, ph); ++sy)
                            for (std::uint32_t sx = 2 * x; sx < std::min(2 * x + 2, pw); ++sx, ++count)
                                for (int k = 0; k < 3; ++k)
                                    sum[k] += above[(std::size_t(sy) * pw + sx) * 3 + k];
                        for (int k = 0; k < 3; ++k)
                            out[(std::size_t(y) * w + x) * 3 + k] = float(sum[k] / count);
                    }
            }
            emission_index_[m] = static_cast<std::uint32_t>(emission_.size());
            emission_.push_back(std::move(chain));
            ++report.materials;
            report.texels += glowing;
        }
        if (emission_.empty())
            emission_index_.clear();
        else
            for (const auto& face : faces_)
                report.faces += face.material < emission_index_.size() && emission_index_[face.material] != UINT32_MAX;
        report.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return true;
    }
    DVector GiBakeScene::emission(std::uint32_t index, float u, float v) const
    {
        if (emission_.empty())
            return {};
        const auto& face = faces_[index];
        if (face.flags & (GiBakeFace::Unresolved | GiBakeFace::SkyPortal) || face.material >= emission_index_.size())
            return {};
        const auto at = emission_index_[face.material];
        if (at == UINT32_MAX)
            return {};
        const auto& chain = emission_[at];
        const std::uint32_t level = std::min<std::uint32_t>(face.mip, std::uint32_t(chain.offsets.size() - 1));
        const std::uint32_t width = std::max(1u, chain.width >> level), height = std::max(1u, chain.height >> level);
        const float w = 1 - u - v;
        const float s = w * face.uv[0][0] + u * face.uv[1][0] + v * face.uv[2][0];
        const float t = w * face.uv[0][1] + u * face.uv[1][1] + v * face.uv[2][1];
        const float* texel = chain.rgb.data() + chain.offsets[level] + (wrap(t, height) * width + wrap(s, width)) * 3;
        return {texel[0], texel[1], texel[2]};
    }
    DVector GiBakeScene::baked_light(std::uint32_t index, float u, float v) const
    {
        const auto& face = faces_[index];
        const float w = 1 - u - v;
        if (face.page == UINT32_MAX)
            return {w * face.colors[0].x + u * face.colors[1].x + v * face.colors[2].x,
                    w * face.colors[0].y + u * face.colors[1].y + v * face.colors[2].y,
                    w * face.colors[0].z + u * face.colors[1].z + v * face.colors[2].z};
        // WorldShade::sample_page: bilinear like the renderer's lightmap sampler, then the scale.
        const auto& page = pages_[face.page];
        const float pu = w * face.lightmap[0][0] + u * face.lightmap[1][0] + v * face.lightmap[2][0];
        const float pv = w * face.lightmap[0][1] + u * face.lightmap[1][1] + v * face.lightmap[2][1];
        const float x = pu * page.width - .5f, y = pv * page.height - .5f;
        const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
        const float fx = x - x0, fy = y - y0;
        auto texel = [&](int i, int j, int channel)
        {
            i = std::clamp(i, 0, int(page.width) - 1);
            j = std::clamp(j, 0, int(page.height) - 1);
            return page.rgba[(std::size_t(j) * page.width + i) * 4 + channel] / 255.f;
        };
        float out[3];
        for (int c = 0; c < 3; ++c)
            out[c] = world_saturate_scale * ((1 - fy) * ((1 - fx) * texel(x0, y0, c) + fx * texel(x0 + 1, y0, c)) +
                                             fy * ((1 - fx) * texel(x0, y0 + 1, c) + fx * texel(x0 + 1, y0 + 1, c)));
        return {out[0], out[1], out[2]};
    }

    void GiBakeScene::trace(DVector origin, DVector direction, float tmax, GiHit& hit, GiTraceStats& stats) const
    {
        trace_tree(main_, origin, direction, tmax, hit, stats);
    }
    void GiBakeScene::trace_sky(DVector origin, DVector direction, float tmax, GiHit& hit,
                                GiTraceStats& stats) const
    {
        trace_tree(sky_, origin, direction, tmax, hit, stats);
    }
    void GiBakeScene::trace_tree(const GiBvh& tree, DVector origin, DVector direction, float tmax, GiHit& hit,
                                 GiTraceStats& stats) const
    {
        hit = {};
        ++stats.rays;
        if (tree.nodes_.empty() || !(tmax >= 0))
            return;
        const float o[3]{origin.x, origin.y, origin.z}, d[3]{direction.x, direction.y, direction.z};
        float inv[3];
        for (int a = 0; a < 3; ++a)
            inv[a] = d[a] != 0 ? 1 / d[a] : std::copysign(1e30f, d[a]);
        // Opaque hit so far and the nearest translucent layers (one more than passes through).
        float best = std::min(tmax, FLT_MAX);
        std::uint32_t best_face = UINT32_MAX;
        float best_u = 0, best_v = 0;
        std::array<GiLayer, L::max_layers + 1> layers;
        std::uint32_t layer_count = 0;
        auto limit = [&]
        { return layer_count == layers.size() ? std::min(best, layers.back().t) : best; };
        std::uint32_t stack[L::max_depth + 2];
        std::uint32_t top = 0, node_index = 0;
        std::uint64_t nodes = 0, triangles = 0, texels = 0;
        for (;;)
        {
            const auto& node = tree.nodes_[node_index];
            ++nodes;
            float near = 0, far = limit();
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
                    // Nearer child first: the first child holds the lower centroids on the axis.
                    const bool flip = d[node.axis] < 0;
                    stack[top++] = node.index + (flip ? 0 : 1);
                    node_index = node.index + (flip ? 1 : 0);
                    continue;
                }
                for (std::uint32_t i = node.index; i < node.index + node.count; ++i)
                {
                    const auto& t = tree.triangles_[i];
                    ++triangles;
                    // Two-sided Moller-Trumbore, WorldShade's arithmetic.
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
                    if (f < 0 || f > limit())
                        continue;
                    const auto& face = faces_[t.face];
                    if (face.flags & GiBakeFace::Translucent)
                    {
                        ++texels;
                        const auto alpha = alpha_byte(face, u, v);
                        if (!alpha)
                            continue; // Cutout.
                        if (alpha < 255)
                        {
                            const GiLayer layer{f, u, v, t.face, alpha / 255.f};
                            auto before = [](const GiLayer& a, const GiLayer& b)
                            { return a.t < b.t || (a.t == b.t && a.face < b.face); };
                            if (layer_count == layers.size() && !before(layer, layers.back()))
                                continue;
                            // Insert in order; a full list drops its farthest layer.
                            std::uint32_t at = 0;
                            while (at < layer_count && !before(layer, layers[at]))
                                ++at;
                            for (auto k = std::min<std::uint32_t>(layer_count, layers.size() - 1); k > at; --k)
                                layers[k] = layers[k - 1];
                            layers[at] = layer;
                            layer_count = std::min<std::uint32_t>(layer_count + 1, layers.size());
                            continue;
                        }
                    }
                    if (f < best || (f == best && t.face < best_face))
                    {
                        best = f;
                        best_face = t.face;
                        best_u = u;
                        best_v = v;
                    }
                }
            }
            if (!top)
                break;
            node_index = stack[--top];
        }
        stats.nodes += nodes;
        stats.triangles += triangles;
        stats.texels += texels;
        std::uint32_t passed = 0;
        while (passed < layer_count && (best_face == UINT32_MAX || layers[passed].t < best))
            ++passed;
        auto finish = [&](std::uint32_t face, float t, float u, float v)
        {
            hit.face = face;
            hit.t = t;
            hit.u = u;
            hit.v = v;
            hit.back = vector_dot(direction, faces_[face].normal) > 0;
        };
        if (passed > L::max_layers)
        {
            hit.kind = GiHit::Kind::LayerLimit;
            const auto& last = layers[L::max_layers];
            finish(last.face, last.t, last.u, last.v);
            passed = L::max_layers;
        }
        else if (best_face != UINT32_MAX)
        {
            hit.kind = faces_[best_face].flags & GiBakeFace::SkyPortal ? GiHit::Kind::Sky : GiHit::Kind::Surface;
            finish(best_face, best, best_u, best_v);
        }
        hit.layer_count = passed;
        std::copy(layers.begin(), layers.begin() + passed, hit.layers.begin());
    }
} // namespace OpenBlood2
