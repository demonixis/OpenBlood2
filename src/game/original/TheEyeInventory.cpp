// Original GameInvItems.cpp CInvTheEye: first use throws; later uses toggle its camera.
#include "GameInvItems.h"
#include "PlayerObj.h"
#include "SeeingEye.h"
#include "engine/collision.h"
using namespace OpenBlood2;
CInvTheEye::~CInvTheEye()
{
    if (eye && g_pServerDE)
    {
        if (active)
            SetActive(false);
        g_pServerDE->RemoveObject(eye);
    }
}
bool CInvTheEye::SetActive(bool on)
{
    auto* p =
        g_pServerDE->IsObjectAlive(owner) ? dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(owner)) : nullptr;
    if (!p || (on && (!available || !g_pServerDE->IsObjectAlive(eye))))
        return false;
    if (g_pServerDE->SendToClient(p->GetClient(), SeeingEyeView{eye, on}) != DE_OK)
    {
        error = "SeeingEye camera message rejected";
        return false;
    }
    active = on;
    return true;
}
bool CInvTheEye::ActivateItem()
{
    error.clear();
    if (!available)
        return false;
    if (eye)
        return SetActive(!active);
    auto* p =
        g_pServerDE->IsObjectAlive(owner) ? dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(owner)) : nullptr;
    if (!p)
        return false;
    ObjectCreateStruct create;
    DVector dims;
    g_pServerDE->GetObjectPos(owner, &create.m_Pos);
    g_pServerDE->GetObjectDims(owner, &dims);
    create.m_Pos.y += dims.y / 2 - 1;
    create.m_Rotation = rotation_from_euler({p->m_fPitch, p->m_fYaw, 0});
    auto* object = dynamic_cast<SeeingEye*>(g_pServerDE->CreateObject(g_pServerDE->GetClass("SeeingEye"), &create));
    if (!object)
    {
        error = "SeeingEye creation failed";
        return false;
    }
    if (!object->Init(owner, p->GetClient()))
    {
        error = object->error;
        g_pServerDE->RemoveObject(object->m_hObject);
        return false;
    }
    eye = object->m_hObject;
    return true;
}
