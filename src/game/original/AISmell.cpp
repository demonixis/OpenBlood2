// Original AI_Mgr.cpp:5959-6078, PlayerObj.cpp:1692-1736 and SmellHint list order.
// See ai_smell_contracts.md for checked identities and source reachability limits.
#include "AI_Mgr.h"
#include "PlayerObj.h"
#include "SmellHint.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
namespace
{
    bool scan(DLink* link, bool backwards, HOBJECT& out)
    {
        out = nullptr;
        auto* server = BaseClass::GetServerDE();
        if (!server || SmellHint::m_dwNumSmells > 65536)
            return false;
        for (unsigned i = 0; link && link != &SmellHint::m_SmellHead && i < SmellHint::m_dwNumSmells; ++i)
        {
            auto* hint = static_cast<SmellHint*>(link->m_pData);
            if (!hint)
                return false;
            const auto h = hint->m_hObject;
            if (server->IsObjectAlive(h))
            {
                if (server->HandleToObject(h) != hint)
                    return false;
                out = h;
                return true;
            }
            link = backwards ? link->m_pPrev : link->m_pNext;
        }
        return link == &SmellHint::m_SmellHead || (!link && !SmellHint::m_dwNumSmells);
    }
} // namespace
bool SmellHint::LastLive(HOBJECT& out)
{
    return scan(m_SmellHead.m_pPrev, true, out);
}
bool SmellHint::NextLive(HOBJECT h, HOBJECT& out)
{
    out = nullptr;
    auto* s = BaseClass::GetServerDE();
    auto* hint = s ? dynamic_cast<SmellHint*>(s->HandleToObject(h)) : nullptr;
    return hint && s->IsObjectAlive(h) && scan(hint->m_Link.m_pNext, false, out);
}
bool CPlayerObj::UpdateSmellTrail()
{
    ++smell_checks;
    smell_error.clear();
    auto fail = [&](const char* why)
    {
        smell_error = why;
        return false;
    };
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject))
        return fail("Smell trail owner unavailable");
    if (m_damage.IsDead() || m_bSpectatorMode)
        return true;
    const float now = s->GetTime();
    if (!std::isfinite(m_fSmellTime) || !std::isfinite(m_fSmellDelay) || m_fSmellDelay < 0 ||
        !std::isfinite(m_fSmellTime + m_fSmellDelay))
        return fail("Invalid smell trail clock");
    if (now <= m_fSmellTime + m_fSmellDelay)
        return true;
    HOBJECT previous;
    DVector position, before;
    if (s->GetObjectPos(m_hObject, &position) != DE_OK || !SmellHint::LastLive(previous))
        return fail("Smell trail position/list unavailable");
    if (previous)
    {
        if (s->GetObjectPos(previous, &before) != DE_OK)
            return fail("Previous smell position unavailable");
        const auto delta = vector_subtract(position, before);
        smell_distance = std::hypot(delta.x, delta.y, delta.z);
        if (smell_distance < 10)
            return true;
    }
    else
        smell_distance = 0;
    ObjectCreateStruct state;
    state.m_Pos = position; // Original identity rotation, ordinary SmellHint scheduling.
    m_fSmellTime = now;     // Source advances the clock before CreateObject, including failure.
    auto* hint = s->CreateObject(s->GetClass("SmellHint"), &state);
    if (!hint)
        return fail("Smell trail creation failed");
    last_smell = hint->m_hObject;
    ++smell_drops;
    return true;
}
bool AI_Mgr::SetSmellCursor(HOBJECT h)
{
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject) ||
        (h && (!s->IsObjectAlive(h) || !dynamic_cast<SmellHint*>(s->HandleToObject(h)))))
        return NavigationFailure("Smell cursor requires a live hint");
    if (h == smell_cursor)
        return true;
    if (h && s->CreateInterObjectLink(m_hObject, h) != DE_OK)
        return NavigationFailure("Smell cursor link failed");
    if (smell_cursor)
        s->BreakInterObjectLink(m_hObject, smell_cursor);
    smell_cursor = h;
    return true;
}
bool AI_Mgr::AI_STATE_SearchSmellTarget()
{
    return NavigationState(StateSearchSmell);
}
bool AI_Mgr::SmellStep()
{
    ++smell_searches;
    smell_exit = 0;
    auto* s = GetServerDE();
    const int stim = ComputeStimuli();
    if (stim < 0)
        return NavigationFailure(perception_error);
    // Preserve original ordering. Smell perception clears m_hTarget and does NOT
    // populate m_pCurSmell; do not invent an autonomous trail-following acquisition.
    if (!GetTarget())
    {
        smell_exit = 1;
        return NavigationDecision();
    }
    if (stim > 0 && stim != 3)
    {
        smell_exit = 2;
        return NavigationDecision(stim);
    }
    if (!smell_cursor || !s->IsObjectAlive(smell_cursor))
    {
        if (smell_cursor)
        {
            SetSmellCursor(nullptr);
            ++smell_invalidations;
        }
        smell_exit = 3;
        return NavigationDecision();
    }
    navigation_threshold = (std::hypot(m_vDims.x, m_vDims.z) + .1f) / 2;
    if (Metacmd == 1)
    {
        const auto hint = FindObjectInRadius(AIFindList::Smells, m_fSmellingDist, FindVisible);
        if (!find.valid)
            return NavigationFailure(find.error);
        if (!hint)
        {
            smell_exit = 4;
            return NavigationDecision();
        }
        if (!SetSmellCursor(hint) || s->GetObjectPos(hint, &smell_position) != DE_OK)
            return NavigationFailure("Selected smell pose/link unavailable");
        return MC_FacePos(smell_position) || NavigationFailure(target_error);
    }
    if (Metacmd == 2)
    {
        bool edge = false;
        if (!IsLedge(m_MoveObj.GetForwardVector(), edge))
            return NavigationFailure(locomotion_error);
        if (edge)
            return NavigationDecision();
        DVector point;
        if (s->GetObjectPos(smell_cursor, &point) != DE_OK)
            return NavigationFailure("Smell cursor pose unavailable");
        point.y = m_MoveObj.GetPos().y;
        const auto delta = vector_subtract(point, m_MoveObj.GetPos());
        navigation_distance = std::hypot(delta.x, delta.y, delta.z);
        // Source repeats this throttled query before its inclusive arrival test.
        if (!IsLedge(m_MoveObj.GetForwardVector(), edge))
            return NavigationFailure(locomotion_error);
        if (edge)
        {
            ++Metacmd;
            return true;
        }
        if (navigation_distance <= navigation_threshold)
        {
            HOBJECT next;
            if (!SmellHint::NextLive(smell_cursor, next))
                return NavigationFailure("Smell successor unavailable");
            if (!next)
            {
                ++Metacmd;
                return true;
            }
            if (!SetSmellCursor(next) || s->GetObjectPos(next, &smell_position) != DE_OK)
                return NavigationFailure("Next smell pose/link unavailable");
            ++smell_advances;
            // Source increments 2 -> 3 through FacePos; it does not reset the phase.
            return MC_FacePos(smell_position) || NavigationFailure(target_error);
        }
        return MC_Run() || NavigationFailure(locomotion_error);
    }
    return Metacmd == 3 ? NavigationDecision() : true;
}
