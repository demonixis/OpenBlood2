#pragma once
#include "client/world.h"
#include "engine/model_view.h"
#include "engine/sprite_view.h"

namespace OpenBlood2
{
    // Append to an already begun presentation frame; neither needs a server/session.
    bool append_client_models(const ClientWorld&, bool player_visible, ModelView&, const Resources&, std::string&);
    bool append_client_sprites(const ClientWorld&, SpriteView&, const Resources&, std::string&);
} // namespace OpenBlood2
