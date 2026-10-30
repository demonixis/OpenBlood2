// Selected original ClientShellDLL/TheVoice.cpp startup table and selection.
// Copyright (c) 1998 Monolith Productions Inc.
#include "client/announcer.h"
#include <array>

namespace OpenBlood2
{
    Conversation ClientAnnouncer::bloodbath(const BloodbathStart& event, const Resources& resources)
    {
        // TheVoice.cpp:134-141. Every entry has VOICEFLAG_ALL, so the source's
        // male/male startup flags select the first index without a search.
        static constexpr std::array<const char*, 8> fallback{
            {"Let The Bloodbath Begin!", "May The Bloodbath Continue!", "Determine The Master Of The Bloodbath",
             "The Carnage Continues!", "Let The Bloodbath Start!", "May The Killing Continue", "Begin The Bloodbath!",
             "Bloodbath, My Children!"}};
        if (!loaded_)
        {
            loaded_ = true;
            std::vector<std::uint8_t> bytes;
            if (const auto* entry = resources.find("cres.dll"); entry && entry->size > 32 * 1024 * 1024)
                error = "cres.dll exceeds the 32 MiB string reader limit";
            else if (resources.read("cres.dll", bytes, error))
                read_string_resources(bytes, strings_, error);
        }
        ++messages;
        const auto index = event.index % fallback.size();
        Conversation result;
        result.character = 4;
        result.file = "Sounds\\Voice\\Voice" + std::to_string(248 + index) + ".wav";
        const auto found = strings_.strings.find(4001 + static_cast<std::uint32_t>(index));
        if (found != strings_.strings.end() && !found->second.empty() && found->second.size() < 500)
            result.text = found->second;
        else
        {
            result.text = fallback[index]; // Original Ive fallback, not invented dialogue.
            ++fallback_texts;
            if (error.empty())
                error = "Missing or invalid announcer string " + std::to_string(4001 + index);
        }
        return result;
    }
} // namespace OpenBlood2
