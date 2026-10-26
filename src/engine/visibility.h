#pragma once
#include "engine/world.h"
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2
{
    // A bounding sphere tested by the retail visibility walk: the object position and
    // |dimensions| + 0.5 (the retail object radius).
    struct VisibilitySphere
    {
        DVector center{};
        float radius = 0;
    };
    struct VisibilityStats
    {
        std::size_t tree_nodes = 0, skipped_nodes = 0, collapsed_nodes = 0, leaves = 0, lists = 0,
                    conditional_lists = 0, list_bytes = 0, trailing_bytes = 0;
    };
    // Retail Server.dll leaf visibility of the root BSP (ServerDE::PingObjects and client object
    // visibility). The leaf lists are bitstreams over a tree mirroring the BSP: nodes whose polygon
    // surface sets Surface::visibility_skip are left out with their subtree, and a node without a
    // leaf whose back side is outside and front side not inside is replaced by its front side.
    // Per node, in depth-first back-then-front order: one bit when it owns a leaf (the leaf is
    // visible), then one bit per tree child (descend). A zero descent bit prunes that subtree.
    // Bit n of a list is byte n/8, mask 1 << n%8. Data is copied; the model may go away.
    class WorldVisibility
    {
      public:
        // Builds the tree and decodes every list once; fails on a malformed tree or a list that
        // needs bits past its own bytes. A model without nodes or lists builds an empty instance.
        bool build(const WorldModel& model, std::string& error);
        void clear()
        {
            *this = {};
        }
        // True when the root BSP has a tree and at least one leaf list.
        bool available() const
        {
            return !tree_.empty() && !lists_.empty();
        }
        // Original descent from the root node: the last leaf met on the way down (side 0 when the
        // signed distance is below -0.0001). UINT32_MAX when no node on the path owns a leaf.
        std::uint32_t find_leaf(DVector point) const;
        // Walks the leaf's unconditional (first) list and sets visible[i] for every sphere that
        // reaches a node whose leaf bit is set. Each sphere is linked where retail inserts the
        // object: descending while entirely behind (d < -r) or in front (d > r) of a node plane,
        // at the first node it straddles or whose front side is not a tree node; one that falls
        // behind into a non-tree side is not linked. Lists are then passed to a child only for
        // spheres touching that side (back: d < r, front: d > -r).
        // Conditional portal lists are not applied (no portal is ever opened). Returns false on
        // an invalid leaf or work bound, leaving visible untouched.
        bool query(std::uint32_t leaf, std::span<const VisibilitySphere> spheres, std::vector<bool>& visible) const;
        const VisibilityStats& stats() const
        {
            return stats_;
        }

      private:
        struct Node
        {
            std::uint32_t bsp = 0;
            std::int32_t children[2]{-1, -1}; // Tree indices; -1 is not a tree node.
        };
        struct List
        {
            std::uint32_t offset = 0, size = 0;
        };
        std::vector<Node> tree_;
        std::vector<Plane> planes_;
        std::vector<BspNode> nodes_;
        std::vector<std::uint32_t> leaf_list_; // Per leaf: index into lists_, UINT32_MAX for none.
        std::vector<List> lists_;
        std::vector<std::uint8_t> bits_;
        std::int32_t root_node_ = BspNode::outside;
        VisibilityStats stats_;
        // Walks one list. With spheres, marks reached ones; returns the bits consumed or SIZE_MAX
        // when the stream ends early or the work bound is hit.
        std::size_t walk(const List& list, std::span<const VisibilitySphere> spheres, std::vector<bool>* visible) const;
        std::int32_t attach(const VisibilitySphere& sphere) const; // Tree node, -1 when not linked.
    };
} // namespace OpenBlood2
