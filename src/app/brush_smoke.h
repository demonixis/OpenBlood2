#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_brush_smoke(GameSession&, ObjectId&, ObjectId& explosion, ObjectId& body);
    bool command_brush_smoke(GameSession&, ObjectId, const char*);
    bool check_brush_smoke(GameSession&, ObjectId, ObjectId);
} // namespace OpenBlood2
