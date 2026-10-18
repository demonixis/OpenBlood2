#pragma once
#include "engine/config.h"

namespace OpenBlood2
{
    // Headless presentation measurement: enters the selected world exactly like
    // --view-world, renders the application scene into an offscreen target and
    // reports per-stage CPU time, GPU completion time and submission counters.
    // Each frame waits for its GPU fence, so stages are measured serialized.
    int run_bench(const Options&, const Config&);
} // namespace OpenBlood2
