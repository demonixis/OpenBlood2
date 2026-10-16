#pragma once
#include "engine/world.h"
#include <array>

namespace OpenBlood2
{
    // Diagnostic geometry only: no texture, baked light, sky or transparency claim.
    struct WorldVertex
    {
        DVector position, color;
    };
    struct WorldRange
    {
        std::uint32_t first = 0, count = 0;
        DVector pivot{};
        std::string name;
    };
    struct WorldMesh
    {
        std::vector<WorldVertex> vertices;
        std::vector<WorldRange> ranges;
        std::size_t invisible = 0, degenerate = 0, transparent = 0, sky = 0;
        bool build(const World&, std::string&);
    };
    struct WorldDraw
    {
        std::uint32_t range = 0;
        DVector position{};
        DRotation rotation;
    };
    struct WorldViewStats
    {
        std::size_t triangles = 0, draws = 0, hidden = 0, unbound = 0, containers = 0;
        std::uint64_t uploads = 0, frames = 0;
    };
    // Never creates objects or advances simulation; current checked identities only.
    bool world_draws(const WorldMesh&, const Objects&, std::vector<WorldDraw>&, WorldViewStats&, std::string&);
    // Row vectors packed for the shader. Original +Y up / +Z forward, SDL depth [0,1].
    struct WorldViewUniform
    {
        std::array<float, 4> eye{}, right{}, up{}, forward{}, projection{}, position{}, quaternion{};
    };
    bool world_view_uniform(DVector eye, DRotation view, float aspect, const WorldDraw&, WorldViewUniform&);
} // namespace OpenBlood2
