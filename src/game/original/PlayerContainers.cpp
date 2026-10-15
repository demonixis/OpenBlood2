// Selected PlayerObj::UpdateClientPhysics and UpdateAirLevel source rules.
#include "PlayerObj.h"
#include <algorithm>
#include <cmath>

DDWORD CPlayerObj::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    auto* server = GetServerDE();
    // Original PlayerObj.cpp 4266-4280: ignore volume damage during the first
    // two level seconds. The explicit level clock survives delayed player entry
    // without restarting the grace period, and resets on world replacement.
    if (message == OpenBlood2::damage_message && server->GetWorldTime() < 2.0f &&
        server->IsKindOf(server->GetObjectClass(sender), server->GetClass("VolumeBrush")))
    {
        ++protected_volume_hits;
        return 0;
    }
    return CBaseCharacter::ObjectMessageFn(sender, message, read);
}

bool CPlayerObj::UpdateContainerState()
{
    auto* server = GetServerDE();
    DVector head;
    server->GetObjectPos(m_hObject, &head);
    head.y += eye_level;
    return server->GetContainers(m_hObject, head, containers);
}
bool CPlayerObj::UpdateAirLevel(float seconds)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds > .1f || !UpdateContainerState())
        return false;
    if (m_damage.IsDead())
        return true;
    if (IsLiquid(containers.head.code) && containers.head.code != CC_FREEFALL)
    {
        m_fAirLevel -= seconds * (100.0f / 15.0f);
        if (m_fAirLevel < 0)
        {
            m_fAirLevel = 0;
            DVector position;
            auto* server = GetServerDE();
            server->GetObjectPos(m_hObject, &position);
            auto message = server->StartMessageToObject(this, m_hObject, OpenBlood2::damage_message);
            if (!message)
                return false;
            if (server->WriteDamage(message,
                                    {{}, position, 5 * seconds, OpenBlood2::DamageType::Suffocate, m_hObject}) != DE_OK)
            {
                server->EndHMessageWrite(message);
                return false;
            }
            if (server->EndMessage(message) != DE_OK)
                return false;
        }
    }
    else
        m_fAirLevel = std::min(100.0f, m_fAirLevel + seconds * (100.0f / 2.5f));
    return UpdateStats();
}
