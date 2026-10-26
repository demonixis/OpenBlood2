#include "engine/visibility.h"
#include <limits>

namespace OpenBlood2
{
    namespace
    {
        constexpr std::size_t candidate_limit = std::size_t(1) << 22;
        double signed_distance(const Plane& plane, DVector p)
        {
            return double(plane.normal.x) * p.x + double(plane.normal.y) * p.y + double(plane.normal.z) * p.z -
                   plane.distance;
        }
    } // namespace
    bool WorldVisibility::build(const WorldModel& model, std::string& error)
    {
        clear();
        error.clear();
        if (model.bsp_nodes.empty() || model.root_node < 0 || model.visibility_lists.empty())
            return true;
        auto fail = [&](std::string message)
        {
            clear();
            error = "Leaf visibility: " + message;
            return false;
        };
        // Retail builds the tree once at load (preorder, back side first) and points each BSP
        // node at its tree node; a node reached twice would not be a tree.
        std::vector<bool> seen(model.bsp_nodes.size());
        struct Pending
        {
            std::int32_t bsp, parent;
            int side;
        };
        std::vector<Pending> pending{{model.root_node, -1, 0}};
        while (!pending.empty())
        {
            const auto next = pending.back();
            pending.pop_back();
            std::int32_t n = next.bsp, created = -1;
            while (n >= 0)
            {
                if (seen[n])
                    return fail("BSP node " + std::to_string(n) + " is reached twice");
                seen[n] = true;
                const auto& node = model.bsp_nodes[n];
                if (model.surfaces[model.polygons[node.polygon].surface].visibility_skip)
                {
                    ++stats_.skipped_nodes; // With its whole subtree, as the retail tree builder does.
                    break;
                }
                if (node.leaf == UINT16_MAX && node.sides[1] != BspNode::inside && node.sides[0] == BspNode::outside)
                {
                    ++stats_.collapsed_nodes;
                    n = node.sides[1];
                    continue;
                }
                created = static_cast<std::int32_t>(tree_.size());
                tree_.push_back({static_cast<std::uint32_t>(n)});
                pending.push_back({node.sides[1], created, 1});
                pending.push_back({node.sides[0], created, 0});
                break;
            }
            if (next.parent >= 0)
                tree_[next.parent].children[next.side] = created;
        }
        if (tree_.empty())
            return true;
        planes_ = model.planes;
        nodes_ = model.bsp_nodes;
        root_node_ = model.root_node;
        bits_ = model.visibility;
        for (const auto& list : model.visibility_lists)
            lists_.push_back({list.offset, list.size});
        leaf_list_.assign(model.bsp_leaves.size(), UINT32_MAX);
        for (std::size_t i = 0; i < model.bsp_leaves.size(); ++i)
        {
            const auto& leaf = model.bsp_leaves[i];
            if (leaf.lists)
                leaf_list_[i] = leaf.first_list;
            if (leaf.shared == UINT16_MAX && leaf.lists > 1)
                stats_.conditional_lists += leaf.lists - 1;
        }
        stats_.tree_nodes = tree_.size();
        stats_.leaves = model.bsp_leaves.size();
        stats_.lists = lists_.size();
        for (std::size_t i = 0; i < lists_.size(); ++i)
        {
            const auto bits = walk(lists_[i], {}, nullptr);
            if (bits == SIZE_MAX)
                return fail("list " + std::to_string(i) + " ends before its tree walk");
            stats_.list_bytes += lists_[i].size;
            stats_.trailing_bytes += lists_[i].size - (bits + 7) / 8;
        }
        return true;
    }
    std::uint32_t WorldVisibility::find_leaf(DVector point) const
    {
        std::uint32_t leaf = UINT32_MAX;
        std::int32_t n = root_node_;
        for (std::size_t steps = 0; n >= 0 && steps <= nodes_.size(); ++steps)
        {
            const auto& node = nodes_[n];
            if (node.leaf != UINT16_MAX)
                leaf = node.leaf;
            n = node.sides[signed_distance(planes_[node.plane], point) < -0.0001f ? 0 : 1];
        }
        return n >= 0 ? UINT32_MAX : leaf; // A cycle never terminates the original descent.
    }
    bool WorldVisibility::query(std::uint32_t leaf, std::span<const VisibilitySphere> spheres,
                                std::vector<bool>& visible) const
    {
        if (!available() || leaf >= leaf_list_.size() || spheres.size() > candidate_limit)
            return false;
        std::vector<bool> out(spheres.size());
        if (leaf_list_[leaf] != UINT32_MAX && walk(lists_[leaf_list_[leaf]], spheres, &out) == SIZE_MAX)
            return false;
        visible = std::move(out);
        return true;
    }
    std::int32_t WorldVisibility::attach(const VisibilitySphere& sphere) const
    {
        std::int32_t x = 0;
        for (std::size_t steps = 0; steps < tree_.size(); ++steps)
        {
            const auto& t = tree_[x];
            const double d = signed_distance(planes_[nodes_[t.bsp].plane], sphere.center);
            std::int32_t next;
            if (d > sphere.radius)
            {
                if ((next = t.children[1]) < 0)
                    return x; // In front of the last node: the inside sentinel keeps it here.
            }
            else if (d < -sphere.radius)
            {
                if ((next = t.children[0]) < 0)
                    return -1; // Behind into the outside sentinel: not linked into the tree.
            }
            else
                return x;
            x = next;
        }
        return -1;
    }
    std::size_t WorldVisibility::walk(const List& list, std::span<const VisibilitySphere> spheres,
                                      std::vector<bool>* visible) const
    {
        const std::size_t limit = std::size_t(list.size) * 8;
        std::size_t bit = 0;
        bool overrun = false;
        auto read = [&]
        {
            if (bit >= limit)
            {
                overrun = true;
                return false;
            }
            const bool set = bits_[list.offset + bit / 8] >> (bit % 8) & 1;
            ++bit;
            return set;
        };
        // Spheres linked at each tree node (CSR), as the retail insertion places objects.
        std::vector<std::uint32_t> first, linked;
        if (visible)
        {
            std::vector<std::int32_t> node(spheres.size());
            first.assign(tree_.size() + 1, 0);
            for (std::size_t i = 0; i < spheres.size(); ++i)
                if ((node[i] = attach(spheres[i])) >= 0)
                    ++first[node[i] + 1];
            for (std::size_t k = 1; k < first.size(); ++k)
                first[k] += first[k - 1];
            linked.resize(first.back());
            auto cursor = first;
            for (std::uint32_t i = 0; i < spheres.size(); ++i)
                if (node[i] >= 0)
                    linked[cursor[node[i]]++] = i;
        }
        // Candidate indices. The top frame owns [begin, pool.size()); a pop releases from mark on.
        std::vector<std::uint32_t> pool;
        struct Frame
        {
            std::int32_t node;
            std::size_t begin, mark;
            bool front;
        };
        // Entering a node adds the spheres linked there (retail's per-node object callback).
        auto enter = [&](std::int32_t node)
        {
            if (visible)
                for (auto k = first[node]; k < first[node + 1]; ++k)
                    if (!(*visible)[linked[k]])
                        pool.push_back(linked[k]);
        };
        // Retail drops already-visible objects when it passes a list to a child.
        auto keep = [&](std::uint32_t i, const Plane& plane, bool front)
        {
            if ((*visible)[i])
                return false;
            const double d = signed_distance(plane, spheres[i].center);
            return front ? d > -spheres[i].radius : d < spheres[i].radius;
        };
        auto narrow = [&](const Frame& f, const Plane& plane, bool front)
        {
            auto out = f.begin;
            if (visible)
                for (auto k = f.begin; k < pool.size(); ++k)
                    if (keep(pool[k], plane, front))
                        pool[out++] = pool[k];
            pool.resize(out);
        };
        std::vector<Frame> stack{{0, 0, 0, false}};
        enter(0);
        while (!stack.empty())
        {
            if (overrun)
                return SIZE_MAX;
            auto& f = stack.back();
            const auto& t = tree_[f.node];
            const auto& node = nodes_[t.bsp];
            const auto& plane = planes_[node.plane];
            if (!f.front)
            {
                if (node.leaf != UINT16_MAX && read() && visible)
                    for (auto k = f.begin; k < pool.size(); ++k)
                        (*visible)[pool[k]] = true;
                if (t.children[0] >= 0 && read())
                {
                    const auto child = t.children[0];
                    if (t.children[1] < 0)
                    {
                        narrow(f, plane, false); // Tail descent: no front side to come back to.
                        f.node = child;
                        enter(child);
                        continue;
                    }
                    f.front = true;
                    const auto mark = pool.size();
                    if (visible)
                        for (auto k = f.begin; k < mark; ++k)
                            if (keep(pool[k], plane, false))
                                pool.push_back(pool[k]);
                    if (pool.size() > candidate_limit || stack.size() > tree_.size())
                        return SIZE_MAX;
                    stack.push_back({child, mark, mark, false});
                    enter(child);
                    continue;
                }
                f.front = true;
            }
            if (t.children[1] >= 0 && read())
            {
                const auto child = t.children[1];
                narrow(f, plane, true);
                f.node = child;
                f.front = false;
                enter(child);
                continue;
            }
            pool.resize(f.mark);
            stack.pop_back();
        }
        return overrun ? SIZE_MAX : bit;
    }
} // namespace OpenBlood2
