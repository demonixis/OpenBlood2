#include "engine/model_view.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    void ModelView::clear()
    {
        mesh = {};
        mesh.materials.push_back({"", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}, {}}});
        mesh.texture_bytes = 4;
        materials_.clear();
        prepared_.clear();
        layouts_.clear();
        cached_vertices_ = 0;
        sampled_.clear();
        begin_frame();
        ++revision;
    }
    void ModelView::begin_frame()
    {
        for (auto it = prepared_.begin(); it != prepared_.end();)
        {
            if (it->second.owner.expired())
            {
                cached_vertices_ -= it->second.vertices.capacity();
                it = prepared_.erase(it);
            }
            else
                ++it;
        }
        for (auto it = layouts_.begin(); it != layouts_.end();)
            it = it->second.owner.expired() ? layouts_.erase(it) : std::next(it);
        instances.clear();
        palette.clear();
        deformation.clear();
        preparation = {};
        preparation.cached_assets = prepared_.size();
        preparation.cached_bytes = cached_vertices_ * sizeof(WorldVertex);
        mesh.blended = 0;
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
        return add_impl(model, nullptr, track, skin, pose, scale, visible, close, object, label, resources, error,
                        hidden_nodes, alpha, require_skin);
    }
    bool ModelView::add(const std::shared_ptr<const Model>& model, const AnimationTrack& track, std::string_view skin,
                        NodePose pose, DVector scale, bool visible, bool close, ObjectId object, std::string_view label,
                        const Resources& resources, std::string& error, std::span<const std::uint8_t> hidden_nodes,
                        float alpha, bool require_skin)
    {
        return add_impl(model.get(), &model, track, skin, pose, scale, visible, close, object, label, resources, error,
                        hidden_nodes, alpha, require_skin);
    }
    bool ModelView::add_impl(const Model* model, const std::shared_ptr<const Model>* owner, const AnimationTrack& track,
                             std::string_view skin, NodePose pose, DVector scale, bool visible, bool close,
                             ObjectId object, std::string_view label, const Resources& resources, std::string& error,
                             std::span<const std::uint8_t> hidden_nodes, float alpha, bool require_skin)
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
            model->faces.size() > 200000 || (!gpu_skinning && model->faces.size() * 3 > 1000000 - mesh.vertices.size()))
        {
            return fail("Invalid model scale/geometry budget");
        }
        if (gpu_skinning)
        {
            if (!owner)
            {
                entry.error = "GPU skinning requires a shared immutable model";
                return true;
            }
            if (!add_skinned(*model, *owner, track, pose, scale, close, alpha, material, hidden_nodes, entry, error))
            {
                begin_frame();
                return false;
            }
            return true;
        }
        WorldDraw draw{static_cast<std::uint32_t>(mesh.ranges.size()), pose.position, pose.rotation, alpha};
        WorldViewUniform uniform;
        if (!world_view_uniform({}, {}, 1, draw, uniform))
        {
            return fail("Invalid model presentation transform");
        }
        WorldRange range;
        range.first = static_cast<std::uint32_t>(mesh.vertices.size());
        range.name = entry.label;
        // Clamping only past the final key preserves exactly the sampler's terminal pose.
        double time = track.time;
        if (std::isfinite(time) && time >= 0 && track.animation < model->animations.size() &&
            !model->animations[track.animation].keys.empty())
            time = std::min(time, double(model->animations[track.animation].keys.back().time));
        auto found = prepared_.find(model);
        if (found != prepared_.end() && owner &&
            (found->second.owner.owner_before(*owner) || owner->owner_before(found->second.owner)))
        {
            cached_vertices_ -= found->second.vertices.capacity();
            prepared_.erase(found);
            found = prepared_.end();
            preparation.cached_assets = prepared_.size();
            preparation.cached_bytes = cached_vertices_ * sizeof(WorldVertex);
        }
        const auto* cached = found == prepared_.end() ? nullptr : &found->second;
        const bool reuse = owner && cached && cached->animation == track.animation && cached->time == time &&
                           cached->scale.x == scale.x && cached->scale.y == scale.y && cached->scale.z == scale.z &&
                           cached->fallback == (material == 0) &&
                           std::equal(hidden_nodes.begin(), hidden_nodes.end(), cached->hidden_nodes.begin(),
                                      cached->hidden_nodes.end());
        if (reuse)
        {
            mesh.vertices.insert(mesh.vertices.end(), cached->vertices.begin(), cached->vertices.end());
            range.min = cached->min;
            range.max = cached->max;
            range.bounded = cached->bounded;
            entry.hidden_triangles = cached->hidden_triangles;
            entry.reused_geometry = true;
            ++preparation.reused;
            preparation.reused_vertices += cached->vertices.size();
        }
        else
        {
            ++preparation.sampled;
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
            for (const auto& vertex : model->vertices)
                if (node_slots[vertex.node] < 0)
                    return fail("Invalid model vertex node index");
            if (!sample_vertices(*model, track.animation, track.time, sampled_))
            {
                entry.error = "Model animation pose is unavailable";
                return true;
            }
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
            // Conservative bounds from the posed vertex set (fewer than face corners); hidden
            // nodes can only make them larger than the drawn triangles, never smaller.
            if (range.count && !sampled_.empty())
            {
                range.min = range.max = sampled_.front();
                for (const auto& p : sampled_)
                {
                    range.min = {std::min(range.min.x, p.x), std::min(range.min.y, p.y), std::min(range.min.z, p.z)};
                    range.max = {std::max(range.max.x, p.x), std::max(range.max.y, p.y), std::max(range.max.z, p.z)};
                }
                range.min = {range.min.x * scale.x, range.min.y * scale.y, range.min.z * scale.z};
                range.max = {range.max.x * scale.x, range.max.y * scale.y, range.max.z * scale.z};
                range.bounded = physics_vector(range.min) && physics_vector(range.max);
            }
            // Cache admission is optional: exceeding its budget never drops a valid instance.
            const auto count = mesh.vertices.size() - range.first;
            const auto previous = cached ? cached->vertices.capacity() : 0;
            if (owner && (cached || prepared_.size() < cache_asset_limit) &&
                std::max(count, previous) <= cache_vertex_limit - (cached_vertices_ - previous))
            {
                auto& next = prepared_[model];
                next.owner = *owner;
                next.animation = track.animation;
                next.time = time;
                next.scale = scale;
                next.fallback = material == 0;
                next.hidden_nodes.assign(hidden_nodes.begin(), hidden_nodes.end());
                next.hidden_triangles = entry.hidden_triangles;
                next.min = range.min;
                next.max = range.max;
                next.bounded = range.bounded;
                // Reuse storage for changing poses; avoid geometric capacity growth beyond the cache budget.
                if (count > next.vertices.capacity())
                    next.vertices = std::vector<WorldVertex>(mesh.vertices.begin() + range.first, mesh.vertices.end());
                else
                    next.vertices.assign(mesh.vertices.begin() + range.first, mesh.vertices.end());
                cached_vertices_ = cached_vertices_ - previous + next.vertices.capacity();
            }
            else
                ++preparation.uncached;
        }
        preparation.cached_assets = prepared_.size();
        preparation.cached_bytes = cached_vertices_ * sizeof(WorldVertex);
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
    bool build_skinned_vertices(const Model& model, std::vector<SkinnedVertex>& out, std::string& error)
    {
        out.clear();
        std::array<int, 256> slots;
        slots.fill(-1);
        if (model.nodes.size() > 256 || model.vertices.size() > 65536 || model.faces.size() > 200000)
        {
            error = "Invalid model node/geometry budget";
            return false;
        }
        for (std::size_t i = 0; i < model.nodes.size(); ++i)
        {
            const auto index = model.nodes[i].index;
            if (index >= slots.size() || slots[index] != -1)
            {
                error = "Invalid model node index";
                return false;
            }
            slots[index] = static_cast<int>(i);
        }
        // Repeated packed indices keep file write order: the last slot wins, as on the CPU path.
        std::vector<std::uint32_t> deform(model.vertices.size(), UINT32_MAX);
        std::uint32_t slot = 0;
        for (const auto& node : model.nodes)
            for (auto vertex : node.deformed_vertices)
            {
                if (vertex >= model.vertices.size())
                {
                    error = "Invalid model deformation vertex";
                    return false;
                }
                deform[vertex] = slot++;
            }
        out.reserve(model.faces.size() * 3);
        for (const auto& face : model.faces)
        {
            std::array<std::uint8_t, 4> nodes{};
            for (unsigned corner = 0; corner < 3; ++corner)
            {
                const auto vertex = face.vertices[corner];
                if (vertex >= model.vertices.size() || slots[model.vertices[vertex].node] < 0)
                {
                    error = "Invalid model face node index";
                    return false;
                }
                nodes[corner + 1] = static_cast<std::uint8_t>(slots[model.vertices[vertex].node]);
            }
            for (unsigned corner = 0; corner < 3; ++corner)
            {
                const auto vertex = face.vertices[corner];
                const auto uv = face.uv[corner];
                const auto& source = model.vertices[vertex];
                if (!physics_vector(source.position) || !std::isfinite(uv[0]) || !std::isfinite(uv[1]) ||
                    std::abs(uv[0]) > 1e6f || std::abs(uv[1]) > 1e6f)
                {
                    error = "Invalid model vertex/UV";
                    return false;
                }
                nodes[0] = nodes[corner + 1];
                out.push_back({source.position, uv, nodes, deform[vertex], source.normal});
            }
        }
        return true;
    }
    bool ModelView::add_skinned(const Model& model, const std::shared_ptr<const Model>& owner,
                                const AnimationTrack& track, NodePose pose, DVector scale, bool close, float alpha,
                                std::uint32_t material, std::span<const std::uint8_t> hidden_nodes,
                                ModelViewEntry& entry, std::string& error)
    {
        auto fail = [&](const char* reason)
        {
            error = reason;
            return false;
        };
        auto layout = layouts_.find(&model);
        if (layout != layouts_.end() &&
            (layout->second.owner.owner_before(owner) || owner.owner_before(layout->second.owner)))
        {
            layouts_.erase(layout);
            layout = layouts_.end();
        }
        if (layout == layouts_.end())
        {
            // Validated once per immutable asset; the renderer builds the same corner stream.
            std::vector<SkinnedVertex> corners;
            if (!build_skinned_vertices(model, corners, error))
                return false;
            SkinLayout next;
            next.owner = owner;
            std::array<int, 256> slots;
            slots.fill(-1);
            for (std::size_t i = 0; i < model.nodes.size(); ++i)
                slots[model.nodes[i].index] = static_cast<int>(i);
            next.node_bounds.resize(model.nodes.size());
            next.bounded.assign(model.nodes.size(), false);
            auto include = [&](std::size_t node, DVector p)
            {
                auto& box = next.node_bounds[node];
                if (!next.bounded[node])
                    box = {p, p};
                box[0] = {std::min(box[0].x, p.x), std::min(box[0].y, p.y), std::min(box[0].z, p.z)};
                box[1] = {std::max(box[1].x, p.x), std::max(box[1].y, p.y), std::max(box[1].z, p.z)};
                next.bounded[node] = true;
            };
            next.vertex_slots.reserve(model.vertices.size());
            for (const auto& vertex : model.vertices)
            {
                const auto node = static_cast<std::size_t>(slots[vertex.node]);
                next.vertex_slots.push_back(static_cast<std::uint8_t>(node));
                include(node, vertex.position);
            }
            // Packed components span 0..255 in every clip; include both extremes.
            for (const auto& clip : model.animations)
                for (std::size_t i = 0; i < model.nodes.size() && i < clip.nodes.size(); ++i)
                    if (!model.nodes[i].deformed_vertices.empty())
                    {
                        const auto& data = clip.nodes[i];
                        include(i, data.translation);
                        include(i, {data.translation.x + 255 * data.scale.x, data.translation.y + 255 * data.scale.y,
                                    data.translation.z + 255 * data.scale.z});
                    }
            layout = layouts_.emplace(&model, std::move(next)).first;
        }
        const auto& info = layout->second;
        if (!sample_skin(model, track.animation, track.time, pose_, deformed_))
        {
            entry.error = "Model animation pose is unavailable";
            return true;
        }
        if (pose_.size() * 2 > palette_limit - palette.size() ||
            deformed_.size() > deformation_limit - deformation.size())
            return fail("Model palette/deformation budget exceeded");
        for (const auto& local : deformed_)
            if (!physics_vector(local))
                return fail("Invalid model vertex/UV");
        if (!hidden_nodes.empty())
            for (const auto& face : model.faces)
            {
                bool hidden = false;
                for (auto vertex : face.vertices)
                    hidden |= hidden_nodes[info.vertex_slots[vertex]] != 0;
                entry.hidden_triangles += hidden;
            }
        entry.triangles = model.faces.size() - entry.hidden_triangles;
        ++preparation.sampled;
        if (!entry.triangles)
            return true; // Nothing drawable; the entry remains inspectable, as on the CPU path.
        SkinnedInstance instance;
        instance.model = &model;
        instance.owner = owner;
        instance.material = material;
        instance.palette = static_cast<std::uint32_t>(palette.size());
        instance.deformation = static_cast<std::uint32_t>(deformation.size());
        instance.scale = scale;
        instance.color = material ? DVector{1, 1, 1} : DVector{1, 0, 1};
        bool bounded = false;
        for (std::size_t i = 0; i < pose_.size(); ++i)
        {
            const auto& node = pose_[i];
            if (!physics_vector(node.position))
                return fail("Invalid model vertex/UV");
            const bool hidden = !hidden_nodes.empty() && hidden_nodes[i] != 0;
            palette.push_back(
                {node.rotation.m_Vec.x, node.rotation.m_Vec.y, node.rotation.m_Vec.z, node.rotation.m_Spin});
            palette.push_back({node.position.x, node.position.y, node.position.z, hidden ? 1.f : 0.f});
            if (!info.bounded[i])
                continue;
            // Conservative sphere around the posed node box, scaled into model space.
            const auto& box = info.node_bounds[i];
            const DVector half{(box[1].x - box[0].x) * .5f, (box[1].y - box[0].y) * .5f, (box[1].z - box[0].z) * .5f};
            const auto center = vector_add(
                node.position,
                rotate_vector(DVector{box[0].x + half.x, box[0].y + half.y, box[0].z + half.z}, node.rotation));
            const float radius = std::sqrt(half.x * half.x + half.y * half.y + half.z * half.z);
            const DVector lo{(center.x - radius) * scale.x, (center.y - radius) * scale.y,
                             (center.z - radius) * scale.z};
            const DVector hi{(center.x + radius) * scale.x, (center.y + radius) * scale.y,
                             (center.z + radius) * scale.z};
            instance.min = bounded ? DVector{std::min(instance.min.x, lo.x), std::min(instance.min.y, lo.y),
                                             std::min(instance.min.z, lo.z)}
                                   : lo;
            instance.max = bounded ? DVector{std::max(instance.max.x, hi.x), std::max(instance.max.y, hi.y),
                                             std::max(instance.max.z, hi.z)}
                                   : hi;
            bounded = true;
        }
        for (const auto& local : deformed_)
            deformation.push_back({local.x, local.y, local.z, 0});
        instance.bounded = bounded && physics_vector(instance.min) && physics_vector(instance.max);
        instance.center = pose.position;
        if (instance.bounded)
            instance.center = vector_add(
                pose.position, rotate_vector(vector_scale(vector_add(instance.min, instance.max), .5f), pose.rotation));
        entry.range = static_cast<std::uint32_t>(instances.size());
        instances.push_back(std::move(instance));
        (close ? close_draws : world_draws).push_back({entry.range, pose.position, pose.rotation, alpha});
        return true;
    }
    bool ModelView::set_light(std::size_t index, const ModelLight& light)
    {
        if (!gpu_skinning || index >= entries.size() || entries[index].range >= instances.size())
            return false;
        auto& instance = instances[entries[index].range];
        if (instance.material)
            instance.light = light;
        return true;
    }
    void ModelView::mark_translucent(std::size_t index)
    {
        const auto range = entries[index].range;
        if (range == UINT32_MAX)
            return;
        if (gpu_skinning)
            instances[range].translucent = true;
        else
            mesh.ranges[range].batches.front().translucent = true;
        ++mesh.blended;
    }
} // namespace OpenBlood2
