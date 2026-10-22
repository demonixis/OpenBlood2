// Selected from original ObjectDLL/Trigger.cpp; see Documentation/implementation/provenance.md.
#include "trigger_helpers.h"
#include "engine/objects.h"
#include "game/legacy/cpp_engineobjects_de.h"
#include <vector>
inline constexpr DDWORD MID_TRIGGER = 1000;

inline void SendTriggerMsgToObject(LPBASECLASS pSender, HOBJECT hObj, DBOOL bRemoveObject, HSTRING hMsg)
{
    if (!g_pServerDE)
        return;

    if (bRemoveObject)
    {
        g_pServerDE->SetObjectUserFlags(hObj, 0); // Quick hack to fix airship door load/saves
        g_pServerDE->RemoveObject(hObj);
    }
    else
    {
        HMESSAGEWRITE hMessage;

        hMessage = g_pServerDE->StartMessageToObject(pSender, hObj, MID_TRIGGER);
        g_pServerDE->WriteToMessageHString(hMessage, hMsg);
        g_pServerDE->EndMessage(hMessage);
    }
}

void SendTriggerMsgToObjects(LPBASECLASS pSender, HSTRING hName, HSTRING hMsg)
{
    if (!g_pServerDE)
        return;

    DBOOL bRemoveObject = DFALSE;

    char* pMsg = g_pServerDE->GetStringData(hMsg);
    char* pName = g_pServerDE->GetStringData(hName);

    if (!pMsg || !pName || !pName[0])
        return;

    // See if it's a remove trigger
    if (!OpenBlood2::CaseLess{}(pMsg, "REMOVE") && !OpenBlood2::CaseLess{}("REMOVE", pMsg))
    {
        bRemoveObject = DTRUE;
    }

    ObjectList* pList = g_pServerDE->FindNamedObjects(pName);
    if (!pList)
        return;

    ObjectLink* pLink = pList->m_pFirstLink;
    while (pLink)
    {
        if (pLink)
        {
            SendTriggerMsgToObject(pSender, pLink->m_hObject, bRemoveObject, hMsg);
        }

        pLink = pLink->m_pNext;
    }

    g_pServerDE->RelinquishList(pList);
}

void SendTriggerMsgToClass(LPBASECLASS pSender, HCLASS hClass, HSTRING hMsg)
{
    if (!g_pServerDE)
        return;

    DBOOL bRemoveObject = DFALSE;

    char* pMsg = g_pServerDE->GetStringData(hMsg);

    if (!pMsg || !hClass)
        return;

    // See if it's a remove trigger
    if (!OpenBlood2::CaseLess{}(pMsg, "REMOVE") && !OpenBlood2::CaseLess{}("REMOVE", pMsg))
    {
        bRemoveObject = DTRUE;
    }

    // The source walks the active, then the inactive objects while sending. Collect them
    // first: a trigger may create objects (corpses) or remove its receiver during the walk.
    std::vector<HOBJECT> targets;
    for (HOBJECT hObj = g_pServerDE->GetNextObject(DNULL); hObj; hObj = g_pServerDE->GetNextObject(hObj))
        if (g_pServerDE->IsKindOf(g_pServerDE->GetObjectClass(hObj), hClass))
            targets.push_back(hObj);
    for (HOBJECT hObj = g_pServerDE->GetNextInactiveObject(DNULL); hObj;
         hObj = g_pServerDE->GetNextInactiveObject(hObj))
        if (g_pServerDE->IsKindOf(g_pServerDE->GetObjectClass(hObj), hClass))
            targets.push_back(hObj);
    for (HOBJECT hObj : targets)
        if (g_pServerDE->IsObjectAlive(hObj))
            SendTriggerMsgToObject(pSender, hObj, bRemoveObject, hMsg);
}
