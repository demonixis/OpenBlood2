// Selected original CameraObj callbacks and rules; see camera_contracts.md.
#include "CameraObj.h"
#include "trigger_helpers.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
DLink CameraObj::m_CameraHead{};
DDWORD CameraObj::m_dwNumCameras = 0;
HOBJECT g_hActiveCamera = nullptr;
CameraObj::CameraObj() : B2BaseClass(OT_NORMAL)
{
    if (!m_dwNumCameras)
        dl_TieOff(&m_CameraHead);
}
CameraObj::~CameraObj()
{
    if (g_pServerDE)
    {
        SetActive(DFALSE);
        g_pServerDE->RemoveCameraEffect(m_hObject);
    }
    if (m_Link.m_pData && m_dwNumCameras)
    {
        dl_Remove(&m_Link);
        --m_dwNumCameras;
    }
}
bool CameraObj::ReadProp()
{
    GenericProp p;
    if (g_pServerDE->GetPropGeneric("AllowPlayerMovement", &p) == DE_OK)
        m_bPlayerMovement = p.m_Bool;
    if (g_pServerDE->GetPropGeneric("StartActive", &p) == DE_OK)
        m_bStartActive = p.m_Bool;
    if (g_pServerDE->GetPropGeneric("IsListener", &p) == DE_OK)
        m_bIsListener = p.m_Bool;
    if (g_pServerDE->GetPropGeneric("HidePlayer", &p) == DE_OK)
        m_bHidePlayer = p.m_Bool;
    if (g_pServerDE->GetPropGeneric("ActiveTime", &p) == DE_OK)
        m_fActiveTime = p.m_Float;
    if (g_pServerDE->GetPropGeneric("Type", &p) == DE_OK)
    {
        if (p.m_Long < 0 || p.m_Long > 2)
            return false;
        m_nType = static_cast<DBYTE>(p.m_Long);
    }
    return std::isfinite(m_fActiveTime) && std::abs(m_fActiveTime) <= 1e6f;
}
void CameraObj::InitialUpdate()
{
    SetActive(m_bStartActive);
    published = {m_nType, m_bPlayerMovement, m_bHidePlayer, m_bIsListener};
    // Preserve the source b2 1.0 level workaround, after path normalization.
    if (const auto var = g_pServerDE->GetGameConVar("GAME_WORLDNAME"))
        if (const auto* value = g_pServerDE->GetVarValueString(var))
        {
            std::string world = value;
            for (char& c : world)
            {
                if (c == '/')
                    c = '\\';
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c + 'a' - 'A');
            }
            if (world.find("worlds\\") != std::string::npos)
                published.listener = false;
        }
    if (!g_pServerDE->PublishCameraEffect(m_hObject, published))
    {
        g_pServerDE->RemoveObject(m_hObject);
        return;
    }
    g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 3));
}
DDWORD CameraObj::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!data || !B2BaseClass::EngineMessageFn(message, data, reason) || reason == 3 ||
            ((reason == 1 || reason == 2) && !ReadProp()))
            return 0;
        auto& state = *static_cast<ObjectCreateStruct*>(data);
        state.m_Flags = FLAG_FORCECLIENTUPDATE | FLAG_FULLPOSITIONRES | FLAG_GOTHRUWORLD;
        if (g_pServerDE->GetVarValueFloat(g_pServerDE->GetGameConVar("DebugCutScene")) == 1)
        {
            state.m_ObjectType = OT_MODEL;
            state.m_Flags |= FLAG_VISIBLE;
            SAFE_STRCPY(state.m_Filename, "Models\\CameraObject.abc");
            SAFE_STRCPY(state.m_SkinName, "Skins\\CameraObject.dtx");
        }
        state.m_NextUpdate = .001f;
        return 1;
    }
    if (message == MID_INITIALUPDATE)
    {
        InitialUpdate();
        if (!m_Link.m_pData && g_pServerDE->IsObjectAlive(m_hObject))
        {
            dl_Insert(&m_CameraHead, &m_Link);
            m_Link.m_pData = this;
            ++m_dwNumCameras;
        }
    }
    else if (message == MID_LINKBROKEN && m_hLinkObject == static_cast<HOBJECT>(data))
        m_hLinkObject = nullptr;
    else if (message == MID_UPDATE)
        Update();
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
void CameraObj::Update()
{
    g_pServerDE->SetNextUpdate(m_hObject, .01f);
    if (m_hLinkObject && g_pServerDE->IsObjectAlive(m_hLinkObject))
    {
        DVector target, from;
        g_pServerDE->GetObjectPos(m_hLinkObject, &target);
        g_pServerDE->GetObjectPos(m_hObject, &from);
        DRotation rotation;
        const DVector forward{target.x - from.x, target.y - from.y, target.z - from.z};
        if (g_pServerDE->AlignRotation(rotation, forward) != DE_OK ||
            g_pServerDE->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            tracking_error = "Cannot orient camera toward target (coincident or invalid positions)";
        else
        {
            tracking_error.clear();
            ++tracking_updates;
        }
    }
    if (m_fActiveTime > 0 && g_pServerDE->GetTime() > m_fDeactivateTime)
        SetActive(DFALSE);
}
void CameraObj::SetActive(DBOOL active)
{
    if (!g_pServerDE)
        return;
    m_bActive = active != 0;
    const auto flags = g_pServerDE->GetObjectUserFlags(m_hObject);
    g_pServerDE->SetObjectUserFlags(m_hObject, m_bActive ? flags | 1 : flags & ~1u);
    g_pServerDE->SetNextUpdate(m_hObject, m_bActive ? .01f : 0);
    if (m_bActive)
    {
        if (m_fActiveTime > 0)
            m_fDeactivateTime = g_pServerDE->GetTime() + m_fActiveTime;
    }
    else
        m_fDeactivateTime = 0;
    if (m_bIsListener && m_bActive)
        g_hActiveCamera = m_hObject;
    else
    {
        g_hActiveCamera = nullptr;
        for (auto* link = m_CameraHead.m_pNext; link != &m_CameraHead; link = link->m_pNext)
        {
            auto* camera = static_cast<CameraObj*>(link->m_pData);
            if (camera->IsActive() && camera->IsListener() && g_pServerDE->IsObjectAlive(camera->m_hObject))
            {
                g_hActiveCamera = camera->m_hObject;
                break;
            }
        }
    }
}
bool CameraObj::Setup(DBYTE type)
{
    if (type > 2)
        return false;
    m_nType = type;
    return true; // Source does not republish an already-created camera effect.
}
bool CameraObj::SetLinkObject(HOBJECT target)
{
    if (target)
    {
        if (!g_pServerDE->IsObjectAlive(target) || target == m_hObject ||
            g_pServerDE->CreateInterObjectLink(m_hObject, target) != DE_OK)
            return false;
        // Source forgot to break an old, replaced target. Keep one checked link.
        if (m_hLinkObject && m_hLinkObject != target)
            g_pServerDE->BreakInterObjectLink(m_hObject, m_hLinkObject);
        if (m_bActive)
            g_pServerDE->SetNextUpdate(m_hObject, .01f);
    }
    else
    {
        if (m_hLinkObject)
            g_pServerDE->BreakInterObjectLink(m_hObject, m_hLinkObject);
        g_pServerDE->SetNextUpdate(m_hObject, 0); // Original unlink also stops the timeout update.
    }
    m_hLinkObject = target;
    return true;
}
DDWORD CameraObj::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto text = g_pServerDE->ReadFromMessageHString(read);
        const auto* raw = g_pServerDE->GetStringData(text);
        last_command = raw ? raw : "";
        g_pServerDE->FreeString(text);
        if (same_name(last_command, "ON"))
            SetActive(DTRUE);
        else if (same_name(last_command, "OFF"))
            SetActive(DFALSE);
        else if (last_command.size() >= 4 && same_name(last_command.substr(0, 4), "LINK"))
        {
            if (last_command.size() >= 6)
            {
                auto* list = g_pServerDE->FindNamedObjects(last_command.c_str() + 5);
                if (list && list->m_pFirstLink)
                    SetLinkObject(list->m_pFirstLink->m_hObject);
                if (list)
                    g_pServerDE->RelinquishList(list);
            }
            else
                SetLinkObject(nullptr);
        }
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
