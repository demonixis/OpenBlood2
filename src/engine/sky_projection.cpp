#include "engine/collision.h"
#include "engine/sky.h"

namespace OpenBlood2
{
    bool valid_sky_definition(const SkyDefinition& d)
    {
        return physics_vector(d.min) && physics_vector(d.max) && physics_vector(d.view_min) &&
               physics_vector(d.view_max) && d.min.x < d.max.x && d.min.y < d.max.y && d.min.z < d.max.z &&
               d.view_min.x <= d.view_max.x && d.view_min.y <= d.view_max.y && d.view_min.z <= d.view_max.z;
    }
    bool sky_view_position(const SkyDefinition& d, DVector min, DVector max, DVector eye, DVector& out)
    {
        out = {};
        if (!valid_sky_definition(d) || !physics_vector(min) || !physics_vector(max) || !physics_vector(eye) ||
            min.x >= max.x || min.y >= max.y || min.z >= max.z)
            return false;
        // Blood II SkyDef contract; later LithTech common_draw.cpp supplies the explicit formula.
        // No clamp: inspection cameras outside world extents extrapolate the view box.
        auto axis = [](float a, float b, float x, float lo, float hi)
        { return float(double(lo) + (double(x) - a) / (double(b) - a) * (double(hi) - lo)); };
        const DVector p{axis(min.x, max.x, eye.x, d.view_min.x, d.view_max.x),
                        axis(min.y, max.y, eye.y, d.view_min.y, d.view_max.y),
                        axis(min.z, max.z, eye.z, d.view_min.z, d.view_max.z)};
        if (!physics_vector(p))
            return false;
        out = p;
        return true;
    }
} // namespace OpenBlood2
