#pragma once
#include "engine/model.h"
#include <span>
namespace OpenBlood2
{
    // One arm chain of a posed model re-routed by an analytic two-bone solve: the arm runs from
    // `shoulder` to the wrist's animated position with the authored segment lengths and bends
    // toward `pole`. The wrist and its subtree keep their animated poses; other nodes under the
    // upper or lower arm move rigidly with that segment. Model space, model units.
    struct ArmTarget
    {
        std::uint16_t upper = 0, lower = 0, wrist = 0; // Model order: lower under upper, wrist under lower.
        DVector shoulder{}, pole{};
    };
    struct ArmSolve
    {
        DVector shoulder{}, elbow{}; // The shoulder slides along the reach when the wrist is out of range.
        bool slid = false;
    };
    // Applies the targets in order to model-space node poses (as sample_skin returns them).
    // False, with `pose` unchanged, on an invalid chain or non-finite input.
    bool solve_arms(const Model&, std::span<const ArmTarget>, std::span<NodePose> pose,
                    std::span<ArmSolve> results = {});
} // namespace OpenBlood2
