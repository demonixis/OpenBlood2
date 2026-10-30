#pragma once
#include "client/session.h"
#include "game/container_messages.h"
#include "game/player_messages.h"
#include <map>

class CBloodServerShell;
class CVoiceMgr;
namespace OpenBlood2
{
    class ServerAdapter;
    class Objects;
    // Immediate solo envelope. No original pointer is part of the report. A wire
    // codec, delayed multiplayer reports are later lots.
    struct LocalMoveReport
    {
        ClientObjectId owner;
        std::uint64_t force_revision = 0, sequence = 0;
        PlayerMove move;
    };
    class LocalMovement
    {
      public:
        LocalMovement(ServerAdapter& server, Objects& objects, ClientSession& client)
            : server_(server), objects_(objects), client_(client)
        {
        }
        bool bind(HCLIENT);
        void clear();
        bool valid() const;
        bool synchronize();
        bool force_position(const PlayerPosition&);
        bool controls(CBloodServerShell&, const PlayerControls&, bool send, bool& crouched);
        bool environment(CBloodServerShell&, ClientContainerState&, int& character);
        bool finish(CBloodServerShell&, float seconds, ClientContainerState&);
        bool jump_voice(CVoiceMgr&, int character, bool external_camera);
        bool publish_collision();
        std::string collision_error;
        std::uint64_t primary_copies = 0, shape_copies = 0;
        bool predict(const PlayerMove&, LocalMoveReport&);
        bool submit(CBloodServerShell&, const LocalMoveReport&);
        ClientMotionState& body()
        {
            return client_.movement.body;
        }

      private:
        ServerAdapter& server_;
        Objects& objects_;
        ClientSession& client_;
        HCLIENT connection_ = nullptr;
        HOBJECT object_ = nullptr;
        std::uint64_t force_revision_ = 0, next_sequence_ = 0;
        // Only the pending envelope can be submitted. Keep its payload too: callers
        // cannot change a checked prediction into a different command at submission.
        LocalMoveReport pending_;
        bool pending = false;
        struct ShapeCopy
        {
            ObjectId id;
            std::weak_ptr<const ContainerShape> source;
            std::shared_ptr<const ContainerShape> copy;
        };
        std::map<std::uint32_t, ShapeCopy> shapes_;
        std::shared_ptr<const CollisionWorld> primary_;
        const CollisionWorld* primary_source_ = nullptr;
        std::uint64_t collision_world_ = 0;
    };
} // namespace OpenBlood2
