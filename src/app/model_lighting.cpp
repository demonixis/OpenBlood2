#include "app/model_lighting.h"
#include "engine/collision.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cctype>
#include <charconv>

namespace OpenBlood2
{
    namespace
    {
        // GetServerConVarValueFloat, then the client's (int) truncation into "ModelAdd %d %d %d".
        float console_channel(const GameSession& game, const char* name)
        {
            const auto& vars = game.server.variables();
            const auto found = vars.find(name);
            if (found == vars.end())
                return 0;
            const auto& text = found->second.text;
            float value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value) ||
                std::abs(value) > 1e6f)
                return 0;
            return static_cast<float>(static_cast<int>(value));
        }
        DVector clamp_light(DVector v)
        {
            return {std::clamp(v.x, 0.f, 1.f), std::clamp(v.y, 0.f, 1.f), std::clamp(v.z, 0.f, 1.f)};
        }
        bool overlaps(const std::array<DVector, 2>& box, DVector min, DVector max)
        {
            return box[0].x <= max.x && box[1].x >= min.x && box[0].y <= max.y && box[1].y >= min.y &&
                   box[0].z <= max.z && box[1].z >= min.z;
        }
    } // namespace
    bool world_info_ambient(std::string_view info, DVector& ambient)
    {
        // Semicolon-separated "Key values" records, as in the original world info string.
        while (!info.empty())
        {
            const auto end = std::min(info.find(';'), info.size());
            auto record = info.substr(0, end);
            info.remove_prefix(std::min(end + 1, info.size()));
            while (!record.empty() && std::isspace(static_cast<unsigned char>(record.front())))
                record.remove_prefix(1);
            constexpr std::string_view key = "AmbientLight";
            if (record.size() <= key.size() || record.substr(0, key.size()) != key ||
                !std::isspace(static_cast<unsigned char>(record[key.size()])))
                continue;
            record.remove_prefix(key.size());
            float channels[3]{};
            for (auto& channel : channels)
            {
                while (!record.empty() && std::isspace(static_cast<unsigned char>(record.front())))
                    record.remove_prefix(1);
                const auto [next, error] = std::from_chars(record.data(), record.data() + record.size(), channel);
                if (error != std::errc{} || !std::isfinite(channel) || channel < 0 || channel > 255)
                    return false;
                record.remove_prefix(static_cast<std::size_t>(next - record.data()));
            }
            ambient = {channels[0], channels[1], channels[2]};
            return true;
        }
        return false;
    }
    void ModelLighting::clear()
    {
        shade_.clear();
        samples_.clear();
        next_.clear();
        poses_.clear();
        ambient_ = {};
        floor_ = 0;
        stats = {};
    }
    bool ModelLighting::build(const WorldMesh& mesh, const World& world, std::string& error)
    {
        clear();
        if (!shade_.build(mesh, error))
            return false;
        DVector ambient{};
        world_info_ambient(world.info, ambient); // Absent or malformed: no ambient.
        stats.world_ambient = ambient;
        ambient_ = vector_scale(ambient, 1.f / 255);
        floor_ = world.min.y - 1;
        poses_.resize(mesh.ranges.size());
        return true;
    }
    void ModelLighting::sample(DVector center, std::span<const WorldDraw> draws, Sample& out)
    {
        out.center = center;
        const DVector bottom{center.x, std::min(center.y, floor_), center.z};
        float fraction = 1;
        if (!shade_.trace(center, bottom, draws, out.light, fraction))
        {
            out.light = ambient_;
            ++stats.ambient;
        }
        const float hit = center.y + (bottom.y - center.y) * fraction;
        out.min = {center.x, hit, center.z};
        out.max = center;
        ++stats.sampled;
    }
    bool ModelLighting::apply(const GameSession& game, const WorldMesh& mesh, std::span<const WorldDraw> draws,
                              ModelView& scene, std::string& error)
    {
        error.clear();
        const auto shade = shade_.stats;
        stats.models = stats.sampled = stats.reused = stats.ambient = stats.unlit = 0;
        stats.model_add = {console_channel(game, "ModelAddR"), console_channel(game, "ModelAddG"),
                           console_channel(game, "ModelAddB")};
        stats.model_dir_add = {console_channel(game, "ModelDirAddR"), console_channel(game, "ModelDirAddG"),
                               console_channel(game, "ModelDirAddB")};
        const auto add = vector_scale(stats.model_add, 1.f / 255);
        // World boxes of ranges that moved, appeared or disappeared since the last apply:
        // a cached segment crossing one is sampled again.
        moved_.clear();
        bool everything = poses_.size() != mesh.ranges.size();
        if (everything)
            poses_.assign(mesh.ranges.size(), {});
        std::vector<bool> drawn(mesh.ranges.size());
        auto moved = [&](std::uint32_t range, const Pose& pose)
        {
            const auto& r = mesh.ranges[range];
            if (!r.bounded)
            {
                everything = true;
                return;
            }
            std::array<DVector, 2> box{};
            for (int corner = 0; corner < 8; ++corner)
            {
                const DVector local{corner & 1 ? r.max.x : r.min.x, corner & 2 ? r.max.y : r.min.y,
                                    corner & 4 ? r.max.z : r.min.z};
                const auto p = vector_add(pose.position, rotate_vector(local, pose.rotation));
                if (!corner)
                    box = {p, p};
                box[0] = {std::min(box[0].x, p.x), std::min(box[0].y, p.y), std::min(box[0].z, p.z)};
                box[1] = {std::max(box[1].x, p.x), std::max(box[1].y, p.y), std::max(box[1].z, p.z)};
            }
            moved_.push_back(box);
        };
        for (const auto& draw : draws)
        {
            if (draw.range >= mesh.ranges.size())
                continue;
            drawn[draw.range] = true;
            auto& last = poses_[draw.range];
            const Pose now{true, draw.position, draw.rotation};
            const bool same = last.drawn && last.position.x == now.position.x && last.position.y == now.position.y &&
                              last.position.z == now.position.z && last.rotation.m_Vec.x == now.rotation.m_Vec.x &&
                              last.rotation.m_Vec.y == now.rotation.m_Vec.y &&
                              last.rotation.m_Vec.z == now.rotation.m_Vec.z &&
                              last.rotation.m_Spin == now.rotation.m_Spin;
            if (same)
                continue;
            if (last.drawn)
                moved(draw.range, last);
            moved(draw.range, now);
            last = now;
        }
        for (std::size_t range = 0; range < poses_.size(); ++range)
            if (poses_[range].drawn && !drawn[range])
            {
                moved(static_cast<std::uint32_t>(range), poses_[range]);
                poses_[range].drawn = false;
            }
        next_.clear();
        const auto& view = game.client_shell.view;
        for (std::size_t i = 0; i < scene.entries.size(); ++i)
        {
            const auto& entry = scene.entries[i];
            if (entry.range >= scene.instances.size())
                continue;
            const auto& instance = scene.instances[entry.range];
            const ObjectRecord* record = entry.object ? game.objects.get(entry.object) : nullptr;
            ++stats.models;
            if (record && (record->state.m_Flags & FLAG_NOLIGHT))
            {
                ++stats.unlit;
                scene.set_light(i, clamp_light(record->color));
                continue;
            }
            const auto center = entry.close ? view.position : instance.center;
            if (!physics_vector(center))
            {
                error = "Invalid model lighting center";
                return false;
            }
            Sample current;
            const std::uint64_t key = entry.object ? (std::uint64_t(entry.object.slot) << 32) | entry.object.generation
                                                   : UINT64_MAX - i; // View models are not cached.
            const auto found = entry.object ? samples_.find(key) : samples_.end();
            bool reuse = !everything && found != samples_.end() && found->second.center.x == center.x &&
                         found->second.center.y == center.y && found->second.center.z == center.z;
            if (reuse)
                for (const auto& box : moved_)
                    if (overlaps(box, found->second.min, found->second.max))
                    {
                        reuse = false;
                        break;
                    }
            if (reuse)
            {
                current = found->second;
                ++stats.reused;
            }
            else
                sample(center, draws, current);
            if (entry.object)
                next_[key] = current;
            auto light = vector_add(current.light, add);
            if (record)
                light = vector_add(light, record->color);
            scene.set_light(i, clamp_light(light));
        }
        samples_.swap(next_);
        stats.shade = shade_.stats;
        stats.shade.traces -= shade.traces;
        stats.shade.hits -= shade.hits;
        stats.shade.node_tests -= shade.node_tests;
        stats.shade.triangle_tests -= shade.triangle_tests;
        return true;
    }
} // namespace OpenBlood2
