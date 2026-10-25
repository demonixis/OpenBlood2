#pragma once
#include "Weapon.h"
#include "engine/model_store.h"
#include "engine/sprite.h"
#include <random>
class ServerDE;
class CBloodServerShell;
namespace OpenBlood2
{
    struct WeaponInput
    {
        bool primary = false, alternate = false, left = false;
        // Original HandleFiring (play input): every carried hand gets the command, and with
        // two weapons alternate fire becomes primary. Otherwise only the `left` hand fires
        // (diagnostic per-hand selection).
        bool all_hands = false;
    };
} // namespace OpenBlood2

// Selected CViewWeapon client animation/firing path, backed by an owned ABC
// track instead of a legacy local renderer object. No server HOBJECT is forged.
class CViewWeapon
{
  public:
    bool Create(DBYTE, bool, OpenBlood2::ModelStore&, OpenBlood2::Resources&, std::string&, DBYTE character = 0);
    bool Update(ServerDE&, CBloodServerShell&, HCLIENT, const OpenBlood2::WeaponInput&, DVector, DVector, double,
                std::string&);
    // Selected original placement, with the unimplemented bob inputs held at zero.
    bool AdjustAngleAndSetPos(float pitch, float yaw, DVector eye, OpenBlood2::NodePose&) const;
    // Local muzzle flash: source lifetime, separate authored primary/alternate sprites.
    std::array<std::shared_ptr<const OpenBlood2::Sprite>, 2> flashes;
    std::array<OpenBlood2::SpriteTrack, 2> flash_tracks;
    unsigned flash_visible = 0;
    double flash_started = 0;
    std::string flash_error, animation_sound_error;
    bool second_barrel = false;
    DVector FlashOffset(bool original_projection) const;
    DBYTE type = 0, character = 0;
    std::string model_path, skin_path;
    bool left = false;
    std::shared_ptr<const OpenBlood2::Model> model;
    OpenBlood2::AnimationTrack animation;
    WeaponState state = WS_HOLSTERED;
    std::string last_key, pending_sound;
    bool sound_indexed = false; // Request only; audio output is not ported.
    std::uint64_t fire_keys = 0, shots = 0, binocular_refusals = 0;
    bool binoculars_active = false; // Client m_bBinocularsActive, set by the shell before Update.
    float bomb_start = 0, bomb_held = 0, bomb_speed = 1000;
    DBYTE voodoo_hit = 0;
    FireResult last_result = FireResult::Invalid;

  private:
    bool PlayAnimation(std::uint32_t);
    std::uint32_t rest_ = UINT32_MAX, idle_ = UINT32_MAX, draw_ = UINT32_MAX, holster_ = UINT32_MAX;
    std::array<std::uint32_t, 2> start_{UINT32_MAX, UINT32_MAX}, fire_{UINT32_MAX, UINT32_MAX},
        stop_{UINT32_MAX, UINT32_MAX};
    float last_shot_time_ = 0, idle_start_ = 0, idle_delay_ = 20;
    bool last_alt_ = false, last_firing_ = false, switch_barrels_ = false;
    std::uint32_t original_fire_ = UINT32_MAX;
    DBYTE next_seed_ = 2;
    std::minstd_rand random_{1};
    OpenBlood2::Resources* resources_ = nullptr;
};
