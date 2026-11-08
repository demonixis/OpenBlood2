#include "game/cinematic_captions.h"
#include "client/audio.h"
#include "engine/collision.h"
#include "engine/localization.h"
#include "game/original/ConversationTrigger.h"
#include <algorithm>
#include <initializer_list>
#include <utility>

namespace OpenBlood2
{
    namespace
    {
        struct Transcript
        {
            std::string_view path, key, text;
            bool gameplay = false;
        };

        constexpr std::initializer_list<Transcript> transcripts{
#include "cinematic_transcripts.inc"
        };

        bool in_audible_range(const ClientSound& sound, const DVector& listener)
        {
            const auto& parameters = sound.parameters;
            if (parameters.volume <= 0)
                return false;
            if (parameters.space == SoundSpace::Local)
                return true;
            const auto offset = vector_subtract(sound.position, listener);
            const auto squared = vector_dot(offset, offset);
            // Match the audio mixer's hard edge when inner and outer radii are equal.
            return squared <= parameters.inner_radius * parameters.inner_radius ||
                   squared < parameters.outer_radius * parameters.outer_radius;
        }
    } // namespace

    CinematicCaption cinematic_caption(const ClientAudio& audio, const Resources& resources,
                                       const Localization* localization, const DVector* listener,
                                       std::uint64_t comm_link_voice)
    {
        CinematicCaption caption;
        std::pair<bool, SoundId> selected{};
        for (const auto& sound : audio.frame().sounds)
        {
            // Cinematic dialogue has priority over nearby optional lines, then newest cue wins.
            const auto priority = std::pair{sound.cinematic, sound.id};
            if (sound.id == comm_link_voice || sound.done || sound.speaker_lost || !sound.error.empty() ||
                priority <= selected || (!sound.cinematic && (!listener || !in_audible_range(sound, *listener))))
                continue;
            std::string path;
            if (!resource_path(sound.parameters.resource, path) || localization_resource_overridden(resources, path))
                continue;
            const auto transcript = std::ranges::find(transcripts, path, &Transcript::path);
            if (transcript != transcripts.end())
            {
                if (!sound.cinematic && !transcript->gameplay)
                    continue;
                caption = {transcript->text, localization ? localization->text(transcript->key) : std::string_view{}};
            }
            else
            {
                if (!sound.cinematic)
                    continue;
                const auto separator = path.find_last_of('/');
                const auto file = std::string_view(path).substr(separator == path.npos ? 0 : separator + 1);
                const auto* text = conversation_text(file);
                if (!text)
                    continue;
                caption = {text, localization ? localization->dialogue(file) : std::string_view{}};
            }
            selected = priority;
        }
        return caption;
    }
} // namespace OpenBlood2
