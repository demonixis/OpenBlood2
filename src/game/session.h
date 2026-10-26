#pragma once
#include "engine/resources.h"
#include "engine/world.h"

namespace OpenBlood2
{
    inline constexpr DBYTE CMSG_LOADWORLD = 108, LOADTYPE_NEW_GAME = 1, LOADTYPE_NEW_LEVEL = 2;
    // Original SharedDefs.h restore type. Only GameSession::load_game builds it; the message
    // parser still refuses it (the menus and file slots are save/load lot 6).
    inline constexpr DBYTE LOADTYPE_RESTORESAVE = 3;
    struct WorldRequest
    {
        std::string filename;
        DBYTE game_type = 0, load_type = LOADTYPE_NEW_GAME, difficulty = 1;
    };
    enum class EntryState
    {
        Created,
        WorldSound,
        CollectedPickup, // Original acknowledged pickup consumed during initial ground placement.
        EditorOnly,
        Unsupported,
        Filtered,
        MissingWorldModel,   // Engine creation rejected; source row retained, no runtime instance.
        AbsentOriginalClass, // Named, source-audited retired class; never a registered class.
        MissingResource,     // Known invalid authored references retained without a runtime instance.
        SavedState,          // Restore mode: not CF_ALWAYSLOAD; the object comes from the save or is gone.
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
        Ready, // Explicit startup checkpoint passed; not a promise about future requests.
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
    // The world DAT a StartNewWorld/LoadWorld name designates: "Worlds\\<name>.dat", except that the
    // add-on build (`addon`, the Nightmares edition) keeps a name containing WORLDS_AO as given,
    // adding only ".dat" (original _ADDON GetWorldTitle/LoadWorld). False for traversal or bad input.
    bool world_resource_path(std::string_view name, bool addon, std::string& path);
    // Original _ADDON test: the name designates an add-on level (contains WORLDS_AO, any case).
    bool addon_world_name(std::string_view name);
    // Parses NEW_GAME/NEW_LEVEL syntax only; this never authorizes a world replacement.
    bool parse_world_request(const WorldMessage& message, WorldRequest& request, std::string& error,
                             bool addon = false);
    bool decode_world_request(const WorldMessage& message, WorldRequest& request, std::string& error,
                              bool addon = false);
} // namespace OpenBlood2
