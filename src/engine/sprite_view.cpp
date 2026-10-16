#include "engine/sprite_view.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    void SpriteView::clear()
    {
        mesh = {};
        mesh.materials.push_back({"", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}}});
        mesh.texture_bytes = 4;
        materials_.clear();
        draws.clear();
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
                         ObjectId object, const Resources& resources, std::string& error)
    {
        error.clear();
        if (!camera_valid_ || entries.size() >= 4096)
        {
            mesh.vertices.clear();
            mesh.ranges.clear();
            draws.clear();
            entries.clear();
            error = "Sprite camera unavailable or instance budget exceeded";
            return false;
        }
        entries.push_back({});
        auto& entry = entries.back();
        entry.object = object;
        entry.frame = track.frame;
        entry.visible = (flags & FLAG_VISIBLE) != 0;
        entry.rotatable = (flags & FLAG_ROTATEABLESPRITE) != 0;
        if (!sprite || track.frame >= sprite->frames.size())
        {
            entry.error = "Sprite manifest/current frame unavailable";
            return true;
        }
        entry.frame_path = sprite->frames[track.frame];
        std::string key;
        if (!resource_path(entry.frame_path, key))
        {
            entry.error = "Invalid sprite frame path";
            return true;
        }
        auto found = materials_.find(key);
        if (found == materials_.end())
        {
            if (mesh.materials.size() >= 4097)
            {
                error = "Sprite material count budget exceeded";
                return false;
            }
            WorldMaterial next;
            next.path = key;
            read_texture_resource(resources, key, next.image, next.error);
            if (next.image.rgba.size() > 128 * 1024 * 1024 - mesh.texture_bytes)
            {
                error = "Sprite texture memory budget exceeded";
                return false;
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
        if (!entry.error.empty())
            return true;
        // Original SDK bits: glow, view bias, no-Z and sky need separate policies.
        // FLAG_SPRITECHROMAKEY is explicitly disabled in that SDK; do not key RGB.
        if (flags & ((1u << 5) | (1u << 6) | (1u << 7) | (1u << 12)))
        {
            entry.error = "Sprite glow/bias/no-Z/sky presentation unavailable";
            return true;
        }
        WorldDraw draw{static_cast<std::uint32_t>(mesh.ranges.size()), pose.position,
                       entry.rotatable ? pose.rotation : camera_};
        WorldViewUniform uniform;
        if (!world_view_uniform(eye_, camera_, 1, draw, uniform) || !physics_vector(scale) || scale.x < 0 ||
            scale.y < 0 || scale.z < 0)
        {
            entry.error = "Invalid sprite transform/scale";
            return true;
        }
        const float x = float(entry.width) * scale.x, y = float(entry.height) * scale.y;
        if (!physics_vector({x, y, 0}))
        {
            entry.error = "Sprite dimensions exceed presentation limits";
            return true;
        }
        if (!entry.visible || x == 0 || y == 0)
            return true;
        // Later LithTech renderer uses texture dimensions as half extents. This is
        // replacement presentation, not a claim of verified Blood II raster parity.
        const WorldVertex corners[] = {{{-x, y, 0}, {1, 1, 1}, {0, 0}},
                                       {{x, y, 0}, {1, 1, 1}, {1, 0}},
                                       {{x, -y, 0}, {1, 1, 1}, {1, 1}},
                                       {{-x, -y, 0}, {1, 1, 1}, {0, 1}}};
        WorldRange range;
        range.first = static_cast<std::uint32_t>(mesh.vertices.size());
        range.count = 6;
        range.name = entry.frame_path;
        range.batches.push_back({range.first, 6, material.index});
        for (auto index : {0, 1, 2, 0, 2, 3})
            mesh.vertices.push_back(corners[index]);
        entry.range = draw.range;
        mesh.ranges.push_back(std::move(range));
        draws.push_back(draw);
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
} // namespace OpenBlood2
