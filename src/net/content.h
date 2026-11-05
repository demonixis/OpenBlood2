#pragma once
#include "engine/resources.h"

namespace OpenBlood2::Net
{
    // Whole mounted archives, in precedence order; never absolute installation paths.
    // Rejects non-retail/mod content. Publishes the digest only after every read succeeds.
    // As with Resources, archives must remain immutable for the life of the session.
    bool content_fingerprint(const Resources&, Sha256Digest&, std::string& error);
} // namespace OpenBlood2::Net
