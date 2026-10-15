#include "KeyData.h"
#include "engine/collision.h"
#include <cmath>
bool KeyData::Copy(const Key& key)
{
    auto* server = g_pServerDE;
    if (!server || !server->IsObjectAlive(key.m_hObject))
        return false;
    server->GetObjectPos(key.m_hObject, &position);
    if (server->GetObjectRotation(key.m_hObject, &rotation) != DE_OK)
        return false;
    name = server->GetObjectName(key.m_hObject);
    timestamp = key.m_fTimeStamp;
    sound_radius = key.m_fSoundRadius;
    loop_sound = key.m_bLoopSound;
    auto copy = [server](HSTRING h)
    {
        const auto* s = server->GetStringData(h);
        return std::string(s ? s : "");
    };
    sound = copy(key.m_hstrSoundName);
    message_target = copy(key.m_hstrMessageTarget);
    message = copy(key.m_hstrMessageName);
    print = copy(key.m_hstrBPrintMessage);
    return OpenBlood2::physics_vector(position) && std::isfinite(timestamp) && std::abs(timestamp) <= 1e6f &&
           std::isfinite(sound_radius) && sound_radius >= 0 && sound_radius <= 1e6f;
}
