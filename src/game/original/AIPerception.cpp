// Selected AI_Mgr list registration, FindObjectInRadius, ComputeStimuli and
// ChooseOpportunityTarget (Monolith 1997/1998). See ai_perception_contracts.md.
#include "AI_Mgr.h"
#include "AmmoPickups.h"
#include "ExitHint.h"
#include "SmellHint.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>

DLink AI_Mgr::m_CabalHead{}, AI_Mgr::m_MonsterHead{};
unsigned AI_Mgr::m_dwNumCabal = 0, AI_Mgr::m_dwNumMonster = 0;
void AI_Mgr::InitLists()
{
    if (!m_dwNumCabal)
        dl_TieOff(&m_CabalHead);
    if (!m_dwNumMonster)
        dl_TieOff(&m_MonsterHead);
}
AI_Mgr::AI_Mgr()
{
    InitLists();
}
AI_Mgr::~AI_Mgr()
{
    if (fire_source && GetServerDE())
        GetServerDE()->RemoveObject(fire_source);
    if (m_hCurSound && GetServerDE())
        GetServerDE()->KillSound(m_hCurSound);
    if (m_Link.m_pData)
    {
        dl_Remove(&m_Link);
        --(registered_cabal ? m_dwNumCabal : m_dwNumMonster);
    }
}
bool AI_Mgr::InsertMe()
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject))
        return false;
    if (m_Link.m_pData)
        return registered_cabal == m_bCabal;
    InitLists();
    registered_cabal = m_bCabal;
    dl_Insert(m_bCabal ? &m_CabalHead : &m_MonsterHead, &m_Link);
    m_Link.m_pData = static_cast<BaseClass*>(this);
    ++(m_bCabal ? m_dwNumCabal : m_dwNumMonster);
    return true;
}
void AI_Mgr::InsertPlayer(CPlayerObj& player)
{
    InitLists();
    if (!player.m_CabalLink.m_pData)
    {
        dl_Insert(&m_CabalHead, &player.m_CabalLink);
        player.m_CabalLink.m_pData = static_cast<BaseClass*>(&player);
        ++m_dwNumCabal;
    }
    if (!player.m_MonsterLink.m_pData)
    {
        dl_Insert(&m_MonsterHead, &player.m_MonsterLink);
        player.m_MonsterLink.m_pData = static_cast<BaseClass*>(&player);
        ++m_dwNumMonster;
    }
}
void AI_Mgr::RemovePlayer(CPlayerObj& player)
{
    if (player.m_CabalLink.m_pData)
    {
        dl_Remove(&player.m_CabalLink);
        player.m_CabalLink = {};
        --m_dwNumCabal;
    }
    if (player.m_MonsterLink.m_pData)
    {
        dl_Remove(&player.m_MonsterLink);
        player.m_MonsterLink = {};
        --m_dwNumMonster;
    }
}
HOBJECT AI_Mgr::FindObjectInRadius(OpenBlood2::AIFindList list, float range, unsigned flags, HCLASS specific)
{
    using namespace OpenBlood2;
    find = {};
    find.list = list;
    find.flags = flags;
    find.range = range;
    auto* server = GetServerDE();
    auto fail = [&](const char* reason) -> HOBJECT
    {
        find.error = reason;
        find.valid = false;
        find.selected = nullptr;
        find.distance = 0;
        return nullptr;
    };
    if (!server || !server->IsObjectAlive(m_hObject) || !std::isfinite(range) || (flags & ~15u) ||
        ((flags & FindSpecific) && !specific))
        return fail("Invalid radius request or missing specific class");
    find.valid = true;
    if (range <= 0)
        return nullptr;
    DRotation head_rotation, rotation;
    find.origin_head = server->GetModelNodeTransform(m_hObject, "head", &find.origin, &head_rotation);
    if ((!find.origin_head && server->GetObjectPos(m_hObject, &find.origin) != DE_OK) ||
        server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
        return fail("Observer transform unavailable");
    const auto forward = rotate_vector({0, 0, 1}, rotation);
    DLink* head = nullptr;
    unsigned count = 0;
    switch (list)
    {
        case AIFindList::Enemies:
            // Preserve source fallback to Cabal when the Monster list is empty.
            head = m_bCabal && m_dwNumMonster ? &m_MonsterHead : &m_CabalHead;
            count = head == &m_MonsterHead ? m_dwNumMonster : m_dwNumCabal;
            break;
        case AIFindList::Smells:
            head = &SmellHint::m_SmellHead;
            count = SmellHint::m_dwNumSmells;
            break;
        case AIFindList::Exits:
            head = &ExitHint::m_ExitHead;
            count = ExitHint::m_dwNumExits;
            break;
        case AIFindList::Ammo:
            head = &AmmoPickup::m_PickupHead;
            count = AmmoPickup::m_dwNumPickups;
            break;
        default:
            return fail("Unknown original candidate list");
    }
    if (!count)
        return nullptr;
    if (count > 65536)
        return fail("Candidate list exceeds session bound");
    auto distance = [](DVector a, DVector b)
    { return std::hypot(double(a.x) - b.x, double(a.y) - b.y, double(a.z) - b.z); };
    auto dot_direction = [&](DVector from, DVector to, double& dot)
    {
        const double length = distance(from, to);
        if (!std::isfinite(length) || length == 0)
            return false;
        dot = ((double(to.x) - from.x) * forward.x + (double(to.y) - from.y) * forward.y +
               (double(to.z) - from.z) * forward.z) /
              length;
        return std::isfinite(dot);
    };
    double saved_distance = 0;
    for (auto* link = head->m_pNext; link != head; link = link->m_pNext)
    {
        if (!link || !link->m_pData || ++find.visited > count)
            return fail("Invalid original candidate list");
        auto candidate = server->ObjectToHandle(static_cast<BaseClass*>(link->m_pData));
        if (!server->IsObjectAlive(candidate))
            continue; // Pending removals stay linked until destruction.
        DVector position;
        if (server->GetObjectPos(candidate, &position) != DE_OK)
            return fail("Candidate transform unavailable");
        const double d = distance(find.origin, position);
        if (!std::isfinite(d))
            return fail("Nonfinite candidate distance");
        if (d > range)
            continue;
        double object_dot = 0;
        if (flags & FindSpecific)
        {
            // Deliberately retain original reversed IsKindOf arguments.
            if (!server->IsKindOf(specific, server->GetObjectClass(candidate)))
                continue;
        }
        else if (!dot_direction(find.origin, position, object_dot))
            return fail("Coincident candidate has undefined original normalization");
        // The specific-class branch leaves fObjectDp at zero in the source:
        // its avoidance test cannot reject. Skip the unused uninitialized-forward
        // dot product instead of importing undefined behavior (AI_AMMO_CONTRACTS).
        if ((flags & FindAvoidTarget) && !(flags & FindSpecific) && m_hTarget)
        {
            double target_dot = 0;
            if (!server->IsObjectAlive(m_hTarget) || !target_position_valid ||
                !dot_direction(m_vTargetPos, find.origin, target_dot))
                return fail("Avoid-target requires a live target with a noncoincident sampled position");
            if (target_dot > 0 && object_dot > 0)
                continue;
        }
        if ((flags & FindFaceObject) && object_dot < 0)
            continue;
        if (flags & FindVisible)
        {
            auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(candidate));
            if (player && player->IsPowerupActive(PU_INVISIBLE))
                continue;
            const bool visible = CanSee(candidate);
            find.rays += sight.rays;
            if (sight.outcome == AISightOutcome::Invalid || sight.outcome == AISightOutcome::Unavailable)
                return fail("Required visibility query failed");
            if (!visible)
                continue;
        }
        ++find.eligible;
        // Equal distance keeps the first link; zero-distance sentinel is source behavior.
        if (saved_distance == 0 || d < saved_distance)
        {
            saved_distance = d;
            find.distance = static_cast<float>(d);
            find.selected = candidate;
        }
    }
    return find.selected;
}
int AI_Mgr::PerceptionFailure(const char* reason)
{
    ++perception_failures;
    perception_error = reason;
    stimulus = -1;
    return -1;
}
int AI_Mgr::ComputeStimuli()
{
    using namespace OpenBlood2;
    ++perception_calls;
    perception_error.clear();
    stimulus_queries = {};
    stimulus = 0;
    from_damage = false;
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !std::isfinite(m_damage.m_fHitPoints) ||
        !std::isfinite(m_damage.m_fMaxHitPoints) || m_damage.m_fMaxHitPoints <= 0 ||
        std::abs(double(m_damage.m_fHitPoints) / m_damage.m_fMaxHitPoints) > std::numeric_limits<float>::max())
        return PerceptionFailure("Observer or health ratio unavailable");
    HOBJECT next = m_damage.GetLastDamager();
    int kind = 0;
    if (next)
    {
        from_damage = true;
        // Original cleared this handle but still cast the non-character below (UB).
        if (!dynamic_cast<CBaseCharacter*>(server->HandleToObject(next)))
        {
            m_damage.ClearLastDamager();
            return PerceptionFailure("Last damager is not a character; unsafe original cast rejected");
        }
        kind = 1;
    }
    else
    {
        DVector origin;
        if (server->GetObjectPos(m_hObject, &origin) != DE_OK)
            return PerceptionFailure("Observer position unavailable");
        m_fStimuli.fill(0);
        if (!SetNewTarget(nullptr))
            return PerceptionFailure("Previous target release failed");
        const auto visible = FindObjectInRadius(AIFindList::Enemies, m_fSeeingDist, FindVisible | FindFaceObject);
        stimulus_queries[0] = find;
        if (!find.valid)
            return PerceptionFailure(find.error.c_str());
        const auto smell = FindObjectInRadius(AIFindList::Smells, m_fSmellingDist);
        stimulus_queries[1] = find;
        if (!find.valid)
            return PerceptionFailure(find.error.c_str());
        const auto sense = FindObjectInRadius(AIFindList::Enemies, m_fSensingDist);
        stimulus_queries[2] = find;
        if (!find.valid)
            return PerceptionFailure(find.error.c_str());
        // Source query order is sight/smell/sense; priority assignment is smell/sense/sight.
        auto select = [&](HOBJECT h, float range, unsigned index, int type)
        {
            if (!h || range <= 0)
                return true;
            DVector pos;
            if (server->GetObjectPos(h, &pos) != DE_OK)
                return false;
            const double distance =
                std::hypot(double(pos.x) - origin.x, double(pos.y) - origin.y, double(pos.z) - origin.z);
            if (distance <= range)
            {
                m_fStimuli[index] = float(1 - distance / range);
                next = m_fStimuli[index] > 0 ? h : nullptr;
                kind = type;
            }
            return true;
        };
        if (!select(smell, m_fSmellingDist, 2, 3) || !select(sense, m_fSensingDist, 3, 4) ||
            !select(visible, m_fSeeingDist, 0, 1))
            return PerceptionFailure("Stimulus position unavailable");
    }
    m_fStimuli[5] = m_damage.m_fHitPoints / m_damage.m_fMaxHitPoints;
    if (next)
    {
        if (kind != 3)
        {
            const auto* character = dynamic_cast<CBaseCharacter*>(server->HandleToObject(next));
            if (!character)
                return PerceptionFailure("Selected target is not a character");
            if (character->m_damage.IsDead())
                return 0; // No next-nearest retry; damage memory retained.
            if (!SetNewTarget(next))
                return PerceptionFailure("Target link acquisition failed");
        }
        m_damage.ClearLastDamager();
        stimulus = kind;
    }
    return stimulus;
}
bool AI_Mgr::ChooseOpportunityTarget()
{
    using namespace OpenBlood2;
    from_damage = false;
    stimulus_queries = {};
    ++perception_calls;
    perception_error.clear();
    stimulus = 0;
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject))
    {
        PerceptionFailure("Observer unavailable");
        return false;
    }
    if (!m_InventoryMgr.CurrentWeapon())
        return false;
    m_fStimuli.fill(0);
    if (!SetNewTarget(nullptr))
    {
        PerceptionFailure("Previous target release failed");
        return false;
    }
    const auto next = FindObjectInRadius(AIFindList::Enemies, m_fSeeingDist, FindVisible | FindFaceObject);
    if (!find.valid)
    {
        PerceptionFailure(find.error.c_str());
        return false;
    }
    if (!next || m_fSeeingDist <= 0)
        return false;
    DVector origin, position;
    if (server->GetObjectPos(m_hObject, &origin) != DE_OK || server->GetObjectPos(next, &position) != DE_OK)
    {
        PerceptionFailure("Opportunity position unavailable");
        return false;
    }
    const double distance =
        std::hypot(double(position.x) - origin.x, double(position.y) - origin.y, double(position.z) - origin.z);
    if (distance > m_fSeeingDist)
        return false;
    m_fStimuli[0] = float(1 - distance / m_fSeeingDist);
    if (m_fStimuli[0] <= 0)
    {
        m_fStimuli[0] = 0;
        return false;
    }
    const auto* character = dynamic_cast<CBaseCharacter*>(server->HandleToObject(next));
    if (!character)
    {
        PerceptionFailure("Opportunity target is not a character");
        return false;
    }
    if (character->m_damage.IsDead())
        return false;
    if (!SetNewTarget(next))
    {
        PerceptionFailure("Opportunity link acquisition failed");
        return false;
    }
    stimulus = 1;
    return true;
}
