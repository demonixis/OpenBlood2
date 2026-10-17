#include <SDL3/SDL.h>
#include <iostream>

int main()
{
    // The Wayland HDR callback can arrive during window destruction, after its
    // property group has been released. Setters must reject that stale ID.
    const auto stale = SDL_CreateProperties();
    if (!stale || !SDL_SetFloatProperty(stale, "headroom", 1.0f))
        return 1;
    SDL_DestroyProperties(stale);
    const auto live = SDL_CreateProperties();
    if (!live || live == stale || !SDL_SetFloatProperty(live, "headroom", 2.0f))
        return 1;
    const bool rejected = !SDL_SetFloatProperty(stale, "headroom", 1.14f) &&
                          !SDL_SetBooleanProperty(stale, "enabled", true) &&
                          !SDL_SetStringProperty(stale, "label", "gone");
    const bool intact = SDL_GetFloatProperty(live, "headroom", 0) == 2.0f;
    SDL_DestroyProperties(live);
    SDL_Quit();
    if (!rejected || !intact)
        return 1;
    std::cout << "Destroyed SDL property IDs reject assignments; live properties remain intact\n";
    return 0;
}
