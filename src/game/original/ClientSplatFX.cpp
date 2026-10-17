// Selected original ObjectDLL/ClientSplatFX.cpp; instant typed publication.
#include "ClientSplatFX.h"
#include "engine/objects.h"
#include "game/blood_splats.h"
using namespace OpenBlood2;
bool CClientSplatFX::Setup(DVector position, DVector direction, float scale, float growth)
{
    error.clear();
    BloodSplatSettings settings;
    settings.position = position;
    settings.scale = scale;
    settings.grow_scale = growth;
    // The source passes the same direction for forward and up.
    if (!g_pServerDE || !align_rotation(direction, direction, settings.rotation))
    {
        error = "Cannot align blood splat normal";
        return false;
    }
    event = g_pServerDE->PublishBloodSplat(m_hObject, settings, error);
    return event && error.empty();
}
DDWORD CClientSplatFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (!g_pServerDE)
        return 0;
    if (message == MID_PRECREATE && (!data || reason != PRECREATE_NORMAL))
        return 0; // Runtime-only subset: no world-file/save reconstruction.
    if (message == MID_INITIALUPDATE)
        g_pServerDE->SetNextUpdate(m_hObject, .0001f);
    else if (message == MID_UPDATE)
        g_pServerDE->RemoveObject(m_hObject);
    return CClientSFX::EngineMessageFn(message, data, reason);
}
