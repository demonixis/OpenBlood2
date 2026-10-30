#pragma once
#include "client/network_collision.h"
#include "engine/model_store.h"
#include "engine/world.h"
#include "net/world_snapshot.h"
#include "net/world_state.h"

namespace OpenBlood2
{
    // Decode resources from this client's mounted data, never construct ObjectDLL
    // objects or borrow the publishing server's asset caches. Commits atomically.
    bool receive_network_world(const Net::WorldSnapshot&, const World&, ModelStore&, SpriteStore&, ClientWorld&,
                               std::string& error);
    class NetworkWorldReceiver
    {
      public:
        enum class StateResult
        {
            Refused,
            Ignored,
            Applied
        };
        bool begin(std::uint64_t world);
        bool receive(const Net::WorldUpdate&, const World&, ModelStore&, SpriteStore&, std::string& error);
        StateResult receive(const Net::WorldState&, std::string& error);
        // Caller supplies the delayed server clock. No prediction, extrapolation,
        // model keys or game callbacks; the predicted player's state is left alone.
        bool sample(double server_time, ObjectId predicted, ClientWorld&, std::string& error) const;
        const ClientWorld& world() const
        {
            return world_;
        }
        std::uint64_t membership_sequence() const
        {
            return membership_sequence_;
        }
        const ClientCollisionWorld& collision() const
        {
            return collision_.world();
        }

      private:
        ClientWorld world_;
        NetworkCollisionReceiver collision_;
        std::vector<std::uint32_t> generations_;
        std::uint64_t membership_sequence_ = 0;
        double membership_time_ = 0;
        struct Sample
        {
            double time;
            Net::ObjectMotion motion;
        };
        struct History
        {
            std::uint32_t generation = 0;
            std::uint64_t sequence = 0;
            std::vector<Sample> samples;
        };
        std::vector<History> histories_;
        void reset_history(const Net::ObjectState&, double time, std::uint64_t sequence);
    };
} // namespace OpenBlood2
