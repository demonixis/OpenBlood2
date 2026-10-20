#pragma once
#include <cstdint>
#include <set>
#include <span>
#include <string_view>

// The NPC classes of save/load lots 11 and 12 and the layout facts other save tests need for its listed
// re-save rewrite: AI_Mgr::Load adds GetTime() to the raw m_fLastUpdate (AI_Mgr.cpp:6912).
namespace save_ai
{
    inline bool is_ai(std::string_view type)
    {
        static const std::set<std::string_view> types{
            "CultistAI", "CivilianAI", "Fanatic", "ShikariAI", "SoulDrudge", "MadScientistAI", "Behemoth", "Prophet",
            "DrudgeLord", "GabriellaAI", "GabriellaREV", "OpheliaAI", "OpheliaREV", "IshmaelAI", "IshmaelREV",
            "CalebAI", "DeathShroud", "SaveAIProbe",
            // Lot 12.
            "Gideon", "EnhancedGideon", "UndeadGideon", "AncientOne", "AncientOneTentacle", "Naga", "ZealotAI",
            "BoneLeech", "Thief", "TheHandAI", "DrudgePriest"};
        return types.contains(type);
    }
    // The class block written before AI_Mgr's (original byte order).
    inline std::size_t subclass_bytes(std::string_view type)
    {
        if (type == "CultistAI" || type == "CivilianAI")
            return 1;
        if (type == "Fanatic" || type == "UndeadGideon" || type == "DrudgePriest")
            return 4;
        if (type == "ZealotAI")
            return 5;
        if (type == "AncientOne")
            return 8;
        if (type == "AncientOneTentacle")
            return 13;
        if (type == "Gideon" || type == "EnhancedGideon" || type == "Naga")
            return 16;
        if (type == "BoneLeech" || type == "Thief" || type == "TheHandAI")
            return 29; // The original 9 bytes, then the tracked player, world flags and dimensions.
        return 0;
    }
    // Offset of m_fLastUpdate in an NPC record's payload, or npos when the block is truncated.
    inline std::size_t last_update_offset(std::string_view type, std::span<const std::uint8_t> payload)
    {
        constexpr std::size_t npos = static_cast<std::size_t>(-1);
        // Target, positions, states, stimuli and dodge flags: 92 bytes before the trigger strings.
        std::size_t at = subclass_bytes(type) + 92;
        for (int i = 0; i < 3; ++i)
        {
            if (payload.size() < at + 4)
                return npos;
            std::uint32_t length = 0;
            for (unsigned b = 0; b < 4; ++b)
                length |= std::uint32_t(payload[at + b]) << (8 * b);
            at += 4 + (length == 0xffffffffu ? 0 : length);
        }
        // Eight distances and speeds, two clocks, corpse type, reload time, two bytes, two
        // commands, flags, block flags and the shutdown byte.
        at += 67;
        return payload.size() >= at + 4 ? at : npos;
    }
    // The listed payload rewrites (lots 11 and 12): m_fLastUpdate gains GetTime(), and Naga's whomp
    // count, written as a DWord and read as a Float, reloads as 0 for a small count.
    inline void normalize_payload(std::string_view type, std::span<std::uint8_t> payload)
    {
        if (!is_ai(type))
            return;
        if (const auto at = last_update_offset(type, payload); at != static_cast<std::size_t>(-1))
            for (unsigned i = 0; i < 4; ++i)
                payload[at + i] = 0;
        if (type == "Naga" && payload.size() >= 4)
            for (unsigned i = 0; i < 4; ++i)
                payload[i] = 0;
    }
} // namespace save_ai
