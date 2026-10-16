#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_ai_fire_smoke(GameSession&, ObjectId&);
    bool check_ai_fire_smoke(GameSession&, ObjectId);
    bool prepare_ai_state_smoke(GameSession&, ObjectId&);
    bool check_ai_state_smoke(GameSession&, ObjectId);
    bool prepare_ai_model_key_smoke(GameSession&, ObjectId&);
    bool check_ai_model_key_smoke(GameSession&, ObjectId, std::uint64_t minimum_keys);
    bool check_ai_target_smoke(GameSession&, ObjectId);
    bool check_ai_sight_smoke(GameSession&, ObjectId);
} // namespace OpenBlood2
