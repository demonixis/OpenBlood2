// Original AI_Mgr::CanSee and its GlobalFilterFn flags (Monolith, 1997/1998).
// See AI_SIGHT_CONTRACTS.md for source scope, typed failures and geometry limits.
#include "AI_Mgr.h"
namespace OpenBlood2
{
    const char* ai_sight_outcome_name(AISightOutcome value)
    {
        switch (value)
        {
            case AISightOutcome::None:
                return "Not queried";
            case AISightOutcome::Body:
                return "Visible: body hit";
            case AISightOutcome::Head:
                return "Visible: head retry hit";
            case AISightOutcome::Blocked:
                return "Blocked";
            case AISightOutcome::Miss:
                return "No target hit";
            case AISightOutcome::Invalid:
                return "Invalid query/identity";
            case AISightOutcome::Unavailable:
                return "Collision unavailable";
        }
        return "Unknown sight outcome";
    }
} // namespace OpenBlood2
bool AI_Mgr::CanSee(HOBJECT target)
{
    using namespace OpenBlood2;
    ++sight_queries;
    sight = {};
    sight.target = target; // Checked inspection identity; no ownership or target acquisition.
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !server->IsObjectAlive(target))
    {
        sight.outcome = AISightOutcome::Invalid;
        return false;
    }
    DRotation rotation;
    sight.origin_head = server->GetModelNodeTransform(m_hObject, "head", &sight.origin, &rotation);
    if (!sight.origin_head)
        server->GetObjectPos(m_hObject, &sight.origin); // Source cached MoveObj position; current pose in this engine.
    server->GetObjectPos(target, &sight.body);
    auto query = [&](DVector end, SegmentHit& hit, SegmentResult& result)
    {
        ++sight.rays;
        result = server->IntersectSegment(sight.origin, end, m_hObject, hit, false, SegmentObjects::SolidOrRayHit,
                                          SegmentFilter::AIVisibility);
        if (result == SegmentResult::Invalid)
            sight.outcome = AISightOutcome::Invalid;
        else if (result == SegmentResult::Unavailable)
            sight.outcome = AISightOutcome::Unavailable;
        else if (result == SegmentResult::Miss)
            sight.outcome = AISightOutcome::Miss;
        else
            sight.outcome = AISightOutcome::Blocked;
        return result == SegmentResult::Hit;
    };
    if (!query(sight.body, sight.body_hit, sight.body_result))
        return false; // Original misses do not attempt the target's head.
    if (sight.body_hit.object == target)
    {
        sight.outcome = AISightOutcome::Body;
        return true;
    }
    if (server->GetModelNodeTransform(target, "head", &sight.head, &rotation))
    {
        sight.retried_head = true;
        if (query(sight.head, sight.head_hit, sight.head_result) && sight.head_hit.object == target)
        {
            sight.outcome = AISightOutcome::Head;
            return true;
        }
    }
    return false;
}
