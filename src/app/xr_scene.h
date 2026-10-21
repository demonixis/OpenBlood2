#pragma once

#include "engine/config.h"

namespace OpenBlood2
{
    // XR first light: an OpenXR session drawing a world-locked floor grid, axes and
    // wireframe controller boxes with aim rays. Needs no game data. Returns a process exit code.
    int run_xr_test(const Options&);
    // Direct world entry rendered in stereo: the game camera's position and yaw carry the tracked
    // head. No menu, HUD or controller input yet. Returns a process exit code.
    int run_xr_world(const Options&, const Config&);
    // Android headsets: the shared public folders openblood2/game (retail, preferred) and
    // openblood2/demo select --xr with that data; neither selects the first-light scene.
    void select_android_xr_data(Options&);
} // namespace OpenBlood2
