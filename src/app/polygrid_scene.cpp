#include "polygrid_scene.h"
#include "client/effect_scene.h"
namespace OpenBlood2
{
    bool update_polygrid_scene(const ClientScene& source, PolyGridView& scene, std::string& error)
    {
        error.clear();
        scene.begin_frame();
        if (source.world.frame().world != source.effects.frame().world)
        {
            error = "PolyGrid effects world mismatch";
            return false;
        }
        return append_effect_polygrids(source.effects, scene, source.resources, error);
    }
} // namespace OpenBlood2
