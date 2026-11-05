#pragma once
#include "engine/resources.h"
#include "engine/string_resources.h"
#include "game/player_messages.h"
#include <span>

namespace OpenBlood2
{
    struct AnnouncerPlayer
    {
        std::string_view name;
        DBYTE character = 0;
    };
    // Original TheVoice startup/combat tables and addressed selection. Local
    // resource lookup only: no server pointer, gameplay callback or random draw.
    class ClientAnnouncer
    {
      public:
        Conversation bloodbath(const BloodbathStart&, const Resources&);
        bool frag(std::uint16_t index, const AnnouncerPlayer& killer, const AnnouncerPlayer& victim, bool suicide,
                  const Resources&, Conversation&);
        bool finish(const FinishHimVoice&, const Resources&, Conversation&);
        std::string error;
        std::uint64_t messages = 0, fallback_texts = 0;

      private:
        StringResources strings_;
        bool loaded_ = false;
        void load(const Resources&);
        bool message(std::uint32_t resource, std::string_view fallback, std::string_view file,
                     std::span<const std::string_view> names, const Resources&, Conversation&);
    };
} // namespace OpenBlood2
