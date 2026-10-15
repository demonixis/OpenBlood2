#pragma once
#include "game/legacy/basedefs_de.h"
#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2
{
    struct ModelVertex
    {
        DVector position, normal;
        std::uint8_t node = 0;
        std::array<std::uint16_t, 2> replacements{};
    };
    struct ModelFace
    {
        std::array<std::array<float, 2>, 3> uv{};
        std::array<std::uint16_t, 3> vertices{};
        DVector normal;
    };
    struct ModelNode
    {
        std::string name;
        DVector min, max;
        std::uint16_t index = 0;
        std::uint8_t flags = 0;
        int parent = -1;
        std::vector<std::uint16_t> deformed_vertices;
    };
    struct NodePose
    {
        DVector position;
        DRotation rotation;
    };
    struct ModelKey
    {
        std::uint32_t time = 0;
        DVector min, max;
        std::string text;
    };
    struct NodeAnimation
    {
        std::vector<NodePose> frames;
        std::vector<std::uint8_t> deformation; // Frame-major compressed XYZ, decoded with scale + translation.
        DVector scale, translation;
    };
    struct ModelAnimation
    {
        std::string name;
        std::uint32_t duration = 0;
        DVector min, max, dimensions;
        std::vector<ModelKey> keys;
        std::vector<NodeAnimation> nodes;
    };
    struct Model
    {
        std::string command;
        DVector min, max;
        std::uint32_t main_vertices = 0;
        std::vector<std::uint16_t> lod_starts;
        std::vector<ModelVertex> vertices;
        std::vector<ModelFace> faces;
        std::vector<ModelNode> nodes; // Original preorder; parent always precedes child.
        std::vector<ModelAnimation> animations;
    };
    inline constexpr std::uint32_t no_animation = UINT32_MAX;
    bool read_model(std::span<const std::uint8_t> bytes, Model& out, std::string& error);
    std::uint32_t animation_index(const Model& model, std::string_view name);
    // Skeleton transforms, followed by optional packed vertex deformation.
    bool sample_pose(const Model& model, std::uint32_t animation, double milliseconds, std::vector<NodePose>& out);
    bool sample_vertices(const Model& model, std::uint32_t animation, double milliseconds, std::vector<DVector>& out);
    DVector rotate_vector(DRotation q, DVector v);

    struct AnimationTrack
    {
        std::uint32_t animation = 0;
        double time = 0;
        bool looping = true, done = false, first = true;
        std::uint64_t revision = 0;
        void reset()
        {
            time = 0;
            done = false;
            first = true;
            ++revision;
        }
    };
    // On failure neither track nor output changes. Events include keys at time zero,
    // all crossed loop boundaries, and the terminal frame of a nonlooping clip.
    bool advance_animation(const Model& model, AnimationTrack& track, double seconds,
                           std::vector<std::uint32_t>& events, std::string& error);
} // namespace OpenBlood2
