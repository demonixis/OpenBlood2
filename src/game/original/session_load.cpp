// Selected load-message block from ObjectDLL/BloodServerShell.cpp::LoadWorld
// and ClientShellDLL/BloodClientShell.cpp::StartNewWorld, original commit
// 006b4aabd3fb348f6846cad03799cf8df685ea95. UI, audio, save/restore and player
// shell methods are not imported or replaced with successful no-op services.
// The original field order and IDs are retained through a typed local transport.
#include "engine/formats/reader.h"
#include "game/session.h"

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
    bool parse_world_request(const WorldMessage& message, WorldRequest& request, std::string& error)
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
        if (next.game_type != 0 || (next.load_type != LOADTYPE_NEW_GAME && next.load_type != LOADTYPE_NEW_LEVEL) ||
            next.difficulty > 3)
        {
            error = "Only single-player NEW_GAME/NEW_LEVEL syntax is supported; saves/multiplayer are unimplemented";
            return false;
        }
        // Keep the original Worlds prefix, but reject traversal instead of stripping
        // arbitrary input or using unchecked _mbscpy/_mbscat into MAX_PATH.
        std::string path;
        if (!resource_path(next.filename, path))
        {
            error = "Invalid world path";
            return false;
        }
        if (!path.starts_with("worlds/"))
            path = "worlds/" + path;
        if (!path.ends_with(".dat"))
            path += ".dat";
        next.filename = std::move(path);
        request = std::move(next);
        error.clear();
        return true;
    }
    bool decode_world_request(const WorldMessage& message, WorldRequest& request, std::string& error)
    {
        request = {};
        WorldRequest next;
        if (!parse_world_request(message, next, error))
            return false;
        if (next.load_type == LOADTYPE_NEW_LEVEL)
        {
            error = "LOADTYPE_NEW_LEVEL requires original KeepAliveSave/KeepAliveLoad; safe player/world transfer is "
                    "unavailable";
            return false;
        }
        request = std::move(next);
        return true;
    }
} // namespace OpenBlood2
