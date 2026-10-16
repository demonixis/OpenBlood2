#include "game/original/Projectile.h"
#include "game/server_adapter.h"
#include <cmath>
namespace OpenBlood2
{
    void ServerAdapter::RecordProjectile(const ProjectileReport& report)
    {
        if (projectile_reports.size() == 64)
            projectile_reports.erase(projectile_reports.begin());
        projectile_reports.push_back(report);
    }
    bool ServerAdapter::advance_projectiles(double seconds)
    {
        if (objects_.busy() || !std::isfinite(seconds) || seconds < 0 || seconds > 60)
            return false;
        bool ok = true;
        for (const auto id : objects_.snapshot())
        {
            auto* record = objects_.get(id);
            auto* projectile = record && record->life == Life::Alive && record->activity == OBJSTATE_ACTIVE
                                   ? dynamic_cast<CProjectile*>(record->game.get())
                                   : nullptr;
            if (projectile)
            {
                const bool was_contact = projectile->m_bExplode;
                ok = projectile->AdvanceFlight(seconds) && ok;
                const auto* target = objects_.get(projectile->m_hHitObject);
                if (!was_contact && projectile->m_bExplode && record->life == Life::Alive && target &&
                    target->life == Life::Alive && (target->state.m_Flags & FLAG_TOUCH_NOTIFY))
                    objects_.engine_message(target->handle, MID_TOUCHNOTIFY, record->handle);
            }
        }
        return ok;
    }
} // namespace OpenBlood2
