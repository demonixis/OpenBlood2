#pragma once
#include "engine/config.h"
namespace OpenBlood2
{
    struct UiRuntime;
    struct MenuSettings;
    class LocalizedFont;
    int run_network_session(const Options&, const Config&, UiRuntime&, std::string& error, bool& quit,
                            const LocalizedFont&, MenuSettings* shared_settings = nullptr);
    int run_network_frontend(const Options&, const Config&);
} // namespace OpenBlood2
