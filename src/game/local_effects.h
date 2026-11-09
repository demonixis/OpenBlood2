#pragma once
#include "client/effects.h"
#include "client/legacy_smoke_trails.h"
#include <map>
namespace OpenBlood2
{
    class GameSession;
    class LocalEffects
    {
      public:
        bool publish(const GameSession&, ClientEffects&, std::string&);
        void clear()
        {
            shapes_.clear();
            legacy_smoke_ = {};
            shape_copies = 0;
        }
        std::uint64_t shape_copies = 0;

      private:
        struct ShapeCopy
        {
            ObjectId id;
            std::weak_ptr<const ContainerShape> source;
            std::shared_ptr<const ContainerShape> copy;
        };
        std::map<std::uint32_t, ShapeCopy> shapes_;
        LegacySmokeTrails legacy_smoke_;
    };
} // namespace OpenBlood2
