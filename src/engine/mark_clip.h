#pragma once
#include "engine/world_view.h"
#include <span>
namespace OpenBlood2
{
    // Clip a sprite quad in its local XY plane against a copied convex BSP ring.
    // The source plane offset is retained; new vertices interpolate texture coordinates.
    bool clip_mark(std::span<const WorldVertex> quad, const WorldDraw&, std::span<const DVector> polygon,
                   std::vector<WorldVertex>& triangles, std::string& error);
} // namespace OpenBlood2
