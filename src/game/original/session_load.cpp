// Selected load-message block from ObjectDLL/BloodServerShell.cpp::LoadWorld
// and ClientShellDLL/BloodClientShell.cpp::StartNewWorld, original commit
// 006b4aabd3fb348f6846cad03799cf8df685ea95. UI, audio, save/restore and player
// shell methods are not imported or replaced with successful no-op services.
// The original field order and IDs are retained through a typed local transport.
#include "engine/formats/reader.h"
#include "game/game_settings.h"
#include "game/session.h"
#include <algorithm>
#include <cctype>

namespace OpenBlood2
{
    bool WorldMessage::WriteToMessageString(const char* value)
    {
        if (!value || std::strlen(value) > 255)
            return false;
        const auto size = std::strlen(value);
        bytes.push_back(static_cast<DBYTE>(size));
        bytes.push_back(static_cast<DBYTE>(size >> 8));
        bytes.insert(bytes.end(), value, value + size);
        return true;
    }
    bool original_world_request(const char* pszWorld, DBYTE nGameType, DBYTE nLoadType, DBYTE nDifficulty,
                                WorldMessage& message)
    {
        message = {};
        if (!pszWorld || !pszWorld[0])
            return false;
        // Original StartNewWorld's CMSG_LOADWORLD block. Transport owns the message
        // by value; a failed bounded write is propagated instead of dispatching it.
        message.id = CMSG_LOADWORLD;
        if (!message.WriteToMessageString(pszWorld))
            return false;                        // Filename
        message.WriteToMessageByte(nGameType);   // Game type
        message.WriteToMessageByte(nLoadType);   // Load type
        message.WriteToMessageByte(nDifficulty); // Difficulty level
        return true;
    }
    bool addon_world_name(std::string_view name)
    {
        constexpr std::string_view marker = "WORLDS_AO";
        for (std::size_t i = 0; i + marker.size() <= name.size(); ++i)
            if (std::equal(marker.begin(), marker.end(), name.begin() + i,
                           [](char a, char b) { return a == std::toupper(static_cast<unsigned char>(b)); }))
                return true;
        return false;
    }
    bool world_resource_path(std::string_view name, bool addon, std::string& path)
    {
        path.clear();
        // Keep the original Worlds prefix, but reject traversal instead of stripping
        // arbitrary input or using unchecked _mbscpy/_mbscat into MAX_PATH.
        if (!resource_path(name, path))
            return false;
        if (!path.starts_with("worlds/") && !(addon && addon_world_name(path)))
            path = "worlds/" + path;
        if (!path.ends_with(".dat"))
            path += ".dat";
        return true;
    }
    bool parse_world_request(const WorldMessage& message, WorldRequest& request, std::string& error, bool addon)
    {
        request = {};
        Reader reader(message.bytes);
        WorldRequest next;
        // Original LoadWorld read order; the rest of the shell is still a blocker.
        next.filename = reader.string(255);
        next.game_type = reader.u8();
        next.load_type = reader.u8();
        next.difficulty = reader.u8();
        if (message.id != CMSG_LOADWORLD || !reader.good() || reader.remaining() || next.filename.empty())
        {
            error = "Malformed CMSG_LOADWORLD";
            return false;
        }
        // The local game types: story single player, Single Player > CHOSEN's GAMETYPE_ACTION and
        // the custom-level GAMETYPE_CUSTOM (MenuCredits). NEW_LEVEL carries the client's current
        // type; the server keeps the NEW_GAME settings either way (LoadWorld's SetGameInfo).
        if (!local_game_type(next.game_type) ||
            (next.load_type != LOADTYPE_NEW_GAME && next.load_type != LOADTYPE_NEW_LEVEL) || next.difficulty > 3)
        {
            error = "Only local-game NEW_GAME/NEW_LEVEL syntax is supported; saves use their own path and "
                    "multiplayer is unimplemented";
            return false;
        }
        std::string path;
        if (!world_resource_path(next.filename, addon, path))
        {
            error = "Invalid world path";
            return false;
        }
        next.filename = std::move(path);
        request = std::move(next);
        error.clear();
        return true;
    }
    bool decode_world_request(const WorldMessage& message, WorldRequest& request, std::string& error, bool addon)
    {
        request = {};
        WorldRequest next;
        if (!parse_world_request(message, next, error, addon))
            return false;
        if (next.load_type == LOADTYPE_NEW_LEVEL)
        {
            error = "LOADTYPE_NEW_LEVEL requires the owned KeepAliveSave/KeepAliveLoad transaction; "
                    "queue it for the frame boundary";
            return false;
        }
        request = std::move(next);
        return true;
    }
} // namespace OpenBlood2
