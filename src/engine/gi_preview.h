#pragma once
#include "engine/gi_lightmap.h"
#include <string>
#include <vector>

// The --gi-preview dump (gi_contracts.md, lot G7): CPU views of a baked world from its start points,
// three panels side by side for judging the k, f, bounce and sky defaults. Diagnostic only: no frame
// of the game uses it.
namespace OpenBlood2
{
    struct GiPreviewView
    {
        DVector eye{}, forward{0, 0, 1};
        std::string label;
    };
    // The first GameStartPoint at its yaw and turned 120 and 240 degrees, then the other start points
    // at their own, six views at most; eyes 36 units above Pos. Empty when the world has none.
    std::vector<GiPreviewView> gi_preview_views(const World&);

    // Per-element light for the panels: the original light (0-1), the D1 composition (0-1) and k E.
    struct GiPreviewValues
    {
        std::vector<float> original, composed, indirect;
    };
    GiPreviewValues gi_preview_values(const GiReceivers&, const GiIndirect&, float k, float f);
    // The indirect light stored in the sections, in receiver order; false when the sections do not list
    // exactly these blocks and sets.
    bool gi_indirect_from_sections(const GiReceivers&, const GiLightmaps&, const GiVertexLight&, GiIndirect&,
                                   std::string& error);

    // The indirect panel's brightness factor: k E is a fraction of the drawn light.
    constexpr float gi_preview_indirect_exposure = 4;
    // Original | GI | Indirect only, each panel width x height, 90-degree horizontal field, 2x2 samples
    // per pixel, RGBA8. Faces show albedo times their drawn light (pages doubled); the indirect panel
    // shows albedo times k E times gi_preview_indirect_exposure on lit faces and black elsewhere. The
    // sky shows as drawn (unscaled) in every panel.
    void render_gi_preview(const GiBakeScene&, const GiSky&, const GiReceivers&, const GiPreviewValues&,
                           const GiPreviewView&, unsigned width, unsigned height, unsigned jobs,
                           std::vector<std::uint8_t>& rgba);
} // namespace OpenBlood2
