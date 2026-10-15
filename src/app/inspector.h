#pragma once

#include "engine/config.h"
#include "engine/runtime.h"
#include "game/server_adapter.h"

namespace blood2 {

struct Inspector {
    Runtime& runtime;
    bool context = false;
    bool platform = false;
    bool renderer = false;
    ObjectId remove_request{};
    std::string spawn_request;

    explicit Inspector(Runtime& value) : runtime(value) {}
    ~Inspector();
    Inspector(const Inspector&) = delete;
    Inspector& operator=(const Inspector&) = delete;
    bool initialize(std::string& error);
    void apply_commands(GameSession& game, ObjectSelection& selection);
    void draw(const Config& config, const DataStatus& data, bool& visible, std::uint64_t frames,
              GameSession& game, ObjectSelection& selection);
};

}
