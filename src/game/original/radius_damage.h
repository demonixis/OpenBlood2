#pragma once
#include "cpp_server_de.h"
// Selected ObjectUtilities::DamageObjectsInRadius rule; local typed transport.
OpenBlood2::RadiusDamageReport DamageObjectsInRadius(HOBJECT responsible, LPBASECLASS damager, DVector origin,
                                                     float radius, float damage, OpenBlood2::DamageType type);
