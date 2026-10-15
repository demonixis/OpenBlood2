#include "objectives_smoke.h"
#include "game/original/ObjectivesTrigger.h"
namespace OpenBlood2
{
    bool objective_smoke(GameSession& g)
    {
        auto* player = g.server_shell.Player(g.local_client);
        if (!player)
            return false;
        unsigned found = 0;
        const auto before = g.client_shell.objective_messages;
        for (auto id : g.objects.snapshot())
        {
            auto* r = g.objects.get(id);
            auto* t = r ? dynamic_cast<ObjectivesTrigger*>(r->game.get()) : nullptr;
            if (!t)
                continue;
            ++found;
            // The selected authored objective may already have fired at spawn.
            // A diagnostic rearm lets the smoke verify delivery with UI hidden.
            t->SetActive(true);
            auto text = g.server.CreateString("TRIGGER");
            auto message = g.server.StartMessageToObject(player, r->handle, 1000);
            if (!message)
            {
                g.server.FreeString(text);
                return false;
            }
            const bool sent =
                g.server.WriteToMessageHString(message, text) == DE_OK && g.server.EndMessage(message) == DE_OK;
            g.server.FreeString(text);
            if (!sent || t->last_delivery != DE_OK)
                return false;
        }
        std::string error;
        return found == 1 && g.pump_player_messages(error) && g.client_shell.objective_messages == before + 1 &&
               g.client_shell.objectives_resolved && g.client_shell.objectives.resource == 1 &&
               !g.client_shell.objectives.title.empty() && !g.client_shell.objectives.text.empty();
    }
} // namespace OpenBlood2
