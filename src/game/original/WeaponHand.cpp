// Selected original Weapon.cpp ShowHandModel/CreateHandModel/destructor rules.
#include "HandWeaponModel.h"
#include "Weapon.h"
#include "server_de.h"
CWeapon::~CWeapon()
{
    ShowHandModel(false);
}
bool CWeapon::ShowHandModel(DBOOL show)
{
    auto* server = BaseClass::GetServerDE();
    if (!server)
        return !m_hHandModel && !m_hHandAttachment;
    if (show && server->IsObjectAlive(m_hHandModel))
    {
        HATTACHMENT current = 0;
        if (server->FindAttachment(m_hOwner, m_hHandModel, &current) == DE_OK && current == m_hHandAttachment)
            return true;
    }
    if (m_hHandAttachment)
        server->RemoveAttachment(m_hHandAttachment);
    m_hHandAttachment = 0;
    if (server->IsObjectAlive(m_hHandModel))
        server->RemoveObject(m_hHandModel);
    m_hHandModel = nullptr;
    hand_error.clear();
    if (!show)
        return true;
    if (!server->IsObjectAlive(m_hOwner))
    {
        hand_error = "Hand model requires a live weapon owner";
        return false;
    }
    ObjectCreateStruct state;
    state.m_ObjectType = OT_MODEL;
    state.m_Flags = FLAG_VISIBLE | FLAG_FORCECLIENTUPDATE;
    server->GetObjectPos(m_hOwner, &state.m_Pos);
    SAFE_STRCPY(state.m_Filename, data.m_szHandModelFilename);
    SAFE_STRCPY(state.m_SkinName, data.m_szHandModelSkin);
    SAFE_STRCPY(state.m_Name, m_bLeftHand ? "Left held weapon" : "Right held weapon");
    auto* model = dynamic_cast<CHandWeaponModel*>(server->CreateObject(server->GetClass("CHandWeaponModel"), &state));
    if (!model || server->GetModelAnimation(model->m_hObject) == INVALID_ANI)
    {
        if (model)
            server->RemoveObject(model->m_hObject);
        hand_error = "Original hand model could not be loaded";
        return false;
    }
    const auto h = model->m_hObject;
    const auto anim = server->GetAnimIndex(h, "handheld");
    if (anim != INVALID_ANI)
        server->SetModelAnimation(h, anim);
    model->type = data.m_nType;
    model->left = m_bLeftHand;
    model->owner = m_hOwner;
    model->player_owned = server->IsKindOf(server->GetObjectClass(m_hOwner), server->GetClass("CPlayerObj"));
    DVector position{}, offset{};
    DRotation rotation, identity;
    const char* node = m_bLeftHand ? "l_gun" : "r_gun";
    const bool found = server->GetModelNodeTransform(m_hOwner, node, &position, &rotation);
    const auto result =
        server->CreateAttachment(m_hOwner, h, found ? node : nullptr, &offset, &identity, &m_hHandAttachment);
    if (result != DE_OK)
    {
        server->RemoveObject(h);
        hand_error = "Original hand model attachment failed";
        return false;
    }
    model->SetVisible(found);
    m_hHandModel = h;
    return true;
}
