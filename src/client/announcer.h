#pragma once
#include "engine/resources.h"
#include "engine/string_resources.h"
#include "game/player_messages.h"

namespace OpenBlood2
{
    // Original TheVoice start-Bloodbath table and SMSG_THEVOICE selection. Local
    // resource lookup only: no server pointer, gameplay callback or random draw.
    class ClientAnnouncer
    {
      public:
        Conversation bloodbath(const BloodbathStart&, const Resources&);
        std::string error;
        std::uint64_t messages = 0, fallback_texts = 0;

      private:
        StringResources strings_;
        bool loaded_ = false;
    };
} // namespace OpenBlood2
