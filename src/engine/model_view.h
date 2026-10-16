#pragma once
#include "engine/world_view.h"
namespace OpenBlood2
{
    struct ModelViewEntry
    {
        ObjectId object;
        std::string label, skin, error;
        bool visible = false, close = false;
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
        WorldMesh mesh;
        std::vector<WorldDraw> world_draws, close_draws;
        std::vector<ModelViewEntry> entries;
        std::uint64_t revision = 0;

      private:
        std::map<std::string, std::uint32_t> materials_;
        std::vector<DVector> sampled_;
    };
} // namespace OpenBlood2
