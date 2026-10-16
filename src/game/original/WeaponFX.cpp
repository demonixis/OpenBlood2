// Original WeaponFX.cpp PlayImpactSound (977-992), GetImpactSound (1104-1152).
#include "WeaponFX.h"
#include "server_de.h"
std::string CWeaponFX::GetImpactSound(std::minstd_rand& random) const
{
    const char* material = nullptr;
    switch (m_eSurfaceType)
    {
        case SURFTYPE_STONE:
        case SURFTYPE_BUILDING:
            material = "stone";
            break;
        case SURFTYPE_METAL:
            material = "metal";
            break;
        case SURFTYPE_WOOD:
            material = "wood";
            break;
        case SURFTYPE_ENERGY:
            material = "energy";
            break;
        case SURFTYPE_GLASS:
            material = "glass";
            break;
        case SURFTYPE_TERRAIN:
            material = "terrain";
            break;
        case SURFTYPE_CLOTH:
            material = "cloth";
            break;
        case SURFTYPE_PLASTIC:
            material = "plastic";
            break;
        case SURFTYPE_FLESH:
            material = "flesh";
            break;
        case SURFTYPE_LIQUID:
            material = "water";
            break;
        default:
            return {}; // No random draw for unknown/sky/fire-through.
    }
    // SharedDefs.h defines B2_LITE: NRES(4) is 3. Own the string instead of writing a literal.
    return std::string("Sounds/Weapons/impacts/") + material + "/impact" +
           std::to_string(std::uniform_int_distribution<int>(0, 2)(random) + 1) + ".wav";
}
bool CWeaponFX::PlayImpactSound(ServerDE& server, HOBJECT source, std::minstd_rand& random)
{
    resource = GetImpactSound(random);
    error.clear();
    sound = 0;
    if (resource.empty())
        return true; // Original intentional silence, not a successful voice.
    OpenBlood2::SoundRequest request;
    request.resource = resource;
    request.outer_radius = 500;
    request.inner_radius = 50;
    request.priority = 0; // SOUNDPRIORITY_MISC_LOW.
    sound = server.PlayDetachedSound(source, request, m_vDestPos, error);
    return sound != 0;
}
