// Original GameInvItems.cpp CInvFlashlight and ClientShellDLL/FlashlightFX.cpp.
#include "Flashlight.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
CInvFlashlight::~CInvFlashlight()
{
    if (light)
        g_pServerDE->RemoveObject(light);
}
bool CInvFlashlight::CreateLight()
{
    if (!g_pServerDE->IsObjectAlive(owner))
    {
        error = "Flashlight owner unavailable";
        return false;
    }
    if (light)
        g_pServerDE->RemoveObject(light);
    light = nullptr;
    ObjectCreateStruct create;
    create.m_Flags = FLAG_FORCECLIENTUPDATE;
    create.m_ObjectType = OT_NORMAL;
    auto* object = g_pServerDE->CreateObject(g_pServerDE->GetClass("BaseClass"), &create);
    if (!object)
    {
        error = "Flashlight child creation failed";
        return false;
    }
    light = object->m_hObject;
    return true;
}
bool CInvFlashlight::AddItem(DBYTE)
{
    if (count >= 100)
        return false;
    count = 100;
    remaining = 120; // Source ignores incoming quantity; rounded 100 refuses recharge.
    return true;
}
bool CInvFlashlight::SetActive(bool on)
{
    error.clear();
    if (!g_pServerDE->IsObjectAlive(owner))
        return false;
    if (on && !light && !CreateLight())
        return false;
    if (!g_pServerDE->IsObjectAlive(light))
    {
        error = "Flashlight child unavailable";
        return false;
    }
    if (on && remaining <= 0)
        return false; // Source creates the child before refusing an empty battery.
    auto flags = g_pServerDE->GetObjectUserFlags(light);
    g_pServerDE->SetObjectUserFlags(light, on ? flags | 1u : flags & ~1u);
    active = on;
    if (auto* player = dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(owner)))
    {
        if (g_pServerDE->SendToClient(player->GetClient(), InventoryAction{2, on}) == DE_OK)
            ++actions;
        else
            error = "Flashlight action could not reach its client";
    }
    return true;
}
bool CInvFlashlight::ActivateItem()
{
    return SetActive(!active); // Original ActivateItem returns true after deactivation too.
}
bool CInvFlashlight::Update()
{
    ++updates;
    update_ok = false;
    const float dt = g_pServerDE->GetFrameTime();
    if (!g_pServerDE->IsObjectAlive(owner) || !std::isfinite(dt) || dt < 0 || dt > 60)
    {
        error = "Invalid flashlight owner/frame time";
        return false;
    }
    if (light)
    {
        DVector position;
        DRotation rotation;
        if (!g_pServerDE->IsObjectAlive(light) || g_pServerDE->GetObjectPos(owner, &position) != DE_OK ||
            g_pServerDE->GetObjectRotation(owner, &rotation) != DE_OK)
        {
            error = "Flashlight child/owner pose unavailable";
            return false;
        }
        if (auto* player = dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(owner)))
            rotation = rotation_from_euler({player->m_fPitch, player->m_fYaw, 0});
        if (g_pServerDE->TeleportObject(light, &position) != DE_OK ||
            g_pServerDE->SetObjectRotation(light, &rotation) != DE_OK)
        {
            error = "Flashlight child pose rejected";
            return false;
        }
    }
    if (active)
    {
        remaining -= dt;
        if (remaining <= 0)
        {
            count = 0;
            return update_ok = SetActive(false);
        }
        count = static_cast<DBYTE>(100 * remaining / 120 + .5f);
    }
    return update_ok = true;
}
bool CFlashlightFX::Update(ServerDE& server, HOBJECT child, HOBJECT character)
{
    ++updates;
    source = child;
    owner = character;
    visible = false;
    error.clear();
    if (!source)
        return true;
    if (!server.IsObjectAlive(source) || !server.IsObjectAlive(owner))
        return true; // Original removed SFX has no visible client light.
    if (!(server.GetObjectUserFlags(source) & 1))
        return true;
    DVector position;
    DRotation rotation;
    if (server.GetObjectPos(source, &position) != DE_OK || server.GetObjectRotation(source, &rotation) != DE_OK)
    {
        error = "Flashlight pose unavailable";
        return false;
    }
    result = server.CastRay(position, rotate_vector(rotation, {0, 0, 1}), hit, owner);
    if (result == SegmentResult::Miss)
        return true;
    if (result != SegmentResult::Hit)
    {
        error = "Flashlight ray unavailable";
        return false;
    }
    shade_position = vector_subtract(position, hit.point);
    distance = std::max(10.f, std::sqrt(vector_dot(shade_position, shade_position)));
    // GetPointShade is unavailable; preserve the source's explicit failure fallback.
    // No successful shade service or baked-lighting parity is claimed.
    shade_available = false;
    minimum_intensity = 132.f / 255;
    const float intensity = std::max(minimum_intensity, (1000.f - distance / 2) / 1000);
    light = {hit.point, {intensity, intensity, intensity}, distance * .4f};
    visible = true;
    return true;
}
PointLight CFlashlightFX::Current(ServerDE& server) const
{
    return visible && server.IsObjectAlive(source) && server.IsObjectAlive(owner) &&
                   (server.GetObjectUserFlags(source) & 1)
               ? light
               : PointLight{};
}
