#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_corpse_smoke(GameSession&, ObjectId&);
    bool check_corpse_smoke(GameSession&, ObjectId, bool log = false);
    bool check_ai_runtime_smoke(GameSession&, ObjectId&);
    bool prepare_ai_escape_smoke(GameSession&, ObjectId&);
    bool check_ai_escape_smoke(GameSession&, ObjectId);
    bool prepare_ai_follow_smoke(GameSession&, ObjectId&);
    bool check_ai_follow_smoke(GameSession&, ObjectId);
    bool prepare_ai_fire_smoke(GameSession&, ObjectId&);
    bool check_ai_fire_smoke(GameSession&, ObjectId);
    bool prepare_ai_state_smoke(GameSession&, ObjectId&);
    bool check_ai_state_smoke(GameSession&, ObjectId);
    bool prepare_ai_model_key_smoke(GameSession&, ObjectId&);
    bool check_ai_model_key_smoke(GameSession&, ObjectId, std::uint64_t minimum_keys);
    bool check_ai_target_smoke(GameSession&, ObjectId);
    bool check_ai_sight_smoke(GameSession&, ObjectId);
} // namespace OpenBlood2
