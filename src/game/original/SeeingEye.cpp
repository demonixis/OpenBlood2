// Original ObjectDLL/SeeingEye.cpp, with the shared bounded model-flight integrator.
#include "SeeingEye.h"
#include "PlayerObj.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
SeeingEye::SeeingEye() : CProjectile(OT_MODEL)
{
    filename = "Models\\Powerups\\theeye_pu.abc";
    skin = "Skins\\Powerups\\theeye_pu.dtx";
    flags = FLAG_VISIBLE | FLAG_GRAVITY | FLAG_SOLID | FLAG_TOUCH_NOTIFY | FLAG_NOSLIDING | FLAG_REMOVEIFOUTSIDE;
    m_fLifeTime = 0;
}
SeeingEye::~SeeingEye()
{
    RemoveEye(true);
}
bool SeeingEye::InitialUpdate()
{
    if (!CProjectile::InitialUpdate())
        return false;
    g_pServerDE->SetObjectUserFlags(m_hObject, 1u << 3); // SeeingEye is saveable, not a weapon projectile target.
    m_damage.SetHitPoints(10);
    m_damage.m_fMaxHitPoints = 10;
    m_damage.SetArmorPoints(0);
    m_damage.m_fMaxArmorPoints = 0;
    m_damage.m_fMass = 30;
    DVector dims{7, 7, 7};
    return g_pServerDE->SetObjectDims(m_hObject, &dims) == DE_OK;
}
bool SeeingEye::Init(HOBJECT owner, HCLIENT handle)
{
    DRotation rotation;
    if (g_pServerDE->GetClientUserData(handle) != owner ||
        g_pServerDE->GetObjectRotation(m_hObject, &rotation) != DE_OK ||
        !Setup(rotate_vector(rotation, {0, 0, 1}), 0, 0, 300, 0, owner))
        return false;
    // Dropit overwrites InitialUpdate's randomized toss with forward * 300.
    client = handle;
    return true;
}
bool SeeingEye::AdvanceFlight(double seconds)
{
    if (!configured || dropped || removing_ || seconds == 0)
        return !failed;
    return g_pServerDE->AdvanceModelProjectile(m_hObject, seconds) || Fail("SeeingEye flight unavailable");
}
void SeeingEye::HandleContact(const SegmentHit& hit)
{
    if (removing_ || !configured)
        return;
    if (dropped)
    {
        if (hit.object == m_hFiredFrom)
            RemoveEye(false);
        return;
    }
    if (hit.object == m_hFiredFrom || (hit.object && !(g_pServerDE->GetObjectFlags(hit.object) & FLAG_SOLID)) ||
        (!hit.object && hit.polygon == UINT32_MAX))
        return;
    dropped = true;
    DVector zero{};
    g_pServerDE->SetVelocity(m_hObject, &zero);
    g_pServerDE->SetAcceleration(m_hObject, &zero);
    if (hit.object)
    {
        stuck = hit.object;
        g_pServerDE->CreateInterObjectLink(m_hObject, stuck);
        g_pServerDE->SetObjectFlags(m_hObject, flags & ~(FLAG_SOLID | FLAG_GRAVITY));
    }
}
bool SeeingEye::Update()
{
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
    if (!g_pServerDE->IsObjectAlive(m_hFiredFrom) || m_damage.IsDead())
    {
        RemoveEye(true);
        return true;
    }
    if (g_pServerDE->GetTime() > next_ping)
    {
        next_ping = g_pServerDE->GetTime() + 1;
        if (!g_pServerDE->PingObjects(m_hObject))
            return Fail("SeeingEye activation ping unavailable");
    }
    if (stuck)
    {
        DVector pos, dims;
        if (!g_pServerDE->IsObjectAlive(stuck))
            RemoveEye(false);
        else if (g_pServerDE->GetObjectPos(stuck, &pos) == DE_OK && g_pServerDE->GetObjectDims(stuck, &dims) == DE_OK)
        {
            pos.y += dims.y / 2 - 10;
            pos.z -= 10;
            g_pServerDE->TeleportObject(m_hObject, &pos);
        }
    }
    if (dropped && !removing_)
    {
        // PingObjects owner overlap: source retries pickup even when both objects stand still.
        DVector at, pos, dims;
        if (g_pServerDE->GetObjectPos(m_hObject, &at) == DE_OK &&
            g_pServerDE->GetObjectPos(m_hFiredFrom, &pos) == DE_OK &&
            g_pServerDE->GetObjectDims(m_hFiredFrom, &dims) == DE_OK && std::abs(at.x - pos.x) <= dims.x + 7 &&
            std::abs(at.y - pos.y) <= dims.y + 7 && std::abs(at.z - pos.z) <= dims.z + 7)
            RemoveEye(false);
    }
    return true;
}
void SeeingEye::RemoveEye(bool destroyed)
{
    if (removing_ || !g_pServerDE)
        return;
    removing_ = true;
    if (auto* owner = g_pServerDE->IsObjectAlive(m_hFiredFrom)
                          ? dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(m_hFiredFrom))
                          : nullptr)
        if (auto* item = owner->m_InventoryMgr.eye.get(); item && item->eye == m_hObject)
        {
            const bool active = item->active;
            // Departure: a restored eye has no saved client (the original's stays NULL and leaves
            // an active view on the removed eye); it reaches the owner's current client instead.
            g_pServerDE->SendToClient(client ? client : owner->GetClient(),
                                      SeeingEyeView{m_hObject, false, destroyed && active ? 10.f : 0.f});
            item->eye = nullptr;
            item->active = false;
            item->available = !destroyed; // Executable source Remove(true) consumes the item.
            ++owner->m_InventoryMgr.revision;
            if (!destroyed)
            {
                DVector position;
                g_pServerDE->GetObjectPos(m_hObject, &position);
                SoundRequest request;
                request.resource = "sounds\\powerups\\inventory1.wav";
                request.inner_radius = 30;
                request.outer_radius = 300;
                request.priority = 2;
                g_pServerDE->PlayDetachedSound(m_hObject, request, position, sound_error);
            }
        }
    m_hFiredFrom = nullptr;
    client = nullptr;
    g_pServerDE->RemoveObject(m_hObject);
}
DDWORD SeeingEye::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_LINKBROKEN)
    {
        if (data == m_hFiredFrom)
            RemoveEye(true);
        else if (data == stuck)
        {
            stuck = nullptr;
            RemoveEye(false);
        }
    }
    return CProjectile::EngineMessageFn(message, data, reason);
}
DDWORD SeeingEye::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    const auto result = BaseClass::ObjectMessageFn(sender, message, read);
    if (message == damage_message && m_damage.IsDead())
        RemoveEye(true);
    return result;
}
void SeeingEye::SaveObject(HMESSAGEWRITE h)
{
    auto* s = g_pServerDE;
    if (!configured || removing_)
    {
        s->RejectPayload(h, "an unconfigured or removed eye cannot be saved");
        return;
    }
    s->WriteToLoadSaveMessageObject(h, m_hFiredFrom); // m_hOwner.
    s->WriteToLoadSaveMessageObject(h, stuck);        // m_hStuckObject.
    s->WriteToMessageByte(h, dropped);
}
void SeeingEye::LoadObject(HMESSAGEREAD h)
{
    // Original Load re-links the owner and re-attaches a stuck eye. Both are saved interlinks the
    // restore re-creates after every Load, in their saved order; the port's live attachment is
    // the link alone (no MID_ATTACH). The client stays unbound (the original's m_hClient is not
    // saved); the next ping is immediate.
    auto* s = g_pServerDE;
    HOBJECT owner = nullptr, stuck_object = nullptr;
    s->ReadFromLoadSaveMessageObject(h, &owner);
    s->ReadFromLoadSaveMessageObject(h, &stuck_object);
    bool is_dropped = false;
    if (!save_fields::read_bool(h, is_dropped))
        return;
    if (stuck_object && !is_dropped)
    {
        s->RejectPayload(h, "a stuck eye must be dropped");
        return;
    }
    m_hFiredFrom = owner;
    stuck = stuck_object;
    dropped = is_dropped;
    configured = true;
    client = nullptr;
}
