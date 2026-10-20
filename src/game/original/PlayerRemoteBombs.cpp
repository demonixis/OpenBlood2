#include "PlayerObj.h"
#include "RemoteBomb.h"
bool CPlayerObj::AddRemoteBomb(HOBJECT h)
{
    remote_error.clear();
    auto* bomb = g_pServerDE->IsObjectAlive(h) ? dynamic_cast<CRemoteBomb*>(g_pServerDE->HandleToObject(h)) : nullptr;
    if (!g_pServerDE->IsObjectAlive(m_hObject) || !bomb || !bomb->configured || bomb->m_hFiredFrom != m_hObject)
    {
        remote_error = "Invalid remote bomb owner or projectile";
        return false;
    }
    if (remote_bombs)
        for (auto* link = remote_bombs->m_pFirstLink; link; link = link->m_pNext)
            if (link->m_hObject == h)
                return true; // Checked set ownership: do not register a projectile twice.
    const bool created = !remote_bombs;
    if (created)
        remote_bombs = g_pServerDE->CreateObjectList();
    if (!remote_bombs || !g_pServerDE->AddObjectToList(remote_bombs, h))
    {
        remote_error = "Remote bomb list unavailable";
        if (created && remote_bombs)
        {
            g_pServerDE->RelinquishList(remote_bombs);
            remote_bombs = nullptr;
        }
        return false;
    }
    if (g_pServerDE->CreateInterObjectLink(m_hObject, h) != DE_OK)
    {
        RemoveRemoteBomb(h);
        if (created)
        {
            g_pServerDE->RelinquishList(remote_bombs);
            remote_bombs = nullptr;
        }
        remote_error = "Remote bomb owner link unavailable";
        return false;
    }
    return true;
}
bool CPlayerObj::RestoreRemoteBomb(HOBJECT h)
{
    // Save/load lot 9: list a restored charge again. The player->charge link is a saved interlink
    // that the restore re-creates after every Load, so only the list entry is rebuilt here.
    remote_error.clear();
    auto* bomb = g_pServerDE->IsObjectAlive(h) ? dynamic_cast<CRemoteBomb*>(g_pServerDE->HandleToObject(h)) : nullptr;
    if (!g_pServerDE->IsObjectAlive(m_hObject) || !bomb || !bomb->configured || bomb->m_hFiredFrom != m_hObject)
    {
        remote_error = "Invalid restored remote bomb owner or projectile";
        return false;
    }
    if (remote_bombs)
        for (auto* link = remote_bombs->m_pFirstLink; link; link = link->m_pNext)
            if (link->m_hObject == h)
                return true;
    const bool created = !remote_bombs;
    if (created)
        remote_bombs = g_pServerDE->CreateObjectList();
    if (!remote_bombs || !g_pServerDE->AddObjectToList(remote_bombs, h))
    {
        remote_error = "Remote bomb list unavailable";
        if (created && remote_bombs)
        {
            g_pServerDE->RelinquishList(remote_bombs);
            remote_bombs = nullptr;
        }
        return false;
    }
    return true;
}
void CPlayerObj::RemoveRemoteBomb(HOBJECT h)
{
    if (!remote_bombs)
        return;
    g_pServerDE->RemoveObjectFromList(remote_bombs, h);
    g_pServerDE->BreakInterObjectLink(m_hObject, h);
}
void CPlayerObj::ClearRemoteBombs()
{
    if (!remote_bombs)
        return;
    // Removal is deferred; no callback can free this list during traversal.
    auto* list = remote_bombs;
    remote_bombs = nullptr;
    for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
        g_pServerDE->RemoveObject(link->m_hObject);
    g_pServerDE->RelinquishList(list);
}
bool CPlayerObj::OnCommandDetonate()
{
    if (IsDemoBuild())
        return true; // Original _DEMO compiles COMMAND_DETONATE out: the command does nothing.
    if (!g_pServerDE->IsObjectAlive(m_hObject) || m_bDead || m_damage.IsDead() || MovementBlocked() ||
        camera_movement_blocked)
        return false;
    ++remote_commands;
    remote_error.clear();
    if (!remote_bombs)
        return true;
    bool ok = true;
    for (auto* link = remote_bombs->m_pFirstLink; link; link = link->m_pNext)
    {
        // The original command marks all bombs, breaks owner links, then releases
        // the list. Damage and list mutation happen only in later updates.
        auto* bomb = g_pServerDE->IsObjectAlive(link->m_hObject)
                         ? dynamic_cast<CRemoteBomb*>(g_pServerDE->HandleToObject(link->m_hObject))
                         : nullptr;
        if (bomb && bomb->m_hFiredFrom == m_hObject)
        {
            if (bomb->Detonate())
                ++remote_signals;
            else
                ok = false;
        }
        g_pServerDE->BreakInterObjectLink(m_hObject, link->m_hObject);
    }
    g_pServerDE->RelinquishList(remote_bombs);
    remote_bombs = nullptr;
    if (!ok)
        remote_error = "A live remote bomb rejected detonation";
    return ok;
}
