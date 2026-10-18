#pragma once
#include "game/legacy/basedefs_de.h"
#include <string_view>
HOBJECT SpawnObject(std::string_view, const DVector&, const DRotation&, DVector velocity);
// Authored spawn class names that no original ObjectDLL class defines (RocketAmmoPU is
// only ever looked up, by AI_Mgr; DieBugDieAmmo, FareAmmoPU and LifeEssencePU are
// authoring errors). The original GetClass returns null and nothing spawns.
bool SpawnClassAbsentFromOriginal(std::string_view request);
