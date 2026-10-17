// Original AI_Mgr.cpp:784-798,6081-6172. See ai_ammo_contracts.md.
#include "AI_Mgr.h"
#include "AmmoPickups.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;

void AI_Mgr::ReceiveAmmo(HOBJECT sender, HMESSAGEREAD read)
{
    auto* s = GetServerDE();
    PickupMessage pickup;
    // Source MID_ADDAMMO: death and NOAMMOCOLLECT guard the inventory mutation.
    // The typed boundary additionally authenticates the live ammo publisher.
    const auto* source = s && s->IsObjectAlive(sender) ? dynamic_cast<AmmoPickup*>(s->HandleToObject(sender)) : nullptr;
    if (!s || !s->IsObjectAlive(m_hObject) || m_damage.IsDead() || (m_dwFlags & 0x200u) || !source ||
        !source->IsOfferingTo(m_hObject) || !s->ReadPickupMessage(read, pickup) || pickup.kind != PickupKind::Ammo ||
        pickup != source->value || m_InventoryMgr.AddAmmo(pickup.type, pickup.amount) != InventoryResult::Ok)
    {
        ++ammo_refusals;
        return;
    }
    ++ammo_collections;
    last_ammo_source = sender;
    auto reply = s->StartMessageToObject(this, sender, picked_up_message);
    if (reply)
    {
        s->WritePickupMessage(reply, pickup);
        s->EndMessage(reply); // May remove source or actor; no borrowed source access afterwards.
    }
}
bool AI_Mgr::AI_STATE_FindAmmo()
{
    return NavigationState(StateFindAmmo);
}
bool AI_Mgr::AmmoStep()
{
    auto* s = GetServerDE();
    navigation_threshold = (std::hypot(m_vDims.x, m_vDims.z) + .1f) / 2;
    switch (Metacmd)
    {
        case 1:
        {
            ++ammo_searches;
            searched_ammo = 0;
            searched_ammo_class.clear();
            const auto* weapon = m_InventoryMgr.CurrentWeapon();
            if (!weapon)
                return SetNewState(StateIdle) || NavigationFailure(state_error);
            searched_ammo = weapon->data.m_nAmmoType;
            // Preserve the original RocketAmmoPU spelling. The actual class is
            // HowitzerAmmoPU; an absent class yields no candidate, never an alias.
            static constexpr const char* classes[]{"",
                                                   "BulletAmmoPU",
                                                   "ShellAmmoPU",
                                                   "BMGAmmoPU",
                                                   "FlareAmmoPU",
                                                   "DieBugDieAmmoPU",
                                                   "RocketAmmoPU",
                                                   "FuelAmmoPU",
                                                   "BatteryAmmoPU"};
            if (searched_ammo < std::size(classes))
                searched_ammo_class = classes[searched_ammo];
            const auto type = searched_ammo_class.empty() ? nullptr : s->GetClass(searched_ammo_class.c_str());
            searched_ammo_target = nullptr;
            m_hTrackObject = nullptr;
            tracked_position_valid = false;
            if (type)
            {
                m_hTrackObject = FindObjectInRadius(AIFindList::Ammo, m_fSeeingDist,
                                                    FindAvoidTarget | FindVisible | FindSpecific, type);
                if (!find.valid)
                    return NavigationFailure(find.error);
            }
            searched_ammo_target = m_hTrackObject;
            if (!m_hTrackObject)
                return SetNewState(StateEscapeRun) || NavigationFailure(state_error);
            if (!RefreshTrackedPosition() || !MC_FacePos(m_vTrackObjPos))
                return NavigationFailure(target_error);
            // Source ignores a failed CalculatePath. Keep its empty-route escape,
            // but never follow partial points after a rejected bounded plan.
            navigation_plan_failed = !m_MoveObj.CalculatePath(m_vTrackObjPos);
            if (navigation_plan_failed)
            {
                if (m_MoveObj.outcome == CMovement::Outcome::Blocked)
                {
                    m_MoveObj.Term();
                    return true;
                }
                return NavigationFailure(m_MoveObj.error);
            }
            return true;
        }
        case 2:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return SetNewState(StateEscapeRun) || NavigationFailure(state_error);
            return MC_FacePos(point) || NavigationFailure(target_error);
        }
        case 3:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return NavigationDecision();
            point.y = m_MoveObj.GetPos().y;
            const auto delta = vector_subtract(point, m_MoveObj.GetPos());
            navigation_distance = std::hypot(delta.x, delta.y, delta.z);
            bool edge = false;
            if (!IsLedge(m_MoveObj.GetForwardVector(), edge))
                return NavigationFailure(locomotion_error);
            if (edge)
                ++Metacmd;
            else if (navigation_distance <= navigation_threshold)
            {
                const bool another = m_MoveObj.MoveToNextPathPoint();
                ++navigation_arrivals;
                if (another && !IsLedge(m_MoveObj.GetForwardVector(), edge))
                    return NavigationFailure(locomotion_error);
                if (!another || edge)
                    return MC_FacePos(m_vTargetPos) || NavigationFailure(target_error);
                Metacmd = 2;
            }
            else
                return MC_Run() || NavigationFailure(locomotion_error);
            return true;
        }
        case 4:
            return NavigationDecision();
        default:
            return true;
    }
}
