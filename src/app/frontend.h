#pragma once
#include "engine/config.h"
#include <optional>
namespace OpenBlood2
{
    // The interactive game. Without a configuration (no --data-dir, BLOOD2_DATA_DIR or
    // blood2.local.ini data_dir) the stored setup choice is used, else the setup screen opens first.
    int run_frontend(const Options&, const std::optional<Config>&);
} // namespace OpenBlood2
