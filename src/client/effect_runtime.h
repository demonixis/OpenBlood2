#pragma once
#include <cstdint>
namespace OpenBlood2
{
    // ClientSession owns the optional game's cosmetic runtime. The replica/core
    // remains usable without linking original game effects or a server shell.
    class EffectRuntime
    {
      public:
        virtual ~EffectRuntime() = default;
        virtual void begin_world(std::uint64_t) = 0;
        virtual void reset_world() = 0;
    };
} // namespace OpenBlood2
