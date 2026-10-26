// Original CultistAI.cpp:269-332; see ai_state_contracts.md.
#include "CultistAI.h"
#include "SaveFields.h"
#include <cmath>

bool CultistAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    DVector position;
    auto* server = GetServerDE();
    if (stim < 0 || stim > 4 || !server || !server->IsObjectAlive(m_hObject) ||
        server->GetObjectPos(m_hObject, &position) != DE_OK || !std::isfinite(m_fStimuli[0]) ||
        !std::isfinite(m_fStimuli[5]) || !std::isfinite(m_vDims.y) || m_vDims.y <= 0)
        return StateFailure("Cultist state stimulus or body unavailable");
    const double height = std::abs(double(m_vTargetPos.y) - position.y);
    if (!std::isfinite(height))
        return StateFailure("Cultist target height invalid");
    if (!stim)
    {
        switch (m_nState)
        {
            case StateIdle:
            case StateSearchVisual:
            case StateSearchSmell:
                return SetNewState(StateIdle);
            case StateFindAmmo:
            case StateFindHealth:
                return SetNewState(m_nLastState);
            case StateEscapeRun:
                if (!SetNewState(StateIdle))
                    return false;
                [[fallthrough]]; // Source continues with the changed last state.
            default:
                if (m_nLastState == StateGuard)
                    return SetNewState(StateGuard);
                if (height <= m_vDims.y && m_fStimuli[5] >= .5f)
                    return SetNewState(StateSearchVisual);
                return SetNewState(StateIdle);
        }
    }
    if (m_fStimuli[5] < .25f)
        return SetNewState(StateEscapeRun);
    if (m_fStimuli[5] < .5f || m_fStimuli[0] <= .5f || height > m_vDims.y)
        return SetNewState(StateAttackFar);
    return SetNewState(StateAttackClose);
}

// Original CultistAI::Save/Load. The add-on Load derives the clown and robe skins from the model
// filenames, then builds the gender table if this is its first instance and binds it. The port's
// restored initial update leaves this to Load (save_load_contracts lot 11).
void CultistAI::SaveSubclass(HMESSAGEWRITE h)
{
    g_pServerDE->WriteToMessageByte(h, npc_male);
}
void CultistAI::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    bool male = true;
    if (!OpenBlood2::save_fields::read_bool(h, male))
        return;
    std::string model, skin;
    if (!s->GetModelFilenames(m_hObject, model, skin))
    {
        s->RejectPayload(h, "CultistAI Load: model filenames unavailable");
        return;
    }
    for (auto& c : skin)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
    const bool clown = skin.find("M_CULTIST4") != skin.npos || skin.find("M_CULTIST5") != skin.npos ||
                       skin.find("M_CULTIST6") != skin.npos;
    // _ADDON: the robe flag comes from the oldschool skin (the retail Load never sets it).
    const bool robe = IsAddonBuild() && skin.find("OLDSCHOOL") != skin.npos;
    if ((clown || robe) && !male)
    {
        s->RejectPayload(h, "CultistAI Load: a clown or robe skin always carries the male flag");
        return;
    }
    npc_male = male;
    npc_clown = clown;
    npc_robe = robe;
    std::string error;
    if (!InitializeSharedTable(error))
        s->RejectPayload(h, "CultistAI Load: " + error);
    else if (robe)
    {
        // RestoreInitialize ran before the skin was known: the oldschool model's body-damage policy.
        m_damage.ai_body_hits = true;
        m_damage.ai_hit_localization = false;
    }
}
