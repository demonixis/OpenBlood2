#pragma once
#include "B2BaseClass.h"
#include <array>
#include <span>
#include <string>
#include <vector>

// Original animation/node subset. Voice helpers and damage localization callers
// are not yet imported. See CHARACTER_ANIMATION_CONTRACTS.md.
inline constexpr unsigned NUM_ALL_NODES = 21, NUM_STD_NODES = 18;
extern const std::array<const char*, NUM_ALL_NODES> szNodes;
void CheckAnims(std::span<DDWORD>);
class CAnim_Sound
{
  public:
    bool SetAnimationIndexes(HOBJECT);
    bool GenerateHitSpheres(HOBJECT);
    bool ForceAnimation(HOBJECT, DDWORD);
    struct Binding
    {
        const char* field;
        DDWORD index;
    };
    std::vector<Binding> InspectAnimations() const;
    std::array<float, NUM_STD_NODES> m_fHitSpheres{};
    std::array<bool, NUM_STD_NODES> hit_nodes_found{};
    bool hit_spheres_valid = false;
    std::string hit_sphere_error;
    DDWORD m_nAnim_IDLE[4]{};
    DDWORD m_nAnim_TALK[5]{};
    DDWORD m_nAnim_WALK[9]{};
    DDWORD m_nAnim_WALK_INJURED_RLEG_RIFLE{};
    DDWORD m_nAnim_WALK_INJURED_RLEG_PISTOL{};
    DDWORD m_nAnim_WALK_INJURED_LLEG_RIFLE{};
    DDWORD m_nAnim_WALK_INJURED_LLEG_PISTOL{};
    DDWORD m_nAnim_RUN[9]{};
    DDWORD m_nAnim_JUMP[9]{};
    DDWORD m_nAnim_CROUCH[9]{};
    DDWORD m_nAnim_CRAWL[9]{};
    DDWORD m_nAnim_SWIM[9]{};
    DDWORD m_nAnim_STRAFERIGHT[9]{};
    DDWORD m_nAnim_STRAFELEFT[9]{};
    DDWORD m_nAnim_PICKUP_WEAPON{};
    DDWORD m_nAnim_SWITCH_WEAPON[3]{};
    DDWORD m_nAnim_FIRE_STAND[9]{};
    DDWORD m_nAnim_FIRE_WALK[9]{};
    DDWORD m_nAnim_FIRE_RUN[9]{};
    DDWORD m_nAnim_FIRE_JUMP[9]{};
    DDWORD m_nAnim_FIRE_CROUCH[9]{};
    DDWORD m_nAnim_FIRE_CRAWL[9]{};
    DDWORD m_nAnim_FALLING{};
    DDWORD m_nAnim_FALLING_UNCONTROL{};
    DDWORD m_nAnim_ROLL_FORWARD{};
    DDWORD m_nAnim_ROLL_RIGHT{};
    DDWORD m_nAnim_ROLL_LEFT{};
    DDWORD m_nAnim_ROLL_BACK{};
    DDWORD m_nAnim_HANDSPRING_FORWARD{};
    DDWORD m_nAnim_HANDSPRING_RIGHT{};
    DDWORD m_nAnim_HANDSPRING_LEFT{};
    DDWORD m_nAnim_HANDSPRING_BACK{};
    DDWORD m_nAnim_FLIP_FORWARD{};
    DDWORD m_nAnim_FLIP_RIGHT{};
    DDWORD m_nAnim_FLIP_LEFT{};
    DDWORD m_nAnim_FLIP_BACK{};
    DDWORD m_nAnim_DODGE_RIGHT{};
    DDWORD m_nAnim_DODGE_LEFT{};
    DDWORD m_nAnim_RECOIL[12]{};
    DDWORD m_nAnim_TAUNT[7]{};
    DDWORD m_nAnim_SPOTPLAYER_RIGHT{};
    DDWORD m_nAnim_SPOTPLAYER_LEFT{};
    DDWORD m_nAnim_SPOTPLAYER_POINT{};
    DDWORD m_nAnim_HUMILIATION[10]{};
    DDWORD m_nAnim_DEATH[12]{};
    DDWORD m_nAnim_CORPSE[12]{};
    DDWORD m_nAnim_LIMB[3]{};
    DDWORD m_nAnim_SPECIAL[9]{};
};
