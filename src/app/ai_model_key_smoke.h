#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_ai_model_key_smoke(GameSession&, ObjectId&);
    bool check_ai_model_key_smoke(GameSession&, ObjectId, std::uint64_t minimum_keys);
} // namespace OpenBlood2
