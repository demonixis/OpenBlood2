#pragma once
#include "engine/config.h"
namespace OpenBlood2
{
    class Runtime;
    int run_network_session(const Options&, const Config&, Runtime&, std::string& error, bool& quit);
    int run_network_frontend(const Options&, const Config&);
} // namespace OpenBlood2
