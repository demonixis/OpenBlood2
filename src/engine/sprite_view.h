#pragma once
#include "engine/particles.h"
#include "engine/sprite.h"
#include "engine/world_view.h"
#include <span>
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
        std::uint64_t blood_splat = 0, blood_spurt = 0, spark_system = 0, lightning_bolt = 0;
        std::uint64_t client_effect = 0, particle_system = 0, trail_system = 0, flame_system = 0, weapon_effect = 0,
                      object_effect = 0, stream_system = 0, warp_gate = 0;
        unsigned particle_index = 0;
        unsigned particles = 0; // Particles drawn by a whole-system entry (add_particle_system).
    };
    // Read-only presentation snapshot: owned textures/values, never a retained game pointer.
    class SpriteView
    {
      public:
        static constexpr unsigned max_entries = 32768;
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
        // A whole particle system (positions relative to `origin`) as one entry and one draw: camera-
        // facing quads, back to front within the system, sorted against other draws by their centre.
        // Particles with invalid positions or colours are skipped and reported in the entry error.
        bool add_particle_system(std::string_view texture, DVector origin, std::span<const Particle>, float half_extent,
                                 float alpha, std::uint64_t system, const Resources&, std::string&);
        // Immediately after add(): clip the last visible mark, leaving its inspection entry.
        bool clip_last(std::span<const DVector>, std::string& error);
        void sort();
        // Submission batches after sort(): consecutive draws sharing a texture and opacity are
        // appended as one world-space range with an identity transform, keeping their order.
        // Per-entry ranges and draws remain the unchanged inspection snapshot.
        void batch();
        WorldMesh mesh;
        std::vector<WorldDraw> draws, batched_draws;
        // One slice per original draw, into the uploaded world-space batches. The global
        // queue can split/rejoin consecutive slices without rebuilding or reuploading vertices.
        std::vector<WorldDraw> batch_slices;
        std::vector<SpriteViewEntry> entries;
        std::uint64_t revision = 0;

      private:
        struct Material
        {
            std::uint32_t index = 0;
            std::size_t zero_alpha = 0, partial_alpha = 0;
        };
        // Validates the camera/budget and appends an entry; nullptr with `error` on a budget failure.
        SpriteViewEntry* begin_entry(ObjectId, float alpha, std::string& error);
        // The texture's material (decoded once per world); nullptr with the entry error set on an
        // unusable texture, or with `error` set on a budget failure.
        const Material* material(std::string_view path, SpriteViewEntry&, const Resources&, std::string& error);
        bool add_frame(std::string_view, unsigned frame, NodePose, DVector, DDWORD, ObjectId, const Resources&,
                       std::string&, float alpha, DVector color, bool direct_extent);
        std::map<std::string, Material> materials_;
        std::vector<std::pair<double, std::uint32_t>> order_; // add_particle_system scratch: depth, index.
        DVector eye_{}, forward_{};
        DRotation camera_;
        bool camera_valid_ = false;
    };
} // namespace OpenBlood2
