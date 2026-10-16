#pragma once
#include "basedefs_de.h"
#include <string>
#include <vector>
namespace OpenBlood2
{
    struct Particle
    {
        DVector position{}, velocity{};
        float lifetime = 0;
    };
    // Local coordinates; no collision, implicit world gravity or callbacks.
    bool advance_particles(std::vector<Particle>&, float gravity, double seconds, std::string&);
} // namespace OpenBlood2
