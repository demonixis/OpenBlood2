#pragma once
#include "engine/sprite.h"
#include "engine/world_view.h"
namespace OpenBlood2
{
    struct SpriteViewEntry
    {
        ObjectId object;
        std::string frame_path, error;
        unsigned frame = 0, width = 0, height = 0;
        std::size_t zero_alpha = 0, partial_alpha = 0;
        bool visible = false, rotatable = false;
        std::uint32_t range = UINT32_MAX;
        float alpha = 1;
        std::uint64_t client_effect = 0;
    };
    // Read-only presentation snapshot: owned textures/values, never a retained game pointer.
    class SpriteView
    {
      public:
        SpriteView()
        {
            clear();
        }
        void clear();
        bool begin_frame(DVector eye, DRotation camera, std::string& error);
        bool add(const Sprite*, const SpriteTrack&, NodePose, DVector scale, DDWORD flags, ObjectId, const Resources&,
                 std::string& error, float alpha = 1);
        void sort();
        WorldMesh mesh;
        std::vector<WorldDraw> draws;
        std::vector<SpriteViewEntry> entries;
        std::uint64_t revision = 0;

      private:
        struct Material
        {
            std::uint32_t index = 0;
            std::size_t zero_alpha = 0, partial_alpha = 0;
        };
        std::map<std::string, Material> materials_;
        DVector eye_{}, forward_{};
        DRotation camera_;
        bool camera_valid_ = false;
    };
} // namespace OpenBlood2
