#pragma once

#include "engine/config.h"

namespace OpenBlood2
{
    // XR first light: an OpenXR session drawing a world-locked floor grid, axes and
    // wireframe controller boxes with aim rays. Needs no game data. Returns a process exit code.
    int run_xr_test(const Options&);
} // namespace OpenBlood2
