#pragma once
#include "client/collision.h"
#include <map>

namespace OpenBlood2
{
    class NetworkCollisionReceiver
    {
      public:
        bool begin(std::uint64_t world, const World&, std::string& error);
        bool receive(const ClientWorld&, std::string& error);
        bool active() const
        {
            return bool(primary_);
        }
        const ClientCollisionWorld& world() const
        {
            return world_;
        }

      private:
        ClientCollisionWorld world_;
        std::shared_ptr<const World> geometry_;
        std::shared_ptr<const CollisionWorld> primary_;
        struct Shape
        {
            ObjectId id;
            std::uint32_t index = UINT32_MAX;
            DRotation basis, rotation;
            DVector origin{};
            bool world_model = false;
            std::shared_ptr<const ContainerShape> base, posed;
        };
        std::map<std::uint32_t, Shape> shapes_;
        bool shape(const ClientObject&, std::shared_ptr<const ContainerShape>&, std::string& error);
    };
} // namespace OpenBlood2
