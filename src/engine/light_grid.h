#pragma once
#include "engine/world_lights.h"
#include <array>

namespace OpenBlood2
{
    // World-space grid of the static light objects (Light/DirLight/ObjectLight) that may
    // reach each cell, for the enhanced renderer's direct specular and normal-map relief on
    // world faces. Built once per world on the CPU by binning each light sphere into the cells
    // it overlaps; there is no baking, ray casting or probe. Each cell keeps at most
    // cell_light_limit lights, strongest first by their bound inside the cell (luminance times
    // the original distance falloff at the nearest cell point).
    struct LightGrid
    {
        static constexpr std::size_t cell_light_limit = 8, cell_limit = 65536, light_limit = 4096;
        static constexpr float minimum_cell = 128;
        // Three float4 per light: position + radius, color + cone cosine (below -1: no cone),
        // cone axis + 0. Colors are the 0-1 StaticLight colors.
        std::vector<std::array<float, 4>> lights;
        // cells[i] = (first index << 4) | count; the light indices follow the cells in the same
        // buffer at cells.size() + first.
        std::vector<std::uint32_t> cells;
        std::array<float, 3> origin{};
        float cell_size = minimum_cell;
        std::array<std::uint32_t, 3> dimensions{};
        std::size_t dropped = 0; // Light references beyond a cell's limit.
        std::size_t cell_count() const
        {
            return std::size_t(dimensions[0]) * dimensions[1] * dimensions[2];
        }
    };
    // An empty light list gives a one-cell grid with no light. Invalid lights fail.
    bool build_light_grid(std::span<const StaticLight>, LightGrid&, std::string& error);
} // namespace OpenBlood2
