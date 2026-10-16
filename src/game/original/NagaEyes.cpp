// Original GameWeapons.cpp CNagaEyes::FireVector and IgnoreTypeFilterFn.
#include "B2BaseClass.h"
#include "Weapon.h"
#include "server_de.h"
using namespace OpenBlood2;
FireResult CNagaEyes::FireVector()
{
    auto* server = BaseClass::GetServerDE();
    beam = 0;
    beam_error.clear();
    explosion_error.clear();
    beam_hit = {};
    beam_end = last_shot.end;
    beam_collision = SegmentResult::Unavailable;
    if (!server || !firing || !server->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    // This separate visual query ignores the owner's complete class family.
    // The base FireVector retains its ordinary damage/continuation query.
    beam_collision = server->IntersectSegment(last_shot.request.position, last_shot.end, m_hOwner, beam_hit, false,
                                              SegmentObjects::SolidOnly, SegmentFilter::IgnoreOwnerClass);
    if (beam_collision != SegmentResult::Hit && beam_collision != SegmentResult::Miss)
    {
        beam_error = "Original Naga visual ray unavailable";
        return last_shot.result = FireResult::Unavailable;
    }
    if (beam_collision == SegmentResult::Hit)
        beam_end = beam_hit.point;
    beam = server->PublishLaserBeam(m_hOwner, last_shot.request.position, beam_end, beam_error);
    // Original sends EXP_NAGA_EYE_BEAM at vDest, with vDest again as normal,
    // even on a miss. Preserve the request in inspection; do not replace it with
    // a different explosion or silently claim the unported composite succeeded.
    explosion_error = "EXP_NAGA_EYE_BEAM composite unavailable (position and normal are both beam endpoint)";
    return CWeapon::FireVector(); // Visual failures never suppress original vector damage.
}
