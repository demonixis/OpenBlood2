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
    // Static corner of a GPU-skinned ABC model, one per face corner in authored order.
    struct SkinnedVertex
    {
        DVector position;                    // Authored node-local position.
        std::array<float, 2> uv{};           // Authored face-corner UV.
        std::array<std::uint8_t, 4> nodes{}; // Vertex node slot, then the face's three corner node slots.
        std::uint32_t deform = UINT32_MAX;   // Last packed-deformation slot writing this vertex, if any.
        DVector normal{};                    // Authored node-local vertex normal (lighting only).
    };
    // Per-instance model light: ambient plus up to four sources lit per vertex with N.L.
    // The default is fully bright (no lighting service).
    struct ModelLight
    {
        struct Source
        {
            DVector position{}, color{}, direction{0, 0, 1};
            float radius = 0, cos_half = -2; // cos_half below -1: no cone.
        };
        static constexpr std::size_t source_limit = 4;
        DVector ambient{1, 1, 1};
        float scale = 1; // Applied after the 0-1 clamp: world_saturate_scale for N.L shading.
        std::uint32_t count = 0;
        std::array<Source, source_limit> sources{};
    };
    // Validates node, face, UV and deformation references and builds the static corner stream.
    bool build_skinned_vertices(const Model&, std::vector<SkinnedVertex>&, std::string& error);
    // One GPU-skinned instance of a shared immutable asset for the current frame.
    struct SkinnedInstance
    {
        const Model* model = nullptr;
        std::weak_ptr<const Model> owner;
        std::uint32_t material = 0;
        std::uint32_t palette = 0;     // First palette vec4: per node rotation, then translation/hidden.
        std::uint32_t deformation = 0; // First deformation slot.
        DVector scale{1, 1, 1}, color{1, 1, 1};
        DVector min{}, max{}; // Conservative scaled model-space bounds when bounded.
        DVector center{};     // World-space bounds center (the pose position when unbounded).
        ModelLight light;
        bool bounded = false, translucent = false;
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
        // GPU skinning mode: visible shared assets publish node palettes and packed
        // deformation instead of CPU-posed vertices, and draws index `instances`.
        // Borrowed models are refused. Off by default; the CPU path is unchanged.
        bool gpu_skinning = false;
        std::vector<SkinnedInstance> instances;
        std::vector<std::array<float, 4>> palette, deformation;
        static constexpr std::size_t palette_limit = 262144, deformation_limit = 1000000;
        // Marks an entry's drawn geometry translucent in either mode.
        void mark_translucent(std::size_t entry);
        // Sets a GPU-skinned entry's light; unresolved-material markers and the CPU path stay
        // unlit. False when the entry has no drawn instance.
        bool set_light(std::size_t entry, const ModelLight& light);

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
        struct SkinLayout
        {
            std::weak_ptr<const Model> owner;
            std::vector<std::uint8_t> vertex_slots;          // Node slot of each vertex.
            std::vector<std::array<DVector, 2>> node_bounds; // Node-local vertex/deformation bounds.
            std::vector<bool> bounded;
        };
        bool add_skinned(const Model&, const std::shared_ptr<const Model>&, const AnimationTrack&, NodePose,
                         DVector scale, bool close, float alpha, std::uint32_t material,
                         std::span<const std::uint8_t> hidden_nodes, ModelViewEntry&, std::string& error);
        std::map<const Model*, SkinLayout> layouts_;
        std::vector<NodePose> pose_;
        std::vector<DVector> deformed_;
        std::map<const Model*, Prepared> prepared_;
        std::size_t cached_vertices_ = 0;
        std::map<std::string, std::uint32_t> materials_;
        std::vector<DVector> sampled_;
    };
} // namespace OpenBlood2
