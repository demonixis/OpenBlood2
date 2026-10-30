#include "client/session.h"
#include <atomic>
#include <cstdlib>

namespace OpenBlood2
{
    ClientSession::ClientSession()
    {
        reset_local_world();
    }
    void ClientSession::reset_local_world()
    {
        // Separate candidate sessions (save/NEW_LEVEL) must never reuse a world's identity.
        movement = {};
        time = 0;
        static std::atomic<std::uint64_t> next{1};
        const auto epoch = next.fetch_add(1, std::memory_order_relaxed);
        if (!world.begin(epoch) || !collision.begin(epoch) || !camera.begin(epoch) || !hud.begin(epoch) ||
            !audio.begin(epoch))
            std::abort(); // Identity exhaustion, never silently accept an old world.
    }
} // namespace OpenBlood2
