// Selected global helper from ObjectDLL/Spawner.cpp; not the Spawner class.
#include "spawn_object.h"
#include "cpp_engineobjects_de.h"
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
