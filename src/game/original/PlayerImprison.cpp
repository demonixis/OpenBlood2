// Original PlayerObj.cpp MID_IMMOBILIZE and CheckItemsAndSpells imprisonment.
#include "PlayerObj.h"
#include <cmath>

bool CPlayerObj::Imprison(float seconds)
{
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject) || !std::isfinite(seconds) || seconds < 0 ||
        !std::isfinite(s->GetTime() + seconds))
        return false;
    ++imprison_requests;
    if (m_bImprisoned || m_bDead)
        return true; // No refresh, extension or newly read duration in the original.
    m_fImprisonLength = seconds;
    m_fImprisonStart = s->GetTime();
    m_bImprisoned = true;
    ++imprison_accepts;
    imprison_notice.clear();
    imprison_error.clear();
    if (!s->GetClientResourceString(265 /* IDS_GENERAL_IMMOBILE */, imprison_notice))
        imprison_error = "Original immobilization resource string unavailable";
    return true;
}
bool CPlayerObj::CheckImprisonment()
{
    if (!m_bImprisoned)
        return true;
    auto* s = GetServerDE();
    if (!s || !std::isfinite(s->GetTime()) || !std::isfinite(m_fImprisonStart) || !std::isfinite(m_fImprisonLength) ||
        m_fImprisonLength < 0)
        return false;
    if (s->GetTime() - m_fImprisonStart > m_fImprisonLength)
    {
        m_bImprisoned = false; // Source strictly >, not >=.
        ++imprison_expiries;
        imprison_notice.clear();
        imprison_error.clear();
        if (!s->GetClientResourceString(264 /* IDS_GENERAL_CANMOVE */, imprison_notice))
            imprison_error = "Original movement-restored resource string unavailable";
    }
    return true;
}
