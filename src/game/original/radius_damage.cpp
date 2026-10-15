#include "radius_damage.h"
#include "Destructable.h"
#include "engine/collision.h"
#include <cmath>
#include <vector>
OpenBlood2::RadiusDamageReport DamageObjectsInRadius(HOBJECT responsible, LPBASECLASS damager, DVector origin,
                                                     float radius, float damage, OpenBlood2::DamageType type)
{
    using namespace OpenBlood2;
    RadiusDamageReport report;
    auto* server = BaseClass::GetServerDE();
    if (!server || !damager || damager->m_hObject != responsible || !server->IsObjectAlive(responsible) ||
        !physics_vector(origin) || !std::isfinite(radius) || radius <= 0 || radius > 1e6f || !std::isfinite(damage) ||
        damage < 0 || damage > 1e6f)
    {
        report.cancelled = true;
        return report;
    }
    // Snapshot original list order before callbacks. A death relay can remove
    // later targets or spawn aggregates; neither invalidates this traversal.
    std::vector<HOBJECT> targets;
    targets.reserve(CDestructable::m_dwNumDestructables);
    auto& head = CDestructable::m_DestructableHead;
    for (auto* link = head.m_pNext; link && link != &head; link = link->m_pNext)
        targets.push_back(static_cast<CDestructable*>(link->m_pData)->m_hObject);
    report.candidates = static_cast<unsigned>(targets.size());
    const float squared = radius * radius, half = squared / 2;
    if (half == 0)
    {
        report.cancelled = true;
        return report;
    }
    for (auto target : targets)
    {
        if (!server->IsObjectAlive(responsible))
        {
            report.cancelled = true;
            break;
        }
        if (!server->IsObjectAlive(target))
            continue;
        DVector position{}, dimensions{};
        server->GetObjectPos(target, &position);
        server->GetObjectDims(target, &dimensions);
        const auto direction = vector_subtract(position, origin);
        const auto average = (dimensions.x + dimensions.y + dimensions.z) / 3;
        const auto distance = vector_dot(direction, direction) - average * average;
        // Source uses squared distance minus squared average half-dimension,
        // not a linear falloff or a visibility/solid-object query.
        if (distance > squared)
            continue;
        const auto amount = distance < half ? damage : damage - (distance - half) / half * damage;
        ++report.in_radius;
        auto message = server->StartMessageToObject(damager, target, damage_message);
        if (!message)
        {
            report.cancelled = true;
            break;
        }
        const auto written = server->WriteDamage(message, {direction, origin, amount, type, responsible, true});
        const auto delivered = server->EndMessage(message);
        if (written != DE_OK || delivered != DE_OK)
        {
            report.cancelled = true;
            break;
        }
        ++report.delivered; // Delivery is not proof of reduced health.
    }
    return report;
}
