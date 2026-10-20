// Selected original ObjectDLL/VolumeBrushTypes.cpp: Minefield, not base volume damage/current.
#include "Minefield.h"
#include "CampaignProjectiles.h"
#include "PlayerObj.h"
using namespace OpenBlood2;

void Minefield::UpdatePhysics(ContainerPhysics& value)
{
    auto* s = GetServerDE();
    auto* character = dynamic_cast<CBaseCharacter*>(s->HandleToObject(value.object));
    if (!character)
        return;
    MovementState support;
    // Source intentionally treats every player as grounded, including an airborne player.
    const bool on_ground =
        dynamic_cast<CPlayerObj*>(character) || (s->GetMovementState(value.object, support) && support.standing);
    if (on_ground)
    {
        ++physics_calls;
        DamageCharacter(value.object);
    }
}

void Minefield::DamageCharacter(HOBJECT target)
{
    auto* s = GetServerDE();
    auto* character = dynamic_cast<CBaseCharacter*>(s->HandleToObject(target));
    if (!character || !s->IsObjectAlive(target) || character->m_damage.IsDead())
        return;
    DVector position;
    if (s->GetObjectPos(target, &position) != DE_OK)
    {
        error = "Minefield target position unavailable";
        return;
    }
    // Original DamageObject uses the victim as sender AND responsible object.
    // It therefore does not take the player's first-two-seconds VolumeBrush grace.
    auto message = s->StartMessageToObject(character, target, damage_message);
    if (!message)
        error = "Minefield damage message unavailable";
    else if (s->WriteDamage(message, {{0, 1, 0}, position, 500, DamageType::Death, target}) != DE_OK)
    {
        s->EndHMessageWrite(message);
        error = "Minefield damage payload rejected";
    }
    else if (s->EndMessage(message) != DE_OK)
        error = "Minefield damage delivery failed";
    else
        ++damage_events;
    PublishCampaignExplosion(m_hObject, position, {0, 1, 0}, 3, "Sounds\\Weapons\\c4\\explosion_1.wav", 1000);
    ++explosions;
}

DDWORD Minefield::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 3510) // Original MID_INCONTAINER: no support check in this branch.
        DamageCharacter(sender);
    return VolumeBrush::ObjectMessageFn(sender, message, read);
}
