#pragma once
#include "client/session.h"
#include "game/player_messages.h"
namespace OpenBlood2
{
    class ServerAdapter;
    class Objects;
    class LocalMovement;
    struct LocalCameraCommand
    {
        ClientObjectId owner;
        std::uint64_t sequence = 0;
        PlayerRotation aim;
    };
    // Immediate solo publication/command boundary, never used by passive consumers.
    class LocalPresentation
    {
      public:
        LocalPresentation(ServerAdapter& server, Objects& objects, ClientSession& client, LocalMovement& movement)
            : server_(server), objects_(objects), client_(client), movement_(movement)
        {
        }
        void sync_clock();
        bool bind(HCLIENT);
        bool publish(HCLIENT, HOBJECT eye, HOBJECT orb);
        bool commit(HCLIENT, const LocalCameraCommand&);
        bool deliver(HCLIENT, const PlayerStats&);
        bool deliver(HCLIENT, const InventoryItems&);
        std::string error;
        std::uint64_t accepted_commands = 0, rejected_commands = 0;

      private:
        ClientObjectId owner(HCLIENT) const;
        ServerAdapter& server_;
        Objects& objects_;
        ClientSession& client_;
        LocalMovement& movement_;
        HOBJECT eye_ = nullptr, orb_ = nullptr;
        std::uint64_t committed_world_ = 0, committed_sequence_ = 0;
    };
} // namespace OpenBlood2
