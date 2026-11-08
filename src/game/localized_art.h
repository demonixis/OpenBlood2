#pragma once
#include <string_view>

namespace OpenBlood2
{
    class HudCanvas;
    class Localization;
    class LocalizedFont;
    class Resources;
    struct HudSurface;

    const HudSurface* demo_end_screen(HudCanvas&, const Resources&, const Localization&, const LocalizedFont&);
    void localize_demo_title(HudSurface&, const Resources&, const Localization&, const LocalizedFont&);
    void localize_loading_art(HudSurface&, std::string_view path, const Localization&, const LocalizedFont&,
                              const HudSurface* chapter_backdrop = nullptr);
    bool localize_loading_plate(HudSurface&, const HudSurface& plate, const Localization&, const LocalizedFont&);
}
