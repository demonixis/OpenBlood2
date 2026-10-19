#pragma once
#include "engine/config.h"

namespace OpenBlood2::Launcher
{
    enum class Edition
    {
        Retail,
        Demo
    };
    struct Installation
    {
        Edition edition = Edition::Retail;
        std::filesystem::path directory;
        bool operator==(const Installation&) const = default;
    };
    struct State
    {
        std::filesystem::path game;
        int selected = -1;
        std::vector<Installation> installations;
        bool operator==(const State&) const = default;
    };
    bool valid_text(std::string_view text, std::size_t limit);
    bool load_state(const std::filesystem::path& file, State& output, std::string& error);
    bool save_state(const std::filesystem::path& file, const State& state, std::string& error);
    bool atomic_write(const std::filesystem::path& file, std::string_view text, std::string& error);
} // namespace OpenBlood2::Launcher
