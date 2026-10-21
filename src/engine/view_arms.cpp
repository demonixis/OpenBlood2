#include "engine/view_arms.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
namespace OpenBlood2
{
    namespace
    {
        bool prefixed(const std::string& name, std::string_view prefix)
        {
            if (name.size() < prefix.size())
                return false;
            for (std::size_t i = 0; i < prefix.size(); ++i)
                if (std::tolower(static_cast<unsigned char>(name[i])) != prefix[i])
                    return false;
            return true;
        }
        int child(const Model& model, std::size_t parent, std::string_view prefix)
        {
            for (std::size_t i = parent + 1; i < model.nodes.size(); ++i)
                if (model.nodes[i].parent == static_cast<int>(parent) && prefixed(model.nodes[i].name, prefix))
                    return static_cast<int>(i);
            return -1;
        }
        bool under(const Model& model, std::size_t node, std::size_t root)
        {
            for (int p = static_cast<int>(node); p >= 0; p = model.nodes[p].parent)
                if (static_cast<std::size_t>(p) == root)
                    return true;
            return false;
        }
        // Model-order node of each vertex; empty on a bad node index.
        std::vector<std::uint8_t> vertex_nodes(const Model& model)
        {
            std::array<int, 256> slots;
            slots.fill(-1);
            for (std::size_t i = 0; i < model.nodes.size() && i < slots.size(); ++i)
                if (model.nodes[i].index < slots.size())
                    slots[model.nodes[i].index] = static_cast<int>(i);
            std::vector<std::uint8_t> out;
            out.reserve(model.vertices.size());
            for (const auto& vertex : model.vertices)
            {
                if (slots[vertex.node] < 0)
                    return {};
                out.push_back(static_cast<std::uint8_t>(slots[vertex.node]));
            }
            return out;
        }
        // Rest grip of the two-handed models, measured on side views of their rest pose: the
        // pistol grip or carry handle where the unmodelled trigger hand would close.
        struct AuditedGrip
        {
            const char* model;
            DVector anchor;
        };
        constexpr AuditedGrip audited_grips[]{
            {"m16_pv", {0, -.39f, -.26f}},           {"lh_m16_pv", {0, -.39f, -.26f}},
            {"sniperrifle_pv", {0, -.67f, -1.45f}},  {"minigun_pv", {0, -.70f, -1.38f}},
            {"teslarod_pv", {0, -.46f, -1.10f}},     {"lh_teslarod_pv", {0, -.46f, -1.10f}},
            {"deathray_pv", {0, -.54f, -1.29f}},     {"howitzer_pv", {0, -.65f, -2.56f}},
            {"napalmcannon_pv", {0, -.39f, -1.57f}}, {"lh_napalmcannon_pv", {0, -.39f, -1.57f}},
            {"singularity_pv", {0, -.23f, -1.49f}},
        };
        DRotation conjugate(DRotation q)
        {
            return {{-q.m_Vec.x, -q.m_Vec.y, -q.m_Vec.z}, q.m_Spin};
        }
    } // namespace
    bool rig_view_arms(const Model& model, std::string_view model_path, bool left, ViewArmRig& out)
    {
        out = {};
        auto rest = animation_index(model, "static_model");
        for (const char* name : {"idle", "fire"})
            if (rest == no_animation)
                rest = animation_index(model, name);
        if (rest == no_animation)
            rest = 0;
        std::vector<NodePose> pose;
        std::vector<DVector> vertices;
        const auto nodes = vertex_nodes(model);
        if (!sample_pose(model, rest, 0, pose) || !sample_vertices(model, rest, 0, vertices) ||
            nodes.size() != vertices.size())
            return false;
        for (std::size_t i = 0; i < model.nodes.size(); ++i)
        {
            if (!prefixed(model.nodes[i].name, "lu_arm"))
                continue;
            const int lower = child(model, i, "ll_arm");
            const int wrist = lower < 0 ? -1 : child(model, static_cast<std::size_t>(lower), "d_null");
            if (wrist < 0)
                continue;
            out.chains.push_back({static_cast<std::uint16_t>(i), static_cast<std::uint16_t>(lower),
                                  static_cast<std::uint16_t>(wrist), pose[i].position.x > 0});
            if (out.held < 0 && out.chains.back().right != left)
                out.held = static_cast<int>(out.chains.size()) - 1;
        }
        if (out.held >= 0)
        {
            // The centre of the hand meshes' rest vertices (the wrist's children, not what they
            // hold): the palm around the grip.
            const auto wrist = out.chains[out.held].wrist;
            DVector lo{}, hi{};
            bool any = false;
            for (std::size_t v = 0; v < vertices.size(); ++v)
            {
                if (model.nodes[nodes[v]].parent != wrist)
                    continue;
                const auto& p = vertices[v];
                lo = any ? DVector{std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)} : p;
                hi = any ? DVector{std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)} : p;
                any = true;
            }
            out.grip = ViewGrip::Hand;
            out.anchor = any ? DVector{(lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2} : pose[wrist].position;
            return true;
        }
        auto name = std::string(model_path.substr(model_path.find_last_of("\\/") + 1));
        name = name.substr(0, name.find('.'));
        for (auto& c : name)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (const auto& audited : audited_grips)
            if (name == audited.model)
            {
                out.grip = ViewGrip::Audited;
                out.anchor = audited.anchor;
                return true;
            }
        out.anchor = {(model.min.x + model.max.x) / 2, (model.min.y + model.max.y) / 2,
                      (model.min.z + model.max.z) / 2};
        return true;
    }
    bool pose_view_arms(const Model& model, const AnimationTrack& track, const ViewArmRig& rig,
                        const ViewArmPlacement& placement, DVector offset, ViewArmPose& out)
    {
        out = {};
        std::vector<DVector> vertices;
        const auto nodes = vertex_nodes(model);
        if (!(placement.scale > 0) || !std::isfinite(placement.scale) ||
            !sample_vertices(model, track.animation, track.time, vertices) || nodes.size() != vertices.size())
            return false;
        const auto rotation = placement.grip.rotation;
        const auto anchor = rotate_vector(
            rotation, {rig.anchor.x * placement.scale, rig.anchor.y * placement.scale, rig.anchor.z * placement.scale});
        out.model = {{placement.grip.position.x - anchor.x, placement.grip.position.y - anchor.y,
                      placement.grip.position.z - anchor.z},
                     rotation};
        // The original camera sat at -offset in model space with a 90-degree field of view. Framed
        // out means beyond that view cone widened by 1.5 units (about 25 cm): parts merely cut by
        // the frame edge or the near plane (stocks, grips, forearms) stay.
        auto off_screen = [&](DVector p)
        {
            const DVector e{p.x + offset.x, p.y + offset.y, p.z + offset.z};
            return std::hypot(e.x, e.y) > 1.5f + std::max(e.z, 0.f);
        };
        out.hidden.assign(model.nodes.size(), 0);
        std::vector<int> owner(model.nodes.size(), -1);
        for (std::size_t c = 0; c < rig.chains.size(); ++c)
            for (std::size_t i = 0; i < model.nodes.size(); ++i)
                if (under(model, i, rig.chains[c].upper))
                    owner[i] = static_cast<int>(c);
        // Per node: has vertices, and whether any is on screen.
        std::vector<std::uint8_t> drawn(model.nodes.size()), seen(model.nodes.size());
        for (std::size_t v = 0; v < vertices.size(); ++v)
        {
            drawn[nodes[v]] = 1;
            seen[nodes[v]] |= !off_screen(vertices[v]);
        }
        for (std::size_t c = 0; c < rig.chains.size(); ++c)
        {
            const auto& chain = rig.chains[c];
            bool visible = static_cast<int>(c) == rig.held;
            for (std::size_t i = 0; i < model.nodes.size() && !visible; ++i)
                visible = seen[i] && under(model, i, chain.wrist);
            if (!visible)
            {
                for (std::size_t i = 0; i < model.nodes.size(); ++i)
                    if (under(model, i, chain.upper))
                        out.hidden[i] = 1;
                continue;
            }
            const auto side = chain.right ? 1 : 0;
            const auto inverse = conjugate(rotation);
            const auto from = placement.shoulders[side];
            const auto shoulder = rotate_vector(
                inverse, {from.x - out.model.position.x, from.y - out.model.position.y, from.z - out.model.position.z});
            out.targets.push_back(
                {chain.upper,
                 chain.lower,
                 chain.wrist,
                 {shoulder.x / placement.scale, shoulder.y / placement.scale, shoulder.z / placement.scale},
                 rotate_vector(inverse, placement.poles[side])});
        }
        for (std::size_t i = 0; i < model.nodes.size(); ++i)
            if (owner[i] < 0 && drawn[i] && !seen[i])
                out.hidden[i] = 1;
        return true;
    }
} // namespace OpenBlood2
