#pragma once
#include "AI_Mgr.h"
// Original CalebAI adds constructor/model/shared-table setup only; AI_Mgr owns every state.
// Those selections are applied by ConfigureRuntime and the checked RuntimePrecreate/Initialize path.
class CalebAI final : public AI_Mgr
{
};
