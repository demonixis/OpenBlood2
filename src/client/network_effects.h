#pragma once
#include "client/effect_audio.h"
#include "client/effects.h"
#include "client/original_effects.h"
#include "net/admission.h"
#include "net/ballistic_events.h"
namespace OpenBlood2
{
    class ClientCollisionWorld;
    class NetworkEffects
    {
      public:
        bool begin(const Net::Joined&);
        void clear();
        bool receive(const Net::BallisticEvent&, std::string&);
        bool advance(double seconds, const ClientWorld&, const ClientCollisionWorld&, ModelStore&, SpriteStore&,
                     const Resources&, EffectAudio&, std::string&);
        const ClientEffects& scene() const
        {
            return scene_;
        }
        const OriginalEffects& runtime() const
        {
            return runtime_;
        }
        std::uint64_t received = 0, failed = 0;
        std::string last_error;

      private:
        Net::Joined owner_;
        std::uint64_t sequence_ = 0;
        double time_ = 0;
        OriginalEffects runtime_;
        ClientEffects scene_;
        std::vector<Net::BallisticEvent> pending_;
    };
} // namespace OpenBlood2
