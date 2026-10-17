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
        std::uint64_t client_effect = 0, particle_system = 0, trail_system = 0, flame_system = 0, weapon_effect = 0,
                      object_effect = 0, stream_system = 0;
        unsigned particle_index = 0;
    };
    // Read-only presentation snapshot: owned textures/values, never a retained game pointer.
    class SpriteView
    {
      public:
        static constexpr unsigned max_entries = 16384;
        SpriteView()
        {
            clear();
        }
        void clear();
        bool begin_frame(DVector eye, DRotation camera, std::string& error);
        bool add(const Sprite*, const SpriteTrack&, NodePose, DVector scale, DDWORD flags, ObjectId, const Resources&,
                 std::string& error, float alpha = 1, DVector color = {1, 1, 1});
        bool add_particle(std::string_view texture, DVector position, float half_extent, DVector color, float alpha,
                          std::uint64_t system, unsigned index, const Resources&, std::string&);
        void sort();
        WorldMesh mesh;
        std::vector<WorldDraw> draws;
        std::vector<SpriteViewEntry> entries;
        std::uint64_t revision = 0;

      private:
        bool add_frame(std::string_view, unsigned frame, NodePose, DVector, DDWORD, ObjectId, const Resources&,
                       std::string&, float alpha, DVector color, bool direct_extent);
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
