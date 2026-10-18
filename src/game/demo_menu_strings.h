#pragma once
#include "engine/string_resources.h"
#include <span>
#include <string>
namespace OpenBlood2
{
    // The shipped 1998 demo client (cshell.dll "DEMO v1.0 (Build 108)") predates
    // the cres.dll IDS_MENU_* table: its CMenu*::Build code pushes literal labels.
    // Each source string id maps to the single code-referenced literal at a fixed
    // file offset of that exact build. Text is read from the user's demo, never
    // embedded; an unknown build or malformed label fails without partial output.
    bool read_demo_menu_strings(std::span<const std::uint8_t> cshell, StringResources&, std::string& error);
} // namespace OpenBlood2
