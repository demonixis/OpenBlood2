#pragma once
#include "engine/world_view.h"
#include <algorithm>
#include <vector>
namespace OpenBlood2
{
    enum class TransparentKind
    {
        World,
        Model,
        Explosion,
        PolyGrid,
        Sprite
    };
    struct TransparentDraw
    {
        TransparentKind kind;
        WorldDraw draw;
        double depth = 0;
    };
    // Stable ties keep the former family/source order. Particle systems retain their
    // internal particle order and are interleaved by their centre, as ordinary objects.
    inline void sort_transparency(std::vector<TransparentDraw>& draws)
    {
        std::stable_sort(draws.begin(), draws.end(), [](const auto& a, const auto& b) { return a.depth > b.depth; });
        std::size_t count = 0;
        for (const auto& item : draws)
        {
            if (count && item.kind == TransparentKind::Sprite)
            {
                auto& previous = draws[count - 1];
                auto& a = previous.draw;
                const auto& b = item.draw;
                if (previous.kind == item.kind && a.range == b.range && a.batch == b.batch && a.alpha == b.alpha &&
                    a.vertex_count && b.vertex_count &&
                    std::uint64_t(a.first_vertex) + a.vertex_count == b.first_vertex)
                {
                    a.vertex_count += b.vertex_count;
                    continue;
                }
            }
            draws[count++] = item;
        }
        draws.resize(count);
    }
} // namespace OpenBlood2
