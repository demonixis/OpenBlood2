#pragma once
#include "engine/config.h"
struct ImFont;
namespace OpenBlood2
{
    struct Runtime;
    class LocalizedFont;
    int run_network_session(const Options&, const Config&, Runtime&, std::string& error, bool& quit,
                            const LocalizedFont* = nullptr, ImFont* localized_face = nullptr);
    int run_network_frontend(const Options&, const Config&);
} // namespace OpenBlood2
