#pragma once
#include "engine/resources.h"
#include "engine/texture.h"
#include "engine/world.h"
#include <array>

namespace OpenBlood2
{
    // Unlit base textures; authored transparent faces use decoded DTX alpha.
    struct WorldVertex
    {
        DVector position, color;
        std::array<float, 2> uv{};
    };
    struct WorldMaterial
    {
        std::string path, error;
        Texture image;
    };
    struct WorldSurfaceEffect
    {
        std::uint32_t model = 0, surface = 0;
        std::string name, parameters, error;
        std::array<float, 2> speed{}; // Original P/Q origin speeds.
        DVector origin_velocity{};
        std::array<double, 2> texel_rate{}; // Negative dot(origin velocity, P/Q).
    };
    // Bounded Pan subset; an unavailable effect leaves authored geometry static.
    bool world_surface_effect(const Surface&, WorldSurfaceEffect&);
    bool world_texture_offset(std::array<double, 2> rate, double time, std::array<float, 4>&);
    struct WorldBatch
    {
        std::uint32_t first = 0, count = 0, material = 0;
        bool translucent = false;
        DVector center{};                // One local-space center per translucent polygon, for sorting.
        std::array<double, 2> uv_rate{}; // Cycles/second; independent of the shared texture.
    };
    struct WorldRange
    {
        std::uint32_t first = 0, count = 0;
        DVector pivot{};
        std::string name;
        std::vector<WorldBatch> batches;
    };
    struct WorldMesh
    {
        std::vector<WorldVertex> vertices;
        std::vector<WorldRange> ranges;
        // Slot zero is white. Failed resources remain named diagnostic records.
        std::vector<WorldMaterial> materials;
        std::vector<WorldSurfaceEffect> effects; // Includes hidden/invisible/unbound authored surfaces.
        std::size_t texture_bytes = 0;
        std::size_t invisible = 0, degenerate = 0, transparent = 0, sky = 0, blended = 0;
        bool build(const World&, std::string&, const Resources* = nullptr);
    };
    struct WorldDraw
    {
        std::uint32_t range = 0;
        DVector position{};
        DRotation rotation;
        float alpha = 1; // Alpha passes only; opaque passes retain their existing policy.
    };
    struct WorldViewStats
    {
        std::size_t triangles = 0, draws = 0, hidden = 0, unbound = 0, containers = 0;
        std::size_t batches = 0;
        std::uint64_t uploads = 0, frames = 0, alpha_frames = 0;
        std::size_t alpha_batches = 0, pan_batches = 0, alpha_pan_batches = 0;
        double surface_time = 0;
    };
    // Never creates objects or advances simulation; current checked identities only.
    bool world_draws(const WorldMesh&, const Objects&, std::vector<WorldDraw>&, WorldViewStats&, std::string&);
    struct WorldAlphaBatch
    {
        std::size_t draw = 0, batch = 0;
        double depth = 0;
    };
    // Stable far-to-near order of current posed faces. Does not retain object handles.
    bool world_alpha_order(const WorldMesh&, std::span<const WorldDraw>, DVector eye, DRotation view,
                           std::vector<WorldAlphaBatch>&, std::string&);
    // Row vectors packed for the shader. Original +Y up / +Z forward, SDL depth [0,1].
    struct WorldViewUniform
    {
        std::array<float, 4> eye{}, right{}, up{}, forward{}, projection{}, position{}, quaternion{}, texture_offset{};
    };
    bool world_view_uniform(DVector eye, DRotation view, float aspect, const WorldDraw&, WorldViewUniform&);
} // namespace OpenBlood2
