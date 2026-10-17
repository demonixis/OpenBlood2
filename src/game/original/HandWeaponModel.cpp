#include "HandWeaponModel.h"
#include "WeaponDefs.h"
#include "WeaponPickups.h"
#include "engine/collision.h"
#include "server_de.h"
#include <algorithm>
using namespace OpenBlood2;
bool CHandWeaponModel::Drop(HATTACHMENT attachment)
{
    auto* s = GetServerDE();
    drop_error.clear();
    HATTACHMENT actual = 0;
    DVector position, dims;
    DRotation rotation;
    if (!s || dropped || (type != WEAP_BERETTA && type != WEAP_SUBMACHINEGUN) || !s->IsObjectAlive(owner) ||
        !s->IsObjectAlive(m_hObject) || s->FindAttachment(owner, m_hObject, &actual) != DE_OK || actual != attachment ||
        s->GetObjectPos(owner, &position) != DE_OK || s->GetObjectRotation(owner, &rotation) != DE_OK ||
        s->GetModelAnimUserDims(m_hObject, &dims, s->GetModelAnimation(m_hObject)) != DE_OK || !physics_vector(dims) ||
        dims.x <= 0 || dims.y <= 0 || dims.z <= 0)
    {
        drop_error = "Drop requires a live owner, attachment and supported model dimensions";
        return false;
    }
    dims.x = dims.z = std::max({dims.x, dims.z, 10.f});
    // Original Drop's missing braces read an uninitialized rotation on success.
    // Always use the checked owner's rotation, after resetting the hand pose to it.
    auto velocity =
        vector_scale(vector_add(rotate_vector(rotation, {0, 0, 1}), rotate_vector(rotation, {0, 1, 0})), 150);
    if (s->RemoveAttachment(attachment) != DE_OK || s->TeleportObject(m_hObject, &position) != DE_OK ||
        s->SetObjectRotation(m_hObject, &rotation) != DE_OK || s->SetObjectDims(m_hObject, &dims) != DE_OK ||
        s->SetVelocity(m_hObject, &velocity) != DE_OK)
    {
        drop_error = "Dropped body setup failed";
        return false;
    }
    s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_NOSLIDING | FLAG_VISIBLE | FLAG_GRAVITY |
                                     FLAG_REMOVEIFOUTSIDE);
    dropped = true;
    owner = nullptr; // Independent lifetime; no weapon/inventory pointer survives.
    s->SetNextUpdate(m_hObject, 1.f);
    return true;
}
DDWORD CHandWeaponModel::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (message == MID_INITIALUPDATE)
    {
        DVector dims{.1f, .1f, .1f};
        server->SetObjectDims(m_hObject, &dims);
        server->SetNextUpdate(m_hObject, .01f);
    }
    else if (message == MID_UPDATE)
    {
        if (first_update)
        {
            first_update = false;
            auto flags = server->GetObjectFlags(m_hObject);
            server->SetObjectFlags(m_hObject, visible ? flags | FLAG_VISIBLE : flags & ~FLAG_VISIBLE);
        }
        if (!dropped)
            server->SetNextUpdate(m_hObject, 0);
        else if (drop_error.empty())
        {
            MovementState support;
            DVector position;
            DRotation rotation;
            if (!server->GetMovementState(m_hObject, support))
                drop_error = "Dropped weapon support query unavailable";
            else if (!support.standing)
                server->SetNextUpdate(m_hObject, .01f);
            else if (server->GetObjectPos(m_hObject, &position) != DE_OK ||
                     server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
                drop_error = "Dropped weapon pose unavailable";
            else
            {
                auto forward = rotate_vector(rotation, {0, 0, 1});
                forward.y = 0;
                position.y += 10;
                if (server->AlignRotation(rotation, forward) != DE_OK)
                    drop_error = "Dropped weapon horizontal facing unavailable";
                else
                {
                    pickup = SpawnWeaponPickup(type, m_hObject, position, rotation, !player_owned);
                    server->RecordSpawn({"CHandWeaponModel",
                                         type == WEAP_BERETTA ? "BerettaPU" : "SubMachineGunPU",
                                         pickup ? "Dropped weapon transferred" : "Dropped weapon pickup failed",
                                         pickup,
                                         position,
                                         {}});
                    if (pickup)
                        server->RemoveObject(m_hObject);
                    else
                        drop_error = "Dropped weapon pickup could not be created";
                }
            }
            if (!drop_error.empty())
            {
                DVector zero{};
                server->SetVelocity(m_hObject, &zero);
                server->SetObjectFlags(m_hObject, server->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY);
                server->SetNextUpdate(m_hObject, 0); // Inspectable failure; no fake transfer/retry duplication.
            }
        }
    }
    return BaseClass::EngineMessageFn(message, data, reason);
}
