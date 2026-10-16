#pragma once
#include "Anim_Sound.h"
namespace OpenBlood2
{
    // Selected AI_Shared::HideNodeAndSubNodes/ResetNodes, without unrelated limb/FX code.
    bool hide_character_node(HOBJECT, int node, bool hidden, std::string& error);
    bool reset_character_nodes(HOBJECT, std::string& error);
} // namespace OpenBlood2
