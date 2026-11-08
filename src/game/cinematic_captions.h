#pragma once
#include <cstdint>
#include <string_view>

struct DVector;

namespace OpenBlood2
{
    class ClientAudio;
    class Localization;
    class Resources;

    struct CinematicCaption
    {
        std::string_view english, translated;
    };

    // Read the current cinematic or an audible, verified gameplay line from the local sound frame.
    CinematicCaption cinematic_caption(const ClientAudio&, const Resources&, const Localization* = nullptr,
                                       const DVector* listener = nullptr, std::uint64_t comm_link_voice = 0);

    // Elapsed time in the retail Intro.smk timeline, in seconds.
    CinematicCaption intro_caption(double seconds, const Localization* = nullptr);
} // namespace OpenBlood2
