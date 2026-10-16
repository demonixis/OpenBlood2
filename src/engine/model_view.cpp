#include "engine/model_view.h"
#include "engine/collision.h"
#include <cmath>
namespace OpenBlood2
{
    void ModelView::clear()
    {
        mesh = {};
        mesh.materials.push_back({"", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}}});
        mesh.texture_bytes = 4;
        materials_.clear();
        sampled_.clear();
        begin_frame();
        ++revision;
    }
    void ModelView::begin_frame()
    {
        mesh.vertices.clear();
        mesh.ranges.clear();
        world_draws.clear();
        close_draws.clear();
        entries.clear();
    }
    bool ModelView::add(const Model* model, const AnimationTrack& track, std::string_view skin, NodePose pose,
                        DVector scale, bool visible, bool close, ObjectId object, std::string_view label,
                        const Resources& resources, std::string& error, std::span<const std::uint8_t> hidden_nodes,
                        float alpha, bool require_skin)
    {
        error.clear();
        auto fail = [&](const char* reason)
        {
            error = reason;
            begin_frame();
            return false;
        };
        if (entries.size() >= 4096)
        {
            return fail("Model instance budget exceeded");
        }
        entries.push_back({object, std::string(label), std::string(skin), {}, visible, close, 0});
        auto& entry = entries.back();
        entry.alpha = alpha;
        if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
            return fail("Invalid model alpha");
        if (!model)
        {
            entry.error = "Model is missing or not decoded";
            return true;
        }
        if (!hidden_nodes.empty() && hidden_nodes.size() != model->nodes.size())
            return fail("Invalid model node visibility mask");
        for (auto hidden : hidden_nodes)
            entry.hidden_nodes += hidden != 0;
        std::uint32_t material = 0;
        std::string key;
        if (!resource_path(skin, key))
            entry.error = "Missing or invalid skin path";
        else
        {
            auto found = materials_.find(key);
            if (found == materials_.end())
            {
                if (mesh.materials.size() >= 4097)
                {
                    return fail("Model material count budget exceeded");
                }
                WorldMaterial next;
                next.path = key;
                read_texture_resource(resources, key, next.image, next.error);
                if (next.image.rgba.size() > 128 * 1024 * 1024 - mesh.texture_bytes)
                {
                    return fail("Model texture memory budget exceeded");
                }
                mesh.texture_bytes += next.image.rgba.size();
                auto index = static_cast<std::uint32_t>(mesh.materials.size());
                mesh.materials.push_back(std::move(next));
                found = materials_.emplace(key, index).first;
                ++revision;
            }
            entry.error = mesh.materials[found->second].error;
            if (entry.error.empty())
                material = found->second;
        }
        if (!visible || (require_skin && !entry.error.empty()))
            return true;
        if (!physics_vector(scale) || scale.x <= 0 || scale.y <= 0 || scale.z <= 0 || model->vertices.size() > 65536 ||
            model->faces.size() > 200000 || model->faces.size() * 3 > 1000000 - mesh.vertices.size())
        {
            return fail("Invalid model scale/geometry budget");
        }
        WorldDraw draw{static_cast<std::uint32_t>(mesh.ranges.size()), pose.position, pose.rotation, alpha};
        WorldViewUniform uniform;
        if (!world_view_uniform({}, {}, 1, draw, uniform))
        {
            return fail("Invalid model presentation transform");
        }
        std::array<int, 256> node_slots;
        node_slots.fill(-1);
        if (model->nodes.size() > 256)
            return fail("Invalid model node count");
        for (std::size_t i = 0; i < model->nodes.size(); ++i)
        {
            const auto index = model->nodes[i].index;
            if (index >= node_slots.size() || node_slots[index] != -1)
                return fail("Invalid model node index");
            node_slots[index] = static_cast<int>(i);
        }
        if (!sample_vertices(*model, track.animation, track.time, sampled_))
        {
            entry.error = "Model animation pose is unavailable";
            return true;
        }
        WorldRange range;
        range.first = static_cast<std::uint32_t>(mesh.vertices.size());
        range.name = entry.label;
        const DVector color = material ? DVector{1, 1, 1} : DVector{1, 0, 1};
        for (const auto& face : model->faces)
        {
            bool hidden = false;
            for (auto vertex : face.vertices)
            {
                if (vertex >= model->vertices.size() || node_slots[model->vertices[vertex].node] < 0)
                    return fail("Invalid model face node index");
                hidden |= !hidden_nodes.empty() && hidden_nodes[node_slots[model->vertices[vertex].node]] != 0;
            }
            if (hidden)
            {
                ++entry.hidden_triangles;
                continue;
            }
            for (unsigned corner = 0; corner < 3; ++corner)
            {
                if (face.vertices[corner] >= sampled_.size())
                {
                    return fail("Invalid model face index");
                }
                const auto p = sampled_[face.vertices[corner]];
                const DVector scaled{p.x * scale.x, p.y * scale.y, p.z * scale.z};
                const auto uv = face.uv[corner];
                if (!physics_vector(scaled) || !std::isfinite(uv[0]) || !std::isfinite(uv[1]) ||
                    std::abs(uv[0]) > 1e6f || std::abs(uv[1]) > 1e6f)
                {
                    return fail("Invalid model vertex/UV");
                }
                mesh.vertices.push_back({scaled, color, uv});
            }
        }
        range.count = static_cast<std::uint32_t>(mesh.vertices.size()) - range.first;
        range.batches.push_back({range.first, range.count, material});
        entry.triangles = range.count / 3;
        if (!range.count)
            return true;
        entry.range = draw.range;
        mesh.ranges.push_back(std::move(range));
        (close ? close_draws : world_draws).push_back(draw);
        return true;
    }
} // namespace OpenBlood2
