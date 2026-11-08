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

        struct IntroCue
        {
            double start, end;
            std::string_view key, english;
        };

        // Align complete phrases to the original Intro.smk narration. Leave the music-only
        // opening and tail clear. Spelling follows the original ConversationStrings table.
        constexpr IntroCue intro_cues[]{
            {6.90, 9.70, "intro.line01", "We are the Cabal."},
            {9.90, 15.60, "intro.line02", "I, Gideon, am its sinister genius leader."},
            {16.10, 19.50, "intro.line03", "We have always lurked among you."},
            {19.50, 25.50, "intro.line04", "Like maggots in the bloated, stinking corpse of a diseased rat."},
            {25.50, 31.70, "intro.line05", "Servants of the One That Binds, the dark god Tchernobog."},
            {31.90, 38.50, "intro.line06", "Until a century ago, everything was going according to our evil plans."},
            {38.70, 42.50, "intro.line07", "We never expected to be betrayed by one of our own."},
            {42.50, 45.60, "intro.line08", "His name was Caleb."},
            {45.60, 51.10, "intro.line09", "He destroyed our god and absorbed the power of Tchernobog."},
            {51.20, 55.90, "intro.line10", "I, and I alone, will restore our dark god."},
            {55.90, 57.90, "intro.line11", "I will destroy Caleb."},
            {57.90, 62.60, "intro.line12", "I will mash his intestines beneath my feet like rotten grapes."},
            {62.70, 67.20, "intro.line13", "And Caleb will die for the last time."},
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

    CinematicCaption intro_caption(double seconds, const Localization* localization)
    {
        const auto cue = std::ranges::find_if(intro_cues, [&](const IntroCue& line)
        { return line.start <= seconds && seconds < line.end; });
        if (cue == std::end(intro_cues))
            return {};
        return {cue->english, localization ? localization->text(cue->key) : std::string_view{}};
    }
} // namespace OpenBlood2
