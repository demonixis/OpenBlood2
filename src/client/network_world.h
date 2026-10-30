#pragma once
#include "engine/model_store.h"
#include "engine/world.h"
#include "net/world_snapshot.h"

namespace OpenBlood2
{
    // Decode resources from this client's mounted data, never construct ObjectDLL
    // objects or borrow the publishing server's asset caches. Commits atomically.
    bool receive_network_world(const Net::WorldSnapshot&, const World&, ModelStore&, SpriteStore&, ClientWorld&,
                               std::string& error);
    class NetworkWorldReceiver
    {
      public:
        bool begin(std::uint64_t world);
        bool receive(const Net::WorldUpdate&, const World&, ModelStore&, SpriteStore&, std::string& error);
        const ClientWorld& world() const
        {
            return world_;
        }
        std::uint64_t membership_sequence() const
        {
            return membership_sequence_;
        }

      private:
        ClientWorld world_;
        std::vector<std::uint32_t> generations_;
        std::uint64_t membership_sequence_ = 0;
    };
} // namespace OpenBlood2
