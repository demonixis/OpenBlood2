#include "HandWeaponModel.h"
#include "server_de.h"
DDWORD CHandWeaponModel::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (message == MID_INITIALUPDATE)
    {
        DVector dims{.1f, .1f, .1f};
        server->SetObjectDims(m_hObject, &dims);
        server->SetNextUpdate(m_hObject, .01f);
    }
    else if (message == MID_UPDATE && first_update)
    {
        first_update = false;
        auto flags = server->GetObjectFlags(m_hObject);
        server->SetObjectFlags(m_hObject, visible ? flags | FLAG_VISIBLE : flags & ~FLAG_VISIBLE);
        server->SetNextUpdate(m_hObject, 0);
    }
    return BaseClass::EngineMessageFn(message, data, reason);
}
