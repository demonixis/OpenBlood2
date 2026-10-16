#pragma once
#include "engine/resources.h"
#include "engine/world.h"

namespace OpenBlood2
{
    inline constexpr DBYTE CMSG_LOADWORLD = 108, LOADTYPE_NEW_GAME = 1, LOADTYPE_NEW_LEVEL = 2;
    struct WorldRequest
    {
        std::string filename;
        DBYTE game_type = 0, load_type = LOADTYPE_NEW_GAME, difficulty = 1;
    };
    enum class EntryState
    {
        Created,
        WorldSound,
        EditorOnly,
        Unsupported,
        Filtered,
        Failed
    };
    const char* entry_state_name(EntryState state);
    struct ResourceUse
    {
        std::string path, purpose;
        bool indexed = false;
    };
    struct WorldEntry
    {
        ObjectId object;
        std::uint64_t sound = 0; // World-owned sound survives the consumed original source instance.
        EntryState state = EntryState::Unsupported;
        std::string detail;
    };
    enum class SessionState
    {
        Empty,
        Inspection,
        Blocked
    };
    // Local source-level message transport; deliberately not the retail network ABI.
    struct WorldMessage
    {
        DBYTE id = CMSG_LOADWORLD;
        std::vector<std::uint8_t> bytes;
        bool WriteToMessageString(const char* value);
        void WriteToMessageByte(DBYTE value)
        {
            bytes.push_back(value);
        }
    };
    // Selected load-command portion of CBloodClientShell::StartNewWorld, not the
    // complete client shell. See original/session_load.cpp and the provenance ledger.
    bool original_world_request(const char* world, DBYTE game_type, DBYTE load_type, DBYTE difficulty,
                                WorldMessage& message);
    bool decode_world_request(const WorldMessage& message, WorldRequest& request, std::string& error);
} // namespace OpenBlood2
