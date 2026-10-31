#pragma once
#include "client/effects.h"
namespace OpenBlood2
{
    class OriginalEffects;
    // Passive copies of original cosmetic pools; no server callbacks or GPU data.
    void append_runtime_effects(const OriginalEffects&, const ClientWorld&, ClientEffectsFrame&);
} // namespace OpenBlood2
