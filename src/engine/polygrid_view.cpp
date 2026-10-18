#include "polygrid_view.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
void PolyGridView::clear()
{
    mesh = {};
    mesh.materials.push_back({"", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}, {}}});
    mesh.texture_bytes = 4;
    materials_.clear();
    begin_frame();
    ++revision;
}
void PolyGridView::begin_frame()
{
    mesh.vertices.clear();
    mesh.ranges.clear();
    entries.clear();
    draws.clear();
}
bool PolyGridView::add(std::uint64_t id, bool visible, NodePose pose, DVector dims, unsigned width,
                       std::span<const std::int8_t> heights, const std::array<DVector, 256>& palette,
                       std::array<float, 2> scale, std::array<float, 2> pan, float alpha, std::string_view path,
                       const Resources& resources, std::string& error)
{
    error.clear();
    WorldViewUniform uniform;
    if (entries.size() >= 128 || width < 2 || width > 128 || heights.size() != width * width || !physics_vector(dims) ||
        dims.x <= 0 || dims.z <= 0 || dims.y < 0 || !std::isfinite(alpha) || alpha < 0 || alpha > 1 ||
        !world_view_uniform({}, {}, 1, {0, pose.position, pose.rotation}, uniform))
    {
        error = "Invalid PolyGrid mesh/pose/budget";
        return false;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (!std::isfinite(scale[i]) || scale[i] <= 0 || scale[i] > 1000000 || !std::isfinite(pan[i]) ||
            std::abs(pan[i]) > 1000000)
        {
            error = "Invalid PolyGrid texture transform";
            return false;
        }
    for (auto c : palette)
        if (!physics_vector(c) || c.x < 0 || c.y < 0 || c.z < 0 || c.x > 1 || c.y > 1 || c.z > 1)
        {
            error = "Invalid PolyGrid palette";
            return false;
        }
    entries.push_back({id, std::string(path), {}, visible, UINT32_MAX, width});
    auto& e = entries.back();
    std::uint32_t material = 0;
    if (!path.empty())
    {
        std::string key;
        if (!resource_path(path, key))
        {
            e.error = "Invalid PolyGrid frame path";
            return true;
        }
        auto it = materials_.find(key);
        if (it == materials_.end())
        {
            if (mesh.materials.size() >= 4097)
            {
                error = "PolyGrid material budget exceeded";
                return false;
            }
            WorldMaterial m;
            m.path = key;
            read_texture_resource(resources, key, m.image, m.error);
            if (m.image.rgba.size() > 128 * 1024 * 1024 - mesh.texture_bytes)
            {
                error = "PolyGrid texture budget exceeded";
                return false;
            }
            mesh.texture_bytes += m.image.rgba.size();
            auto index = std::uint32_t(mesh.materials.size());
            mesh.materials.push_back(std::move(m));
            it = materials_.emplace(key, index).first;
            ++revision;
        }
        material = it->second;
        e.error = mesh.materials[material].error;
        if (!e.error.empty())
            return true;
    }
    if (!visible)
        return true;
    const auto count = 6 * (width - 1) * (width - 1);
    if (mesh.vertices.size() + count > 1000000)
    {
        error = "PolyGrid vertex budget exceeded";
        return false;
    }
    WorldRange range;
    range.first = std::uint32_t(mesh.vertices.size());
    range.count = count;
    range.name = path;
    // Original API height [-127,127], palette index height+128. Later renderer
    // corroborates XZ spacing and normalized repeat UVs; no retail raster parity claim.
    auto vertex = [&](unsigned x, unsigned y)
    {
        const auto h = heights[y * width + x];
        const float u = float(x) / float(width - 1), v = float(y) / float(width - 1);
        return WorldVertex{{(2 * u - 1) * dims.x, float(h) * dims.y / 127, (2 * v - 1) * dims.z},
                           palette[unsigned(int(h) + 128)],
                           {std::fmod(pan[0], 1.f) + u * scale[0], std::fmod(pan[1], 1.f) + v * scale[1]}};
    };
    for (unsigned y = 0; y + 1 < width; ++y)
        for (unsigned x = 0; x + 1 < width; ++x)
        {
            const WorldVertex corners[] = {vertex(x, y), vertex(x + 1, y), vertex(x + 1, y + 1), vertex(x, y + 1)};
            for (auto i : {0, 1, 2, 0, 2, 3})
                mesh.vertices.push_back(corners[i]);
        }
    // One translucent batch per surface, ordered against other surfaces by its origin.
    // Cells draw in lattice order, as later LithTech submits a PolyGrid as one indexed list.
    range.batches.push_back({range.first, count, material, true, {}});
    bound_range(mesh.vertices, range);
    e.range = std::uint32_t(mesh.ranges.size());
    draws.push_back({e.range, pose.position, pose.rotation, alpha});
    mesh.ranges.push_back(std::move(range));
    return true;
}
