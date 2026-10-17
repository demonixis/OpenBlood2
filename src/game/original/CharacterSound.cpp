// Selected original Anim_Sound voice helpers and PlayerObj model-key sound path.
// See docs/contracts/character_audio_contracts.md for preserved rules and unavailable callers.
#include "PlayerObj.h"
#include <algorithm>
#include <cmath>
#include <string_view>
using OpenBlood2::SoundId;
namespace
{
    bool bounded(std::string_view text, std::size_t maximum)
    {
        return text.size() <= maximum && text.find('\0') == std::string_view::npos;
    }
    bool same(std::string_view a, std::string_view b)
    {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                                  [](unsigned char x, unsigned char y)
                                                  {
                                                      auto lower = [](unsigned char c)
                                                      { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
                                                      return lower(x) == lower(y);
                                                  });
    }
} // namespace
CAnim_Sound::~CAnim_Sound()
{
    if (m_hVoxSound && g_pServerDE)
        g_pServerDE->KillSound(m_hVoxSound);
}
bool CAnim_Sound::SetSoundRoot(std::string_view root)
{
    if (!bounded(root, 255))
        return false;
    sound_root_ = root;
    return true;
}
bool CAnim_Sound::GetSoundPath(std::string& sound, int index) const
{
    if (!bounded(sound, 255))
        return false;
    auto path = sound_root_ + "\\" + sound;
    if (index)
        path += std::to_string(index) + ".wav";
    if (path.size() > 255)
        return false;
    sound = std::move(path);
    return true;
}
SoundId CAnim_Sound::PlaySound(HOBJECT object, std::string_view sound, float radius, int volume, bool get_handle)
{
    const bool valid_name = bounded(sound, 255);
    const std::string name = valid_name ? std::string(sound) : std::string{};
    ++sound_attempts;
    sound_error.clear();
    sound_resource.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(object) || !valid_name)
        sound_error = "Character sound owner or name unavailable";
    else
    {
        sound_resource = name;
        if (!GetSoundPath(sound_resource))
            sound_error = "Character sound path exceeds original 255-byte capacity";
        else
        {
            OpenBlood2::SoundRequest request;
            request.resource = sound_resource;
            request.speaker = object;
            request.inner_radius = radius * .1f;
            request.outer_radius = radius;
            request.volume = static_cast<float>(volume) / 100.f;
            request.priority = 3; // SOUNDPRIORITY_AI_MEDIUM
            const auto id = server->PlayCharacterSound(object, request, get_handle, sound_error);
            if (id)
            {
                ++sound_starts;
                return get_handle ? id : 0; // Original GETHANDLE distinction; unretained voices self-release.
            }
        }
    }
    ++sound_failures;
    return 0;
}
bool CAnim_Sound::PlayVoice(HOBJECT object, std::string_view sound, float radius, int volume)
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(object))
    {
        sound_error = "Character voice owner unavailable";
        return false;
    }
    if (m_hVoxSound)
    {
        bool done = false;
        if (server->IsSoundDone(m_hVoxSound, done) && !done)
        {
            ++voice_busy;
            return false;
        }
        server->KillSound(m_hVoxSound);
        m_hVoxSound = 0;
    }
    m_hVoxSound = PlaySound(object, sound, radius, volume, true);
    // Adaptation: expose actual start failure instead of the source's unconditional true.
    return m_hVoxSound != 0;
}
bool CPlayerObj::PlayPlayerSound(std::string_view sound, float radius, DBYTE volume, bool stream, DBYTE priority)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject))
        return false;
    ++sound_attempts;
    if (m_hCurSound)
        server->KillSound(m_hCurSound);
    m_hCurSound = 0; // The source interrupts even when the replacement subsequently fails.
    sound_resource = bounded(sound, 255) ? std::string(sound) : std::string{};
    sound_error.clear();
    if (stream)
        sound_error = "Character FileStream playback is unavailable";
    else if (sound_resource.empty())
        sound_error = "Character sound name unavailable or over limit";
    else
    {
        OpenBlood2::SoundRequest request;
        request.resource = sound_resource;
        request.speaker = m_hObject;
        request.inner_radius = radius * .1f;
        request.outer_radius = radius;
        request.volume = std::min<unsigned>(volume, 100) / 100.f;
        request.priority = static_cast<DBYTE>(priority + 4); // Original PLAYER base + caller modifier.
        m_hCurSound = server->PlayCharacterSound(m_hObject, request, true, sound_error);
    }
    // Source alerts after every playback attempt, including a missing WAV.
    DVector position;
    ++sound_alerts;
    sound_alert = {};
    if (server->GetObjectPos(m_hObject, &position) != DE_OK)
    {
        sound_alert.error = "Player sound position unavailable";
        ++unavailable_sound_alerts;
    }
    else if (!SendSoundTrigger(m_hObject, 2, position, radius, sound_alert))
        ++unavailable_sound_alerts;
    if (m_hCurSound)
        ++sound_starts;
    else
        ++sound_failures;
    return m_hCurSound != 0;
}
bool CPlayerObj::OnStringKey(ArgList* args)
{
    if (!args || !args->argv || args->argc < 2 || !args->argv[0] || !args->argv[1] ||
        !same(args->argv[0], "play_sound"))
        return false;
    ++sound_keys;
    if (same(args->argv[1], "footstep"))
    {
        if (m_bDisableFootsteps)
        {
            ++suppressed_footsteps;
            return true;
        }
        std::string sound;
        if (GetFootstepSound(sound))
            PlayPlayerSound(sound, 400, 70, false, 0);
        else
            ++unresolved_footsteps;
        return true;
    }
    std::string sound = args->argv[1];
    if (!bounded(sound, 251))
    {
        sound_error = "Model sound key exceeds original path capacity";
        ++sound_failures;
        return true;
    }
    sound += ".wav"; // The source appends even if the key already has a suffix.
    if (!m_Anim_Sound.GetSoundPath(sound))
    {
        sound_error = "Model sound key exceeds original path capacity";
        ++sound_failures;
        return true;
    }
    PlayPlayerSound(sound, 1000, 100, false, 0);
    return true;
}
bool CPlayerObj::GetFootstepSound(std::string& sound)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject))
        return false;
    DVector position{}, dimensions{};
    server->GetObjectPos(m_hObject, &position);
    if (server->GetObjectDims(m_hObject, &dimensions) != DE_OK)
        return false;
    position.y -= dimensions.y - 1.f;
    footstep_surface = m_eLastSurface;
    if (!server->GetPointContainerCode(position, footstep_container))
        return false;
    if (IsLiquid(footstep_container))
        footstep_surface = SURFTYPE_LIQUID;
    const char* material = nullptr;
    switch (footstep_surface)
    {
        case SURFTYPE_WOOD:
            material = "wood";
            break;
        case SURFTYPE_FLESH:
            material = "flesh";
            break;
        case SURFTYPE_GLASS:
            material = "glass";
            break;
        case SURFTYPE_METAL:
            material = "metal";
            break;
        case SURFTYPE_PLASTIC:
            material = "plastic";
            break;
        case SURFTYPE_STONE:
            material = "stone";
            break;
        case SURFTYPE_TERRAIN:
            material = "terrain";
            break;
        case SURFTYPE_LIQUID:
            material = "water";
            break;
        case SURFTYPE_CLOTH:
            material = "carpet";
            break;
        default:
            sound_error = "No original footstep mapping for surface " + std::to_string(footstep_surface);
            return false;
    }
    sound = std::string("sounds/player/footsteps/") + material + (m_bNextFootstepLeft ? "/leftm.wav" : "/rightm.wav");
    m_bNextFootstepLeft = !m_bNextFootstepLeft; // Caleb is the only admitted character; female entry still rejects.
    return true;
}
void CPlayerObj::UpdateSound()
{
    auto* server = GetServerDE();
    DVector velocity{};
    if (server->GetVelocity(m_hObject, &velocity) == DE_OK)
        m_bDisableFootsteps = std::hypot(velocity.x, velocity.y, velocity.z) < 50;
    if (m_hCurSound)
    {
        bool done = false;
        if (server->IsSoundDone(m_hCurSound, done) && done)
        {
            server->KillSound(m_hCurSound);
            m_hCurSound = 0;
        }
    }
}
