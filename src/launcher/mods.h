#pragma once
#include "launcher/state.h"

namespace OpenBlood2::Launcher
{
    struct Mod
    {
        Installation installation;
        std::filesystem::path archive;
        bool enabled = false;
        bool operator==(const Mod&) const = default;
    };
    bool load_mods(const std::filesystem::path&, std::vector<Mod>&, std::string&);
    bool save_mods(const std::filesystem::path&, const std::vector<Mod>&, std::string&);
    std::vector<std::filesystem::path> enabled_mods(const std::vector<Mod>&, const Installation&);
} // namespace OpenBlood2::Launcher
