// Selected original ObjectDLL/ClientSmokeTrail.cpp setup.
#include "ClientSmokeTrail.h"
#include "engine/collision.h"

bool CClientSmokeTrail::Setup(DVector initial_velocity, DBOOL small_trail)
{
    auto* server = GetServerDE();
    if (configured || !server || !server->IsObjectAlive(m_hObject) || !OpenBlood2::physics_vector(initial_velocity))
        return false;
    velocity = initial_velocity;
    small = small_trail != 0;
    configured = true;
    return true;
}
