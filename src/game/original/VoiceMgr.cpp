// Selected original ClientShellDLL/VoiceMgr.cpp rules.
// Copyright (c) 1998 Monolith Productions Inc.
#include "VoiceMgr.h"
#include <algorithm>
#include <cmath>
void CVoiceGroup::Init(const OpenBlood2::Resources& resources, std::string_view standard, std::string_view custom_dir)
{
    *this = {};
    const auto add = [&](std::string_view directory)
    {
        std::string prefix;
        if (!OpenBlood2::resource_path(directory, prefix))
            return;
        prefix += '/';
        for (auto it = resources.index().lower_bound(prefix);
             it != resources.index().end() && it->first.starts_with(prefix); ++it)
        {
            const auto& path = it->first;
            const auto name = std::string_view(path).substr(prefix.size());
            if (name.size() <= 4 || name.find('/') != std::string_view::npos || !name.ends_with(".wav"))
                continue;
            if (sounds.size() >= 64 || path.size() >= 128)
                ++rejected;
            else
                sounds.push_back(path);
        }
    };
    add(custom_dir);
    custom = !sounds.empty();
    if (!custom)
        add(standard);
}
const std::string* CVoiceGroup::GetRandomSoundString(std::mt19937& random)
{
    if (sounds.empty())
        return nullptr;
    if (sounds.size() == 1)
        return &sounds.front();
    std::uniform_int_distribution<int> pick(0, static_cast<int>(sounds.size()) - 1);
    int selected = pick(random), safety = 200;
    while (selected == last && safety > 0)
    {
        selected = pick(random);
        --safety;
    }
    last = selected;
    return &sounds[static_cast<std::size_t>(selected)];
}
void CVoiceMgr::Init(const OpenBlood2::Resources& resources)
{
    static constexpr const char* characters[] = {"caleb", "ophelia", "ishmael", "gabriella"};
    static constexpr const char* events[] = {"Idle",    "BigGib", "Pain",    "Kill",   "Death", "Burning",
                                             "Powerup", "Spawn",  "Suicide", "Weapon", "Taunt", "Jump"};
    for (std::size_t c = 0; c < groups.size(); ++c)
        for (std::size_t e = 0; e < groups[c].size(); ++e)
        {
            const auto suffix = std::string(characters[c]) + '/' + events[e];
            CVoiceGroup next;
            next.Init(resources, "sounds/chosen/" + suffix, suffix);
            if (next.sounds == groups[c][e].sounds)
                next.last = groups[c][e].last;
            groups[c][e] = std::move(next);
        }
}
void CVoiceMgr::StopAll()
{
    ++stops;
    next_play_time = 0;
    if (voice)
        server_.KillSound(voice);
    voice = 0;
}
void CVoiceMgr::Reset()
{
    StopAll();
    played = {};
}
void CVoiceMgr::SetNextPlayTime(float delta)
{
    next_play_time = server_.GetTime() + delta;
}
void CVoiceMgr::InheritHistory(const CVoiceMgr& source)
{
    // NEW_LEVEL retains the original client, but this engine replaces the session.
    // Copy only values, after destination success; never transfer a sound handle.
    played = source.played;
    on = source.on;
    random_ = source.random_;
    for (std::size_t c = 0; c < groups.size(); ++c)
        for (std::size_t e = 0; e < groups[c].size(); ++e)
            if (groups[c][e].sounds == source.groups[c][e].sounds)
                groups[c][e].last = source.groups[c][e].last;
}
bool CVoiceMgr::IsOkToPlaySound(bool external_camera)
{
    reason.clear();
    error.clear();
    if (!on)
        reason = "disabled";
    else if (voice)
    {
        bool done = false;
        if (!server_.IsSoundDone(voice, done))
        {
            ++failures;
            error = "Retained voice is missing or has failed";
            return false;
        }
        if (!done)
            reason = "busy";
    }
    if (reason.empty() && external_camera)
        reason = "external camera";
    if (reason.empty() && !(server_.GetTime() > next_play_time))
        reason = "cooldown";
    if (!reason.empty())
        ++refused;
    return reason.empty();
}
bool CVoiceMgr::PlaySound(HOBJECT player, std::string_view resource, bool stream, float delay)
{
    StopAll(); // Source replacement happens even if the new sound is missing.
    last_resource = resource;
    error.clear();
    if (!std::isfinite(delay) || delay < 0)
    {
        error = "Invalid voice delay";
        ++failures;
        return false;
    }
    OpenBlood2::SoundRequest request;
    request.resource = resource;
    request.space = OpenBlood2::SoundSpace::Local;
    request.streaming = stream;
    request.priority = 5; // PLAYER_MEDIUM; no reverb flag in original local helper.
    request.reverb_requested = false;
    voice = server_.PlayCinematicSound(player, request, error);
    if (!voice)
    {
        ++failures;
        return false;
    }
    float duration = 1;
    if (server_.GetSoundDuration(voice, duration))
        duration += delay;
    else
        duration = 1;
    SetNextPlayTime(duration);
    ++starts;
    return true;
}
bool CVoiceMgr::PlayEventSound(HOBJECT player, int character, int event, bool external_camera)
{
    ++requests;
    if (!IsOkToPlaySound(external_camera))
        return false;
    if (character < 0 || character >= 4 || event < 0 || event >= 12)
    {
        ++failures;
        error = "Invalid voice group";
        return false;
    }
    const auto* sound = groups[character][event].GetRandomSoundString(random_);
    if (!sound)
    {
        ++failures;
        error = "Voice group contains no WAV resources";
        return false;
    }
    return PlaySound(player, *sound, event != 11); // Only JUMP is buffered in the source.
}
bool CVoiceMgr::PlayUniqueSound(HOBJECT player, int character, int unique, bool external_camera)
{
    ++requests;
    reason.clear();
    error.clear();
    if (character < 0 || character >= 4 || unique < 0 || unique >= 9)
    {
        ++failures;
        error = "Unique voice requires Caleb and an original unique ID";
        return false;
    }
    if (character != 0)
    {
        ++refused;
        reason = "unique voices are Caleb-only";
        return false;
    }
    if (played[character][unique])
    {
        ++refused;
        reason = "unique already played";
        return false;
    }
    if (!IsOkToPlaySound(external_camera))
        return false;
    const auto path = "sounds/chosen/caleb/unique/msc" + std::to_string(unique + 1) + "_cal.wav";
    if (!PlaySound(player, path, true))
        return false;
    played[character][unique] = true;
    return true;
}
