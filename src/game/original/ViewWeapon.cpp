#include "ViewWeapon.h"
#include "BloodShells.h"
#include "engine/collision.h"
#include "engine/model_commands.h"
#include <cmath>

using namespace OpenBlood2;
bool CViewWeapon::AdjustAngleAndSetPos(float pitch, float yaw, DVector eye, NodePose& out) const
{
    out = {};
    const auto* data = weapon_definition(type);
    if (!model || !data || !std::isfinite(pitch) || !std::isfinite(yaw) || !physics_vector(eye))
        return false;
    out.rotation = rotation_from_euler({pitch, yaw, 0});
    const DVector offset{left ? -data->m_vViewModelOffset.x : data->m_vViewModelOffset.x, data->m_vViewModelOffset.y,
                         data->m_vViewModelOffset.z};
    out.position = vector_add(eye, rotate_vector(out.rotation, offset));
    return physics_vector(out.position);
}
bool CViewWeapon::Create(DBYTE weapon, bool left_hand, ModelStore& models, Resources& resources, std::string& error)
{
    const auto* definition = weapon_definition(weapon);
    if (!definition)
    {
        error = "Unsupported view weapon";
        return false;
    }
    std::shared_ptr<const Model> loaded;
    if (!models.load(left_hand ? definition->m_szLeftViewModelFilename : definition->m_szViewModelFilename, loaded,
                     error))
        return false;
    if (!resources.find(definition->m_szViewModelSkin))
    {
        error = "Weapon skin is not indexed";
        return false;
    }
    *this = {};
    type = weapon;
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
bool CViewWeapon::Update(ServerDE& engine, CBloodServerShell& server, HCLIENT client, const WeaponInput& input,
                         DVector position, DVector direction, double seconds, std::string& error)
{
    if (!model)
        return true;
    auto* player = server.Player(client);
    if (!player || player->m_damage.IsDead())
        return true;
    auto* weapon = player->m_InventoryMgr.CurrentWeapon(left);
    if (!weapon || weapon->data.m_nType != type)
    {
        error = "View/server weapon association mismatch";
        return false;
    }
    const auto& data = weapon->data;
    auto ammo = [&](bool alt) { return weapon->CheckAmmo(alt); };
    auto fire = [&]()
    {
        if (!server.Player(client))
            return true;
        if (state != WS_FIRING && state != WS_ALT_FIRING &&
            !(inventory_bomb(type) && (state == WS_STOP_FIRING || state == WS_STOP_ALT_FIRING)))
            return true;
        const bool alternate = state == WS_ALT_FIRING || state == WS_STOP_ALT_FIRING;
        if (player->m_InventoryMgr.CurrentWeapon(left) != weapon)
            return true;
        next_seed_ = static_cast<DBYTE>(2 + random_() % 254);
        auto result =
            server.OnMessage(client, WeaponFire{position, direction, next_seed_, type, alternate, left, bomb_speed});
        last_result = result;
        if (result == FireResult::Fired)
        {
            ++shots;
            last_shot_time_ = engine.GetTime();
            last_alt_ = alternate;
            pending_sound = alternate ? data.m_szAltFireSound : data.m_szFireSound;
            sound_indexed = resources_->find(pending_sound) != nullptr;
            return true;
        }
        if (result == FireResult::Dry || result == FireResult::Blocked)
            return true;
        error = "Original weapon fire rejected or damage delivery failed";
        return false;
    };
    const float time = engine.GetTime();
    const bool primary = input.primary && input.left == left, alternate = input.alternate && input.left == left;
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
                const int mode = primary && ammo(false) >= data.m_nAmmoUse       ? 0
                                 : alternate && ammo(true) >= data.m_nAltAmmoUse ? 1
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
            bomb_speed = 250 + 1500 * std::min(bomb_held / .75f, 1.f);
            if (type == WEAP_PROXIMITYBOMB && bomb_held > 1.25f)
            {
                bomb_speed = 0;
                bomb_held = 0;
                state = WS_STOP_FIRING;
                animation.looping = false;
            }
        }
        else
            animation.looping = false;
    }
    last_firing_ = primary || alternate;
    if (animation.animation == no_animation)
        return true; // Original missing-animation fallback above.
    std::vector<std::uint32_t> events;
    if (!advance_animation(*model, animation, seconds, events, error))
        return false;
    for (auto key : events)
    {
        if (!server.Player(client))
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
                else if (equal(command[i], "SOUND_KEY") && i + 1 < command.size())
                {
                    pending_sound = "Sounds\\Weapons\\" + command[++i];
                    sound_indexed = resources_->find(pending_sound) != nullptr;
                }
            }
    }
    return true;
}
