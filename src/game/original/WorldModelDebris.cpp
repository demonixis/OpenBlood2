// Original WorldModelDebris rules; see Documentation/contracts/world_debris_contracts.md.
#include "WorldModelDebris.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
WorldModelDebris::WorldModelDebris()
{
    AddAggregate(&rotating);
    dynamic_debris = true;
}
bool WorldModelDebris::Fail(const char* why)
{
    failed = true;
    rotating.SetOff();
    error = why;
    GetServerDE()->SetNextUpdate(m_hObject, 0);
    return false;
}
bool WorldModelDebris::Start()
{
    auto* server = GetServerDE();
    if (!ready || !server->IsObjectAlive(m_hObject))
        return false;
    m_damage.Heal(10000);
    rotating.SetSpinUp();
    on = true;
    // Intentionally replaces ALL flags, including BOXPHYSICS and DONTFOLLOWSTANDING.
    const auto flags = FLAG_VISIBLE | FLAG_GRAVITY | FLAG_RAYHIT | FLAG_SOLID;
    server->SetObjectFlags(m_hObject, flags);
    if (server->GetObjectFlags(m_hObject) != flags || server->SetVelocity(m_hObject, &initial_velocity) != DE_OK ||
        !server->SetBlockingPriority(m_hObject, 100))
        return Fail("World debris start services failed");
    // Source SetForceIgnoreLimit(0) requests every contact. The replacement body
    // dispatch already retains all contacts; no force estimate is fabricated.
    failed = false;
    error.clear();
    ++starts;
    server->SetNextUpdate(m_hObject, .01f);
    return true;
}
void WorldModelDebris::Update()
{
    auto* server = GetServerDE();
    if (failed)
        return;
    if (on)
    {
        DVector gravity;
        if (server->GetGlobalForce(&gravity) != DE_OK)
        {
            Fail("World debris global force unavailable");
            return;
        }
        auto acceleration = vector_scale(gravity, gravity_multiplier - 1);
        MovementState support;
        if (server->SetAcceleration(m_hObject, &acceleration) != DE_OK || !server->GetMovementState(m_hObject, support))
        {
            Fail("World debris gravity/support services failed");
            return;
        }
        server->SetNextUpdate(m_hObject, .01f);
        // The typed standing bit includes the primary world (whose handle is null).
        if (support.standing)
        {
            on = false;
            ++landings;
            rotating.SetSpinDown();
        }
    }
    else
        server->SetNextUpdate(m_hObject, 0);
}
DDWORD WorldModelDebris::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (message == MID_PRECREATE)
    {
        const auto accepted = CDestructableBrush::EngineMessageFn(message, data, reason);
        if (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP)
        {
            server->GetPropReal("GravityMultiplier", &gravity_multiplier);
            GenericProp prop;
            if (server->GetPropGeneric("InitialVelocity", &prop) == DE_OK)
                initial_velocity = prop.m_Vec;
        }
        return accepted && rotating.valid && physics_vector(initial_velocity) && std::isfinite(gravity_multiplier) &&
               std::abs(gravity_multiplier) <= 1000;
    }
    if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME)
    {
        rotating.Init(m_hObject);
        rotating.SetOff(); // Overrides authored StartOn before aggregate INITIALUPDATE.
    }
    if (message == MID_SAVEOBJECT && data)
    {
        // Original WorldModelDebris.cpp:207-216, before the CDestructableBrush block.
        const auto h = *static_cast<HMESSAGEWRITE*>(data);
        server->WriteToMessageVector(h, &initial_velocity);
        server->WriteToMessageByte(h, on);
        server->WriteToMessageFloat(h, gravity_multiplier);
    }
    if (message == MID_LOADOBJECT && data)
    {
        // Original WorldModelDebris.cpp:226-235. Velocity and acceleration are engine record fields.
        const auto h = *static_cast<HMESSAGEREAD*>(data);
        DVector velocity{};
        bool saved_on = false;
        server->ReadFromMessageVector(h, &velocity);
        const bool ok = OpenBlood2::save_fields::read_bool(h, saved_on);
        const float multiplier = server->ReadFromMessageFloat(h);
        if (ok && (!physics_vector(velocity) || !std::isfinite(multiplier) || std::abs(multiplier) > 1000))
            server->RejectPayload(h, "world debris block holds values the port cannot represent");
        else if (ok)
        {
            initial_velocity = velocity;
            on = saved_on;
            gravity_multiplier = multiplier;
        }
    }
    if (message == MID_UPDATE)
        Update();
    const auto accepted = CDestructableBrush::EngineMessageFn(message, data, reason);
    if (message == MID_INITIALUPDATE && server->IsObjectAlive(m_hObject))
    {
        ready = accepted != 0; // Parent configured this specialized body before aggregate initialization.
    }
    if (failed)
        server->SetNextUpdate(m_hObject, 0); // Aggregates must not conceal a failed service.
    return accepted;
}
