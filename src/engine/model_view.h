#pragma once
#include "engine/world_view.h"
namespace OpenBlood2
{
    struct ModelViewEntry
    {
        ObjectId object;
        std::string label, skin, error;
        bool visible = false, close = false, reused_geometry = false;
        std::size_t triangles = 0;
        std::size_t hidden_nodes = 0, hidden_triangles = 0;
        std::uint32_t range = UINT32_MAX;
        float alpha = 1;
        std::uint64_t client_effect = 0;
        std::uint64_t fragment_effect = 0;
        std::uint64_t laser_effect = 0;
    };
    // Presentation-only snapshot. No original object pointer survives add().
    class ModelView
    {
      public:
        ModelView()
        {
            clear();
        }
        void clear();
        void begin_frame();
        bool add(const Model*, const AnimationTrack&, std::string_view skin, NodePose, DVector scale, bool visible,
                 bool close, ObjectId, std::string_view label, const Resources&, std::string&,
                 std::span<const std::uint8_t> hidden_nodes = {}, float alpha = 1, bool require_skin = false);
        // Only immutable, shared assets may reuse prepared geometry. Borrowed models are always sampled.
        bool add(const std::shared_ptr<const Model>&, const AnimationTrack&, std::string_view skin, NodePose,
                 DVector scale, bool visible, bool close, ObjectId, std::string_view label, const Resources&,
                 std::string&, std::span<const std::uint8_t> hidden_nodes = {}, float alpha = 1,
                 bool require_skin = false);
        struct PreparationStats
        {
            std::size_t sampled = 0, reused = 0, uncached = 0, reused_vertices = 0;
            std::size_t cached_assets = 0, cached_bytes = 0;
        } preparation;
        static constexpr std::size_t cache_vertex_limit = 1000000, cache_asset_limit = 256;
        WorldMesh mesh;
        std::vector<WorldDraw> world_draws, close_draws;
        std::vector<ModelViewEntry> entries;
        std::uint64_t revision = 0;

      private:
        struct Prepared
        {
            std::weak_ptr<const Model> owner;
            std::uint32_t animation = 0;
            double time = 0;
            DVector scale{};
            bool fallback = false;
            std::vector<std::uint8_t> hidden_nodes;
            std::size_t hidden_triangles = 0;
            std::vector<WorldVertex> vertices;
            DVector min{}, max{}; // Local bounds of vertices, reused with them.
            bool bounded = false;
        };
        bool add_impl(const Model*, const std::shared_ptr<const Model>*, const AnimationTrack&, std::string_view skin,
                      NodePose, DVector scale, bool visible, bool close, ObjectId, std::string_view label,
                      const Resources&, std::string&, std::span<const std::uint8_t> hidden_nodes, float alpha,
                      bool require_skin);
        std::map<const Model*, Prepared> prepared_;
        std::size_t cached_vertices_ = 0;
        std::map<std::string, std::uint32_t> materials_;
        std::vector<DVector> sampled_;
    };
} // namespace OpenBlood2
