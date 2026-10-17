// Original AI_Mgr.cpp:276-281,880-975 (Monolith, 1997). See ai_model_key_contracts.md.
#include "AI_Mgr.h"
#include "CameraObj.h"
#include "Sparam.h"
#include <charconv>
#include <cmath>
#include <utility>

namespace OpenBlood2
{
    const char* ai_key_outcome_name(AIKeyOutcome value)
    {
        switch (value)
        {
            case AIKeyOutcome::ExtraAction:
                return "Extra action accepted (inspect effect/shot)";
            case AIKeyOutcome::FireAttempt:
                return "Weapon attempt (inspect shot result)";
            case AIKeyOutcome::FireBlocked:
                return "Fire blocked";
            case AIKeyOutcome::None:
                return "No AI action";
            case AIKeyOutcome::Played:
                return "Sound started";
            case AIKeyOutcome::ChanceSkipped:
                return "Chance skipped";
            case AIKeyOutcome::VoiceBusy:
                return "Shared voice busy";
            case AIKeyOutcome::CameraBlocked:
                return "Server camera suppressed";
            case AIKeyOutcome::ScriptBlocked:
                return "Script state suppressed";
            case AIKeyOutcome::NoTarget:
                return "Fire has no target";
            case AIKeyOutcome::Unavailable:
                return "Required action unavailable";
            case AIKeyOutcome::Invalid:
                return "Invalid key or handle";
            case AIKeyOutcome::PlaybackFailed:
                return "Sound playback failed";
        }
        return "Unknown outcome";
    }
} // namespace OpenBlood2
namespace
{
    template <class T> bool number(std::string_view text, T& value)
    {
        constexpr std::string_view spaces = " \t\r\n\f\v";
        const auto first = text.find_first_not_of(spaces);
        if (first == text.npos || text.size() > 31)
            return false;
        text = text.substr(first, text.find_last_not_of(spaces) - first + 1);
        if (text.front() == '+')
        {
            text.remove_prefix(1);
            if (!text.empty() && text.front() == '-')
                return false;
        }
        if (text.empty() || text.front() == '+')
            return false;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        return ec == std::errc{} && end == text.data() + text.size() && std::isfinite(double(value));
    }
} // namespace
bool AI_Mgr::InitStatics(std::shared_ptr<CAnim_Sound> sound)
{
    if (!sound)
        return false;
    m_pAnim_Sound = std::move(sound);
    return true;
}
bool AI_Mgr::MC_Extra(std::string_view)
{
    ai_key_error = "Original AI MC_Extra is not integrated for this class";
    return false;
}
bool AI_Mgr::OnStringKey(ArgList* args)
{
    using OpenBlood2::AIKeyOutcome;
    auto* server = GetServerDE();
    if (!server || !args || args->argc < 1 || !args->argv || !args->argv[0])
        return false;
    const std::string_view input(args->argv[0]);
    ++model_keys;
    ai_key.assign(input.substr(0, 4096));
    ai_key_error.clear();
    ai_sound_resource.clear();
    selected_variant = 0;
    chance_roll = -1;
    key_outcome = AIKeyOutcome::None;
    auto fail = [&](AIKeyOutcome outcome, const char* reason)
    {
        ++key_failures;
        key_outcome = outcome;
        ai_key_error = reason;
        return true;
    };
    if (input.size() > 4096 || !server->IsObjectAlive(m_hObject))
        return fail(AIKeyOutcome::Invalid, "AI key input or owner unavailable");
    const std::string_view key = ai_key;
    constexpr const char* commands[]{"fire_key", "show_weapon", "extra_key", "play_sound"};
    unsigned branch = 4;
    std::string_view value;
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto status = Sparam_Get(key, commands[i], value);
        if (status == SparamStatus::Invalid)
            return fail(AIKeyOutcome::Invalid, "Malformed AI command parameter");
        if (status == SparamStatus::Found)
        {
            branch = i;
            break;
        }
    }
    if (branch == 0)
    {
        if (!m_hTarget)
        {
            ++no_target;
            key_outcome = AIKeyOutcome::NoTarget;
            return true; // Original early return also skips base motion keys.
        }
        if (!server->IsObjectAlive(m_hTarget))
            return fail(AIKeyOutcome::Invalid, "AI fire target is pending or stale");
        if (Fire())
            key_outcome = AIKeyOutcome::FireAttempt;
        else if (fire_result == FireResult::Blocked)
            key_outcome = AIKeyOutcome::FireBlocked;
        else
        {
            ++unavailable_keys;
            fail(AIKeyOutcome::Unavailable, fire_error.c_str());
        }
    }
    else if (branch == 2)
    {
        if (MC_Extra(value))
            key_outcome = AIKeyOutcome::ExtraAction;
        else
        {
            ++unavailable_keys;
            // Copy before fail assigns ai_key_error (its buffer may alias the input).
            const auto reason = ai_key_error;
            fail(AIKeyOutcome::Unavailable, reason.c_str());
        }
    }
    else if (branch == 1)
    {
        ++unavailable_keys;
        fail(AIKeyOutcome::Unavailable, "AI ShowHandModel unavailable; source reads an uninitialized buffer");
    }
    else if (branch == 3)
    {
        ++audio_keys;
        if (g_hActiveCamera && m_nState != StateScript && server->IsObjectAlive(g_hActiveCamera))
        {
            const auto* camera = dynamic_cast<const CameraObj*>(server->HandleToObject(g_hActiveCamera));
            if (!camera)
                return fail(AIKeyOutcome::Invalid, "Server active camera has the wrong class");
            if (!camera->AllowPlayerMovement())
            {
                ++camera_suppressed;
                key_outcome = AIKeyOutcome::CameraBlocked;
                return true; // Source returns before base motion dispatch.
            }
        }
        if (m_nState == StateScript)
        {
            ++script_suppressed;
            key_outcome = AIKeyOutcome::ScriptBlocked;
        }
        else
        {
            if (value.size() > 31)
                return fail(AIKeyOutcome::Invalid, "AI sound name exceeds original scalar buffer");
            std::string sound(value);
            std::string_view random_value;
            const auto random_status = Sparam_Get(key, "sound_random", random_value);
            if (random_status == SparamStatus::Invalid)
                return fail(AIKeyOutcome::Invalid, "Malformed sound_random parameter");
            if (random_status == SparamStatus::Found)
            {
                // SharedDefs.h unconditionally defines B2_LITE: NRES(nMax) is 3,
                // not nMax. Its atoi result is unused; preserve presence semantics.
                if (!server->RandomInt(1, 3, selected_variant))
                    return fail(AIKeyOutcome::Invalid, "AI variant draw failed");
                sound += std::to_string(selected_variant);
            }
            sound += ".wav";
            float radius = 1000;
            int volume = 100, chance = 100, voice = 0;
            auto option = [&](const char* name, auto& out)
            {
                std::string_view text;
                const auto status = Sparam_Get(key, name, text);
                return status == SparamStatus::Missing || (status == SparamStatus::Found && number(text, out));
            };
            if (!option("sound_radius", radius) || !option("sound_volume", volume) || !option("sound_chance", chance) ||
                !option("sound_voice", voice))
                return fail(AIKeyOutcome::Invalid, "Invalid numeric AI sound option");
            // Original IsRandomChance always consumes a 0..99 draw, including
            // 0/100/extreme percentages; the portable session RNG is not CRT-identical.
            if (!server->RandomInt(0, 99, chance_roll))
                return fail(AIKeyOutcome::Invalid, "AI chance draw failed");
            if (chance_roll >= chance)
            {
                ++chance_suppressed;
                key_outcome = AIKeyOutcome::ChanceSkipped;
            }
            else if (!m_pAnim_Sound)
                fail(AIKeyOutcome::Invalid, "AI InitStatics sound table is unavailable");
            else
            {
                const auto starts = m_pAnim_Sound->sound_starts, busy = m_pAnim_Sound->voice_busy;
                if (voice)
                    m_pAnim_Sound->PlayVoice(m_hObject, sound, radius, volume);
                else
                    m_pAnim_Sound->PlaySound(m_hObject, sound, radius, volume);
                if (m_pAnim_Sound->voice_busy != busy)
                {
                    ++busy_voices;
                    key_outcome = AIKeyOutcome::VoiceBusy;
                }
                else
                {
                    ai_sound_resource = m_pAnim_Sound->sound_resource;
                    if (m_pAnim_Sound->sound_starts != starts)
                    {
                        ++audio_starts;
                        key_outcome = AIKeyOutcome::Played;
                    }
                    else
                        fail(AIKeyOutcome::PlaybackFailed, m_pAnim_Sound->sound_error.c_str());
                }
            }
        }
    }
    const bool base = CBaseCharacter::OnStringKey(args);
    return branch != 4 || base;
}
