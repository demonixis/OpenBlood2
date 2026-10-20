#pragma once
#include "game/original/AI_Mgr.h"
#include "game/original/AmmoPickups.h"
#include "game/original/CameraObj.h"
#include "game/original/ExitHint.h"
#include "game/original/FlagObjects.h"
#include "game/original/SmellHint.h"

namespace OpenBlood2
{
    // Exchange fixed-address sentinels only while their owning session is suspended.
    struct OriginalGlobals
    {
        std::array<DLink*, 7> heads{&AI_Mgr::m_CabalHead,    &AI_Mgr::m_MonsterHead, &CDestructable::m_DestructableHead,
                                    &SmellHint::m_SmellHead, &ExitHint::m_ExitHead,  &AmmoPickup::m_PickupHead,
                                    &CameraObj::m_CameraHead};
        std::array<DDWORD*, 7> counts{
            &AI_Mgr::m_dwNumCabal,     &AI_Mgr::m_dwNumMonster, &CDestructable::m_dwNumDestructables,
            &SmellHint::m_dwNumSmells, &ExitHint::m_dwNumExits, &AmmoPickup::m_dwNumPickups,
            &CameraObj::m_dwNumCameras};
        std::array<DLink, 7> saved_heads{};
        std::array<DDWORD, 7> saved_counts{};
        HOBJECT camera = nullptr;
        AI_Mgr::RuntimeTables tables;
        CTFWorldState ctf;
        void exchange()
        {
            for (std::size_t i = 0; i < heads.size(); ++i)
            {
                std::swap(*heads[i], saved_heads[i]);
                std::swap(*counts[i], saved_counts[i]);
            }
            std::swap(g_hActiveCamera, camera);
            AI_Mgr::ExchangeRuntimeTables(tables);
            std::swap(g_ctf, ctf);
        }
        bool empty() const
        {
            for (auto* count : counts)
                if (*count)
                    return false;
            return !g_hActiveCamera && !g_ctf.flags[0] && !g_ctf.flags[1];
        }
    };
} // namespace OpenBlood2
