#include "engine/sprite_view.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    void SpriteView::clear()
    {
        mesh = {};
        mesh.materials.push_back({"", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}, {}}});
        mesh.texture_bytes = 4;
        materials_.clear();
        draws.clear();
        batched_draws.clear();
        entries.clear();
        camera_valid_ = false;
        ++revision;
    }
    bool SpriteView::begin_frame(DVector eye, DRotation camera, std::string& error)
    {
        error.clear();
        mesh.vertices.clear();
        mesh.ranges.clear();
        draws.clear();
        batched_draws.clear();
        entries.clear();
        WorldViewUniform uniform;
        camera_valid_ = world_view_uniform(eye, camera, 1, {}, uniform);
        if (!camera_valid_)
        {
            error = "Invalid sprite camera transform";
            return false;
        }
        eye_ = eye;
        camera_ = camera;
        forward_ = {uniform.forward[0], uniform.forward[1], uniform.forward[2]};
        return true;
    }
    bool SpriteView::add(const Sprite* sprite, const SpriteTrack& track, NodePose pose, DVector scale, DDWORD flags,
                         ObjectId object, const Resources& resources, std::string& error, float alpha, DVector color)
    {
        const std::string_view path =
            sprite && track.frame < sprite->frames.size() ? sprite->frames[track.frame] : std::string_view{};
        return add_frame(path, track.frame, pose, scale, track.has_frame ? flags : flags & ~FLAG_VISIBLE, object,
                         resources, error, alpha, color, false);
    }
    bool SpriteView::add_particle(std::string_view path, DVector position, float extent, DVector color, float alpha,
                                  std::uint64_t system, unsigned index, const Resources& resources, std::string& error)
    {
        if (!add_frame(path, 0, {position, {}}, {extent, extent, 0}, FLAG_VISIBLE, {}, resources, error, alpha, color,
                       true))
            return false;
        entries.back().particle_system = system;
        entries.back().particle_index = index;
        return true;
    }
    SpriteViewEntry* SpriteView::begin_entry(ObjectId object, float alpha, std::string& error)
    {
        error.clear();
        if (!camera_valid_ || entries.size() >= max_entries)
        {
            mesh.vertices.clear();
            mesh.ranges.clear();
            draws.clear();
            batched_draws.clear();
            entries.clear();
            error = "Sprite camera unavailable or instance budget exceeded";
            return nullptr;
        }
        entries.push_back({});
        auto& entry = entries.back();
        entry.object = object;
        entry.alpha = alpha;
        if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
            entry.error = "Invalid sprite alpha";
        return &entry;
    }
    const SpriteView::Material* SpriteView::material(std::string_view path, SpriteViewEntry& entry,
                                                     const Resources& resources, std::string& error)
    {
        if (path.empty())
        {
            entry.error = "Sprite manifest/current frame unavailable";
            return nullptr;
        }
        entry.frame_path = path;
        std::string key;
        if (!resource_path(entry.frame_path, key))
        {
            entry.error = "Invalid sprite frame path";
            return nullptr;
        }
        auto found = materials_.find(key);
        if (found == materials_.end())
        {
            if (mesh.materials.size() >= 4097)
            {
                error = "Sprite material count budget exceeded";
                return nullptr;
            }
            WorldMaterial next;
            next.path = key;
            read_texture_resource(resources, key, next.image, next.error);
            if (next.image.rgba.size() > 128 * 1024 * 1024 - mesh.texture_bytes)
            {
                error = "Sprite texture memory budget exceeded";
                return nullptr;
            }
            Material material{static_cast<std::uint32_t>(mesh.materials.size())};
            for (std::size_t i = 3; i < next.image.rgba.size(); i += 4)
            {
                material.zero_alpha += next.image.rgba[i] == 0;
                material.partial_alpha += next.image.rgba[i] > 0 && next.image.rgba[i] < 255;
            }
            mesh.texture_bytes += next.image.rgba.size();
            mesh.materials.push_back(std::move(next));
            found = materials_.emplace(key, material).first;
            ++revision;
        }
        const auto& material = found->second;
        const auto& texture = mesh.materials[material.index];
        entry.error = texture.error;
        entry.width = texture.image.width;
        entry.height = texture.image.height;
        entry.zero_alpha = material.zero_alpha;
        entry.partial_alpha = material.partial_alpha;
        return entry.error.empty() ? &material : nullptr;
    }
    bool SpriteView::add_frame(std::string_view path, unsigned frame, NodePose pose, DVector scale, DDWORD flags,
                               ObjectId object, const Resources& resources, std::string& error, float alpha,
                               DVector color, bool direct_extent)
    {
        auto* added = begin_entry(object, alpha, error);
        if (!added)
            return false;
        auto& entry = *added;
        if (!entry.error.empty())
            return true;
        entry.frame = frame;
        entry.visible = (flags & FLAG_VISIBLE) != 0;
        entry.rotatable = (flags & FLAG_ROTATEABLESPRITE) != 0;
        const auto* found = this->material(path, entry, resources, error);
        if (!found)
            return error.empty();
        const auto& material = *found;
        // Original SDK bits: glow, view bias, no-Z and sky need separate policies.
        // FLAG_SPRITECHROMAKEY is explicitly disabled in that SDK; do not key RGB.
        if (flags & ((1u << 5) | (1u << 6) | (1u << 7) | (1u << 12)))
        {
            entry.error = "Sprite glow/bias/no-Z/sky presentation unavailable";
            return true;
        }
        WorldDraw draw{static_cast<std::uint32_t>(mesh.ranges.size()), pose.position,
                       entry.rotatable ? pose.rotation : camera_, alpha};
        WorldViewUniform uniform;
        if (!world_view_uniform(eye_, camera_, 1, draw, uniform) || !physics_vector(scale) || scale.x < 0 ||
            scale.y < 0 || scale.z < 0)
        {
            entry.error = "Invalid sprite transform/scale";
            return true;
        }
        if (!physics_vector(color) || color.x < 0 || color.x > 1 || color.y < 0 || color.y > 1 || color.z < 0 ||
            color.z > 1)
        {
            entry.error = "Invalid sprite color";
            return true;
        }
        const float x = direct_extent ? scale.x : float(entry.width) * scale.x;
        const float y = direct_extent ? scale.y : float(entry.height) * scale.y;
        if (!physics_vector({x, y, 0}))
        {
            entry.error = "Sprite dimensions exceed presentation limits";
            return true;
        }
        if (!entry.visible || x == 0 || y == 0)
            return true;
        // Later LithTech renderer uses texture dimensions as half extents. This is
        // replacement presentation, not a claim of verified Blood II raster parity.
        const WorldVertex corners[] = {{{-x, y, 0}, color, {0, 0}},
                                       {{x, y, 0}, color, {1, 0}},
                                       {{x, -y, 0}, color, {1, 1}},
                                       {{-x, -y, 0}, color, {0, 1}}};
        WorldRange range;
        range.first = static_cast<std::uint32_t>(mesh.vertices.size());
        range.count = 6;
        range.name = entry.frame_path;
        range.batches.push_back({range.first, 6, material.index});
        for (auto index : {0, 1, 2, 0, 2, 3})
            mesh.vertices.push_back(corners[index]);
        bound_range(mesh.vertices, range);
        entry.range = draw.range;
        mesh.ranges.push_back(std::move(range));
        draws.push_back(draw);
        return true;
    }
    bool SpriteView::add_particle_system(std::string_view path, DVector origin, std::span<const Particle> particles,
                                         float extent, float alpha, std::uint64_t system, const Resources& resources,
                                         std::string& error)
    {
        auto* added = begin_entry({}, alpha, error);
        if (!added)
            return false;
        auto& entry = *added;
        entry.particle_system = system;
        if (!entry.error.empty())
            return true;
        entry.visible = true;
        const auto* found = material(path, entry, resources, error);
        if (!found)
            return error.empty();
        if (!physics_vector(origin) || !std::isfinite(extent) || extent < 0 || extent > 1e6f)
        {
            entry.error = "Invalid sprite transform/scale";
            return true;
        }
        // Valid particles back to front, as the per-particle path's depth sort would order them.
        auto& order = order_;
        order.clear();
        DVector centre{};
        std::size_t invalid = 0;
        for (std::uint32_t i = 0; i < particles.size(); ++i)
        {
            const auto& p = particles[i];
            const auto at = vector_add(origin, p.position);
            if (!physics_vector(at) || !physics_vector(p.color) || p.color.x < 0 || p.color.x > 1 || p.color.y < 0 ||
                p.color.y > 1 || p.color.z < 0 || p.color.z > 1)
            {
                ++invalid;
                continue;
            }
            const DVector d{at.x - eye_.x, at.y - eye_.y, at.z - eye_.z};
            order.push_back({double(d.x) * forward_.x + double(d.y) * forward_.y + double(d.z) * forward_.z, i});
            centre = vector_add(centre, at);
        }
        if (invalid)
            entry.error = std::to_string(invalid) + " invalid particle position/colour values skipped";
        if (order.empty() || extent == 0)
            return true;
        centre = vector_scale(centre, 1.f / float(order.size()));
        std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        // Camera-facing corners (the per-particle path's camera rotation), relative to the centre.
        const auto right = vector_scale(rotate_vector({1, 0, 0}, camera_), extent);
        const auto up = vector_scale(rotate_vector({0, 1, 0}, camera_), extent);
        WorldRange range;
        range.first = static_cast<std::uint32_t>(mesh.vertices.size());
        range.count = std::uint32_t(order.size() * 6);
        range.name = entry.frame_path;
        range.batches.push_back({range.first, range.count, found->index});
        mesh.vertices.reserve(mesh.vertices.size() + range.count);
        for (const auto& [depth, i] : order)
        {
            (void)depth;
            const auto& p = particles[i];
            const auto at = vector_subtract(vector_add(origin, p.position), centre);
            const WorldVertex corners[] = {{vector_add(vector_subtract(at, right), up), p.color, {0, 0}},
                                           {vector_add(vector_add(at, right), up), p.color, {1, 0}},
                                           {vector_subtract(vector_add(at, right), up), p.color, {1, 1}},
                                           {vector_subtract(vector_subtract(at, right), up), p.color, {0, 1}}};
            for (auto index : {0, 1, 2, 0, 2, 3})
                mesh.vertices.push_back(corners[index]);
        }
        bound_range(mesh.vertices, range);
        entry.range = static_cast<std::uint32_t>(mesh.ranges.size());
        entry.particles = std::uint32_t(order.size());
        mesh.ranges.push_back(std::move(range));
        draws.push_back({entry.range, centre, {}, alpha});
        return true;
    }
    void SpriteView::sort()
    {
        auto depth = [&](const WorldDraw& draw)
        {
            const DVector d{draw.position.x - eye_.x, draw.position.y - eye_.y, draw.position.z - eye_.z};
            return double(d.x) * forward_.x + double(d.y) * forward_.y + double(d.z) * forward_.z;
        };
        std::stable_sort(draws.begin(), draws.end(), [&](const auto& a, const auto& b) { return depth(a) > depth(b); });
    }
    void SpriteView::batch()
    {
        batched_draws.clear();
        const auto first_batch = mesh.ranges.size();
        for (const auto& draw : draws)
        {
            const auto source = mesh.ranges[draw.range].first, count = mesh.ranges[draw.range].count;
            const auto material = mesh.ranges[draw.range].batches.front().material;
            if (batched_draws.empty() || batched_draws.back().alpha != draw.alpha ||
                mesh.ranges[batched_draws.back().range].batches.front().material != material)
            {
                WorldRange range;
                range.first = static_cast<std::uint32_t>(mesh.vertices.size());
                range.batches.push_back({range.first, 0, material});
                batched_draws.push_back({static_cast<std::uint32_t>(mesh.ranges.size()), {}, {}, draw.alpha});
                mesh.ranges.push_back(std::move(range));
            }
            auto& range = mesh.ranges[batched_draws.back().range];
            for (std::uint32_t i = source; i < source + count; ++i)
            {
                auto vertex = mesh.vertices[i];
                vertex.position = vector_add(draw.position, rotate_vector(vertex.position, draw.rotation));
                mesh.vertices.push_back(vertex);
            }
            range.count += count;
            range.batches.front().count += count;
        }
        for (auto i = first_batch; i < mesh.ranges.size(); ++i)
            bound_range(mesh.vertices, mesh.ranges[i]);
    }
} // namespace OpenBlood2
