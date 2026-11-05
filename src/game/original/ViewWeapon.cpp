#include "ViewWeapon.h"
#include "CharacterProfiles.h"
#include "engine/collision.h"
#include "engine/model_commands.h"
#include "engine/texture.h"
#include <cmath>

using namespace OpenBlood2;
bool CViewWeapon::Create(DBYTE weapon, bool left_hand, ModelStore& models, Resources& resources, std::string& error,
                         DBYTE profile_id)
{
    return Create(weapon, left_hand, models, resources, error, profile_id, ResourcesMode::Presentation);
}
bool CViewWeapon::Create(DBYTE weapon, bool left_hand, ModelStore& models, Resources& resources, std::string& error,
                         DBYTE profile_id, ResourcesMode mode)
{
    const auto* definition = weapon_definition(weapon);
    const auto* profile = character_profile(profile_id);
    if (!definition || !profile)
    {
        error = "Unsupported view weapon";
        return false;
    }
    std::string resolved_model = left_hand ? definition->m_szLeftViewModelFilename : definition->m_szViewModelFilename;
    std::string resolved_skin = definition->m_szViewModelSkin;
    auto substitute = [&](std::string& path)
    {
        for (auto& c : path)
            if (c >= 'a' && c <= 'z')
                c = char(c - 'a' + 'A');
        const auto found = path.find("C_");
        if (found != path.npos)
            path[found] = profile->hands;
    };
    substitute(resolved_skin);
    if (weapon == WEAP_MELEE)
        substitute(resolved_model);
    std::shared_ptr<const Model> loaded;
    if (!models.load(resolved_model, loaded, error))
        return false;
    if (mode == ResourcesMode::Presentation && !resources.find(resolved_skin))
    {
        error = "Selected character weapon skin is not indexed: " + resolved_skin;
        return false;
    }
    *this = {};
    type = weapon;
    character = profile_id;
    model_path = std::move(resolved_model);
    skin_path = std::move(resolved_skin);
    left = left_hand;
    model = std::move(loaded);
    resources_ = &resources;
    auto index = [&](const char* name) { return animation_index(*model, name); };
    rest_ = index("static_model");
    idle_ = index("idle");
    draw_ = index("draw");
    holster_ = index("holster");
    start_ = {index("start_fire"), index("start_alt_fire")};
    fire_ = {index("fire"), index("alt_fire")};
    stop_ = {index("end_fire"), index("end_alt_fire")};
    // Original SetupViewModel: absent alternate clips reuse the complete primary sequence.
    if (idle_ == no_animation)
        idle_ = rest_;
    if (fire_[1] == no_animation)
    {
        start_[1] = start_[0];
        fire_[1] = fire_[0];
        stop_[1] = stop_[0];
    }
    original_fire_ = fire_[0];
    if (mode == ResourcesMode::Presentation)
    {
        SpriteStore sprites(resources);
        const char* paths[]{definition->m_szFlashSprite, definition->m_szAltFlashSprite};
        for (unsigned i = 0; i < 2; ++i)
        {
            if (!paths[i] || !paths[i][0])
                continue;
            std::string failure;
            if (sprites.load(paths[i], flashes[i], failure))
                for (const auto& frame : flashes[i]->frames)
                {
                    Texture image;
                    if (!read_texture_resource(resources, frame, image, failure))
                        break;
                }
            if (!failure.empty())
            {
                flashes[i].reset();
                if (!flash_error.empty())
                    flash_error += "; ";
                flash_error += failure;
            }
        }
    }
    animation.animation = no_animation;
    animation.looping = false;
    state = WS_DRAW;
    PlayAnimation(draw_);
    return true;
}
bool CViewWeapon::PlayAnimation(std::uint32_t clip)
{
    if (!model || clip == no_animation)
        return false;
    if (animation.animation == clip && animation.done)
        return false;
    if (animation.animation != clip)
    {
        animation.animation = clip;
        animation.reset();
    }
    return true;
}
void CViewWeapon::CancelFiringState()
{
    if (!model)
        return;
    // ViewWeapon.cpp:1241. Both resets are intentional, including repeated dead
    // frames; RNG/reload/barrel state is retained rather than constructing a gun.
    animation.reset();
    PlayAnimation(rest_);
    animation.reset();
    state = WS_REST;
    flash_visible = 0;
    pending_sound.clear(); // The authoritative death/loadout path stops the retained weapon loop.
}
bool CViewWeapon::Update(WeaponServices& services, const WeaponInput& input, DVector position, DVector direction,
                         double seconds, std::string& error)
{
    bomb_power = -1;
    if (!model)
        return true;
    if (!services.player_alive())
    {
        flash_visible = 0;
        return true;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (flashes[i])
            advance_sprite(*flashes[i], flash_tracks[i], seconds);
    if (flash_visible && services.time() >= flash_started + weapon_definition(type)->m_fFlashDuration &&
        services.time() >= flash_started + seconds + .05)
        flash_visible = 0;
    const auto* definition = weapon_definition(type);
    if (!definition || !services.weapon_selected(type, left))
    {
        error = "View/owner weapon association mismatch";
        return false;
    }
    const auto& data = *definition;
    auto ammo = [&](bool alt) { return services.ammunition(alt); };
    auto fire = [&]()
    {
        if (!services.has_player())
            return true;
        if (state != WS_FIRING && state != WS_ALT_FIRING &&
            !(inventory_bomb(type) && (state == WS_STOP_FIRING || state == WS_STOP_ALT_FIRING)))
            return true;
        const bool alternate = state == WS_ALT_FIRING || state == WS_STOP_ALT_FIRING;
        if (!services.weapon_selected(type, left))
            return true;
        if (binoculars_active)
        {
            ++binocular_refusals; // Original Fire returns before its seed draw.
            return true;
        }
        next_seed_ = static_cast<DBYTE>(2 + random_() % 254);
        auto result =
            services.fire(WeaponFire{position, direction, next_seed_, type, alternate, left, bomb_speed, voodoo_hit});
        last_result = result;
        if (result == FireResult::Fired)
        {
            ++shots;
            last_shot_time_ = services.time();
            last_alt_ = alternate;
            if (flashes[alternate])
            {
                flash_visible = alternate ? 2 : 1;
                flash_started = services.time();
            }
            pending_sound = alternate ? data.m_szAltFireSound : data.m_szFireSound;
            sound_indexed = resources_->find(pending_sound) != nullptr;
            return true;
        }
        if (result == FireResult::Dry || result == FireResult::Blocked)
            return true;
        error = "Original weapon fire rejected or damage delivery failed";
        return false;
    };
    const float time = services.time();
    const bool hand = input.all_hands || input.left == left;
    bool primary = input.primary && hand, alternate = input.alternate && hand;
    // Original client CShotgun::UpdateFiringState: the second barrel uses alt_fire's reload
    // animation, but is still a primary shot (one shell and primary damage/flash/sound).
    if (type == WEAP_SHOTGUN)
    {
        if (state == WS_REST && switch_barrels_)
        {
            switch_barrels_ = false;
            second_barrel = !second_barrel;
            fire_[0] = fire_[0] == fire_[1] ? original_fire_ : fire_[1];
        }
        else if (state == WS_FIRING && !switch_barrels_)
            switch_barrels_ = true;
        if (second_barrel && alternate)
        {
            primary = true;
            alternate = false;
        }
    }
    if (inventory_bomb(type) && state == WS_STOP_FIRING)
    {
        if (!PlayAnimation(stop_[0]))
        {
            PlayAnimation(draw_);
            state = WS_DRAW;
        }
        idle_start_ = time;
    }
    // Client CMelee::UpdateFiringState chooses fire/fire2 while resting.
    if (type == WEAP_MELEE && state == WS_REST)
        fire_[0] = animation_index(*model, random_() % 2 ? "fire" : "fire2");
    if (type == WEAP_VOODOO && state == WS_REST)
    {
        constexpr const char* names[]{"fire", "fire_lleg", "fire_larm", "fire_rarm", "fire_rleg"};
        voodoo_hit = static_cast<DBYTE>(random_() % 5);
        fire_[0] = animation_index(*model, names[voodoo_hit]);
    }
    // Original CViewWeapon::UpdateFiringState branches, same transition order.
    switch (state)
    {
        case WS_IDLE:
            if (!PlayAnimation(idle_))
            {
                PlayAnimation(rest_);
                state = WS_REST;
                idle_start_ = time;
                idle_delay_ = 20 + 40 * static_cast<float>(random_() - random_.min()) /
                                       static_cast<float>(random_.max() - random_.min());
                animation.looping = false;
            }
            [[fallthrough]];
        case WS_REST:
            if (time - idle_start_ > idle_delay_)
            {
                animation.looping = false;
                PlayAnimation(idle_);
                state = WS_IDLE;
            }
            if (time - last_shot_time_ > (last_alt_ ? data.m_fAltReloadTime : data.m_fReloadTime))
            {
                const int mode = primary && ammo(false) >= data.m_nAmmoUse                               ? 0
                                 : alternate && !data.m_bAltFireZoom && ammo(true) >= data.m_nAltAmmoUse ? 1
                                                                                                         : -1;
                if (mode >= 0)
                {
                    last_shot_time_ = time;
                    state = mode ? WS_START_ALT_FIRING : WS_START_FIRING;
                    if (!PlayAnimation(start_[mode]))
                    {
                        animation.looping = mode ? data.m_bAltLoopAnim : data.m_bLoopAnim;
                        PlayAnimation(fire_[mode]);
                        state = mode ? WS_ALT_FIRING : WS_FIRING;
                    }
                }
            }
            break;
        case WS_DRAW:
            if (!PlayAnimation(draw_))
            {
                PlayAnimation(rest_);
                state = WS_REST;
                animation.looping = false;
                idle_start_ = time;
            }
            break;
        case WS_HOLSTER:
            if (!PlayAnimation(holster_))
            {
                state = WS_HOLSTERED;
                animation.looping = false;
            }
            break;
        case WS_START_FIRING:
        case WS_START_ALT_FIRING:
        {
            const bool alt = state == WS_START_ALT_FIRING;
            if (!PlayAnimation(start_[alt]))
            {
                animation.looping = alt ? data.m_bAltLoopAnim : data.m_bLoopAnim;
                PlayAnimation(fire_[alt]);
                state = alt ? WS_ALT_FIRING : WS_FIRING;
            }
            idle_start_ = time;
            break;
        }
        case WS_FIRING:
        case WS_ALT_FIRING:
        {
            const bool alt = state == WS_ALT_FIRING, looping = alt ? data.m_bAltLoopAnim : data.m_bLoopAnim,
                       firing = alt ? alternate : primary;
            const auto next = alt ? WS_STOP_ALT_FIRING : WS_STOP_FIRING;
            const auto use = alt ? data.m_nAltAmmoUse : data.m_nAmmoUse;
            if (fire_[alt] == no_animation && !looping)
            {
                if (ammo(alt) >= use && !fire())
                    return false;
                state = next;
            }
            else if (looping)
            {
                if (!firing || ammo(alt) < use)
                {
                    state = next;
                    animation.looping = false;
                }
            }
            else if (data.m_bSemiAuto && !alt && !last_firing_ && firing && ammo(alt) >= use)
                animation.reset();
            else if (!PlayAnimation(fire_[alt]))
                state = next;
            idle_start_ = time;
            break;
        }
        case WS_STOP_FIRING:
        case WS_STOP_ALT_FIRING:
            if (!PlayAnimation(stop_[state == WS_STOP_ALT_FIRING]))
            {
                PlayAnimation(rest_);
                state = WS_REST;
                animation.looping = false;
            }
            idle_start_ = time;
            break;
        default:
            break;
    }
    if (inventory_bomb(type))
    {
        if (state == WS_START_FIRING)
            bomb_start = time;
        if (state == WS_FIRING)
        {
            bomb_held = time - bomb_start;
            bomb_power = std::min(bomb_held / .75f, 1.f); // fPercent = m_fTimeHeld / BOMBS_MAX_DIST_TIME.
            bomb_speed = 250 + 1500 * bomb_power;
            if ((type == WEAP_PROXIMITYBOMB || type == WEAP_TIMEBOMB) && bomb_held > 1.25f)
            {
                ++bomb_overholds; // pShell->CSPrint("Idiot! Let go next time!")
                bomb_speed = 0;
                bomb_held = 0;
                state = WS_STOP_FIRING;
                animation.looping = false;
            }
        }
        else
            animation.looping = false;
    }
    if (type == WEAP_ORB && state == WS_REST)
        animation.looping = true; // Original client COrb idle spin.
    last_firing_ = primary || alternate;
    if (animation.animation == no_animation)
        return true; // Original missing-animation fallback above.
    std::vector<std::uint32_t> events;
    if (!advance_animation(*model, animation, seconds, events, error))
        return false;
    for (auto key : events)
    {
        if (!services.has_player())
            break; // A hit callback may remove the owner.
        last_key = model->animations[animation.animation].keys[key].text;
        std::vector<std::vector<std::string>> parsed;
        if (!model_commands(last_key, parsed))
        {
            error = "Malformed weapon model key";
            return false;
        }
        for (const auto& command : parsed)
            for (std::size_t i = 0; i < command.size(); ++i)
            {
                const auto equal = [](std::string_view a, std::string_view b)
                { return !CaseLess{}(a, b) && !CaseLess{}(b, a); };
                if (equal(command[i], "FIRE_KEY"))
                {
                    ++fire_keys;
                    if (!fire())
                        return false;
                }
                else if ((equal(command[i], "SOUND_KEY") || equal(command[i], "SOUNDLOOP_KEY")) &&
                         i + 1 < command.size())
                {
                    const auto action =
                        equal(command[i], "SOUND_KEY") ? WeaponSoundAction::OneShot : WeaponSoundAction::Loop;
                    const auto path = "Sounds\\Weapons\\" + command[++i];
                    if (action == WeaponSoundAction::OneShot)
                    {
                        pending_sound = path;
                        sound_indexed = resources_->find(pending_sound) != nullptr;
                    }
                    // Original CViewWeapon::OnModelKey sends every SOUND_KEY to the server
                    // (CMSG_WEAPON_SOUND 0x00), which plays it from the player for any weapon
                    // (Mac-10 start/end_alt_fire stock flips, shotgun reload, ...).
                    // Retail keys include doubled backslashes. Preserve the
                    // existing normalization and the audio service's path checks.
                    std::string resource;
                    for (char c : path)
                    {
                        if (c == '\\')
                            c = '/';
                        if (c != '/' || resource.empty() || resource.back() != '/')
                            resource += c;
                    }
                    services.animation_sound(type, action, resource, animation_sound_error);
                }
                else if (equal(command[i], "SOUNDSTOP_KEY"))
                    services.animation_sound(type, WeaponSoundAction::Stop, {}, animation_sound_error);
            }
    }
    return true;
}
