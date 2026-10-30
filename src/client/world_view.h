#pragma once
#include "client/world.h"
#include "engine/world_view.h"

namespace OpenBlood2
{
    bool world_draws(const WorldMesh&, const ClientWorld&, std::vector<WorldDraw>&, WorldViewStats&, std::string&);
    bool client_sky_draws(const World&, const WorldMesh&, const ClientWorld&, DVector eye, std::vector<WorldDraw>&,
                          DVector& sky_eye, std::string&);
} // namespace OpenBlood2
