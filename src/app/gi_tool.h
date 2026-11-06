#pragma once
#include "engine/config.h"

namespace OpenBlood2
{
    // GI command-line tools (gi_contracts.md lot G2). Both mount the configured data read-only.
    // --list-worlds [--json]: every mounted world DAT in a fixed order with its digest and GI file
    // name. Exit 0, 2 (data), 5 (a world cannot be read).
    int run_list_worlds(const Options&, const Config&);
    // --gi-verify FILE: validates an .ob2gi file, prints its header and compares it with the mounted
    // world. Exit 0 (ready), 2 (arguments, data, unreadable file), 4 (invalid, stale or foreign),
    // 5 (its world is not mounted or cannot be read).
    int run_gi_verify(const Options&, const Config&);
    // --gi-place WORLD|single|all (lot G4): places light and reflection probes and writes the placement
    // file of each world (existing settings, volumes and manual probes are kept unless --gi-reset;
    // --gi-dry-run writes nothing). Exit 0, 2 (arguments, data, directory), 4 (a placement failed or an
    // existing file is invalid or foreign), 5 (a world cannot be read).
    int run_gi_place(const Options&, const Config&);
    // --bake-gi WORLD (lot G5): bakes one world into --gi-out (default user_directory()/gi), printing
    // GI_PROGRESS lines and one GI_RESULT line. SIGINT/SIGTERM cancel; nothing is written then. Exit 0,
    // 2 (arguments, data), 4 (placement, bake or write), 5 (world load), 12 (cancelled).
    int run_gi_bake(const Options&, const Config&);
} // namespace OpenBlood2
