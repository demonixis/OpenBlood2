// Original PlayerObj::GrabFlag/GiveFlag/DropFlag, with checked local match state.
#include "FlagObjects.h"
#include "PlayerObj.h"
#include <algorithm>
#include <limits>
using namespace OpenBlood2;
void CPlayerObj::NotifyCTF(CTFEventKind kind, int points, HOBJECT capturer)
{
    if (!m_hClient)
        return;
    if (GetServerDE()->SendToClient(m_hClient, CTFEvent{kind, static_cast<DBYTE>(ctf_team), points, capturer}) == DE_OK)
        ++ctf_messages;
    else
        ctf_error = "CTF client message delivery failed";
}
bool CPlayerObj::SetCTFTeam(int team)
{
    if (team < 1 || team > 2 || !(g_ctf.flags[0] || g_ctf.flags[1]))
        return false;
    if (ctf_team != team)
        DropFlag();
    ctf_team = team;
    NotifyCTF(CTFEventKind::Team);
    return ctf_error.empty();
}
void CPlayerObj::GrabFlag(HOBJECT h)
{
    auto* s = GetServerDE();
    if (!h || m_damage.IsDead() || m_bDead || m_bSpectatorMode || m_hFlagObject == h || !s->IsObjectAlive(h))
        return;
    auto* f = dynamic_cast<FlagObject*>(s->HandleToObject(h));
    if (!f)
        return;
    if (f->team == ctf_team)
    {
        if (!f->in_stand)
        {
            NotifyCTF(CTFEventKind::Returned);
            f->ReturnToFlagStand();
        }
        return;
    }
    if (m_hFlagAttach)
    {
        s->RemoveAttachment(m_hFlagAttach);
        m_hFlagAttach = 0;
    }
    DVector offset{0, 75, 0};
    DRotation rotation;
    if (s->CreateAttachment(m_hObject, h, nullptr, &offset, &rotation, &m_hFlagAttach) != DE_OK)
    {
        ctf_error = "Flag attachment unavailable";
        return;
    }
    if (s->CreateInterObjectLink(m_hObject, h) != DE_OK)
    {
        s->RemoveAttachment(m_hFlagAttach);
        m_hFlagAttach = 0;
        ctf_error = "Flag lifetime link unavailable";
        return;
    }
    NotifyCTF(CTFEventKind::Grabbed);
    f->GiveToPlayer(m_hObject);
    m_hFlagObject = h;
}
void CPlayerObj::GiveFlag(HOBJECT h)
{
    auto* s = GetServerDE();
    if (!h || !m_hFlagAttach || !m_hFlagObject || !s->IsObjectAlive(h) || !s->IsObjectAlive(m_hFlagObject))
        return;
    auto* stand = dynamic_cast<FlagStand*>(s->HandleToObject(h));
    auto* f = dynamic_cast<FlagObject*>(s->HandleToObject(m_hFlagObject));
    if (!stand || !f || stand->team != ctf_team)
        return;
    if (!f->IsOtherFlagInStand())
    {
        const float now = s->GetTime();
        if (g_ctf.last_need_flag + 4 < now)
        {
            NotifyCTF(CTFEventKind::NeedFlag);
            g_ctf.last_need_flag = now;
        }
        return;
    }
    int opponents = 0;
    if (g_ctf.use_team_size)
        for (auto h = s->GetNextObject(nullptr); h; h = s->GetNextObject(h))
            if (auto* p = dynamic_cast<CPlayerObj*>(s->HandleToObject(h));
                p && p->GetClient() && p->ctf_team == 3 - ctf_team)
                ++opponents;
    const auto points = std::int64_t(g_ctf.flag_value) * std::max(1, opponents);
    if (points < 0 || points > 1000000 || ctf_score > std::numeric_limits<int>::max() - points ||
        g_ctf.team_scores[ctf_team - 1] > std::numeric_limits<int>::max() - points)
    {
        ctf_error = "CTF score outside bounded integer range";
        return;
    }
    ctf_score += points;
    g_ctf.team_scores[ctf_team - 1] += points;
    SoundRequest sound;
    sound.resource = "sounds_multipatch\\CtfFlagCapture.wav";
    sound.speaker = m_hObject;
    sound.inner_radius = 200;
    sound.outer_radius = 2000;
    sound.priority = 1;
    s->PlayCharacterSound(m_hObject, sound, false, ctf_error);
    for (auto h = s->GetNextObject(nullptr); h; h = s->GetNextObject(h))
        if (auto* p = dynamic_cast<CPlayerObj*>(s->HandleToObject(h)); p && p->GetClient())
            p->NotifyCTF(CTFEventKind::Captured, static_cast<int>(points), m_hObject);
    s->RemoveAttachment(m_hFlagAttach);
    m_hFlagAttach = 0;
    s->BreakInterObjectLink(m_hObject, m_hFlagObject);
    f->ReturnToFlagStand();
    m_hFlagObject = nullptr;
}
void CPlayerObj::DropFlag(bool notify)
{
    auto* s = GetServerDE();
    if (m_hFlagAttach)
    {
        s->RemoveAttachment(m_hFlagAttach);
        m_hFlagAttach = 0;
    }
    if (!m_hFlagObject)
        return;
    s->BreakInterObjectLink(m_hObject, m_hFlagObject);
    auto* f = s->IsObjectAlive(m_hFlagObject) ? dynamic_cast<FlagObject*>(s->HandleToObject(m_hFlagObject)) : nullptr;
    if (f)
    {
        DVector position;
        if (s->GetObjectPos(m_hObject, &position) != DE_OK || s->TeleportObject(m_hFlagObject, &position) != DE_OK)
            ctf_error = "Flag drop pose unavailable";
        else
        {
            if (notify)
                NotifyCTF(CTFEventKind::Dropped);
            f->DropToGround();
        }
    }
    m_hFlagObject = nullptr;
}
