// Selected global helper from ObjectDLL/Spawner.cpp; not the Spawner class.
#include "spawn_object.h"
#include "cpp_engineobjects_de.h"
#include "game/key_messages.h"
#include "game/pickup_messages.h"
HOBJECT SpawnObject(std::string_view request, const DVector& position, const DRotation& rotation, DVector velocity)
{
    auto* server = BaseClass::GetServerDE();
    std::string name, props;
    if (!server || !server->ValidateSpawnObject(request) || !OpenBlood2::split_spawn(request, name, props))
        return nullptr;
    ObjectCreateStruct state;
    state.m_Pos = position;
    state.m_Rotation = rotation;
    auto* object = server->CreateObjectProps(server->GetClass(name.c_str()), &state, props.c_str());
    if (!object)
        return nullptr;
    auto handle = server->ObjectToHandle(object);
    if (!server->IsObjectAlive(handle) || server->SetVelocity(handle, &velocity) != DE_OK)
    {
        server->RemoveObject(handle);
        return nullptr;
    }
    return handle;
}
bool SpawnClassAbsentFromOriginal(std::string_view request)
{
    std::string name, props;
    if (!OpenBlood2::split_spawn(request, name, props))
        return false;
    for (const char* absent : {"RocketAmmoPU", "DieBugDieAmmo", "FareAmmoPU", "LifeEssencePU"})
        if (OpenBlood2::same_name(name, absent))
            return true;
    return false;
}
