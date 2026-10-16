// Selected original Anim_Sound.cpp animation mappings and hit-sphere rules.
#include "Anim_Sound.h"
#include <cmath>
const std::array<const char*, NUM_ALL_NODES> szNodes{
    "head",   "neck",    "torso",  "pelvis", "ru_arm", "rl_arm",  "r_hand", "lu_arm", "ll_arm", "l_hand", "lu_leg",
    "ll_leg", "l_ankle", "l_foot", "ru_leg", "rl_leg", "r_ankle", "r_foot", "obj",    "l_gun",  "r_gun"};
void CheckAnims(std::span<DDWORD> animations)
{
    // DDWORD is unsigned: the original < 0 branch is unreachable. Slot zero
    // remains untouched, including UINT32_MAX; >999 copies the previous slot.
    for (std::size_t i = 1; i < animations.size(); ++i)
        if (animations[i] > 999)
            animations[i] = animations[i - 1];
}
bool CAnim_Sound::SetAnimationIndexes(HOBJECT m_hObject)
{
    ServerDE* pServerDE = BaseClass::GetServerDE();
    std::string current_name;
    if (!pServerDE || !m_hObject ||
        !pServerDE->GetModelAnimationName(m_hObject, pServerDE->GetModelAnimation(m_hObject), current_name))
        return false;

    m_nAnim_IDLE[0] = pServerDE->GetAnimIndex(m_hObject, "idle1");
    m_nAnim_IDLE[1] = pServerDE->GetAnimIndex(m_hObject, "idle2");
    m_nAnim_IDLE[2] = pServerDE->GetAnimIndex(m_hObject, "idle3");
    m_nAnim_IDLE[3] = pServerDE->GetAnimIndex(m_hObject, "idle4");

    m_nAnim_TALK[0] = pServerDE->GetAnimIndex(m_hObject, "talk1");
    m_nAnim_TALK[1] = pServerDE->GetAnimIndex(m_hObject, "talk2");
    m_nAnim_TALK[2] = pServerDE->GetAnimIndex(m_hObject, "talk3");
    m_nAnim_TALK[3] = pServerDE->GetAnimIndex(m_hObject, "talk4");
    m_nAnim_TALK[4] = pServerDE->GetAnimIndex(m_hObject, "talk5");

    m_nAnim_WALK[0] = pServerDE->GetAnimIndex(m_hObject, "walk_rifle");
    m_nAnim_WALK[1] = pServerDE->GetAnimIndex(m_hObject, "walk_rifle");
    m_nAnim_WALK[2] = pServerDE->GetAnimIndex(m_hObject, "walk_pistol");
    m_nAnim_WALK[3] = pServerDE->GetAnimIndex(m_hObject, "walk_pistol");
    m_nAnim_WALK[4] = pServerDE->GetAnimIndex(m_hObject, "walk_knife1");
    m_nAnim_WALK[5] = pServerDE->GetAnimIndex(m_hObject, "walk_knife1");
    m_nAnim_WALK[6] = pServerDE->GetAnimIndex(m_hObject, "walk_knife1");
    m_nAnim_WALK[7] = pServerDE->GetAnimIndex(m_hObject, "walk_pistol");
    m_nAnim_WALK[8] = pServerDE->GetAnimIndex(m_hObject, "walk_pistol");

    m_nAnim_WALK_INJURED_RLEG_RIFLE = pServerDE->GetAnimIndex(m_hObject, "walk_irleg_rifle");
    m_nAnim_WALK_INJURED_RLEG_PISTOL = pServerDE->GetAnimIndex(m_hObject, "walk_irleg_nogun");

    m_nAnim_WALK_INJURED_LLEG_RIFLE = pServerDE->GetAnimIndex(m_hObject, "walk_illeg_rifle");
    m_nAnim_WALK_INJURED_LLEG_PISTOL = pServerDE->GetAnimIndex(m_hObject, "walk_illeg_nogun");

    m_nAnim_RUN[0] = pServerDE->GetAnimIndex(m_hObject, "run_rifle");
    m_nAnim_RUN[1] = pServerDE->GetAnimIndex(m_hObject, "run_rifle");
    m_nAnim_RUN[2] = pServerDE->GetAnimIndex(m_hObject, "run_pistol");
    m_nAnim_RUN[3] = pServerDE->GetAnimIndex(m_hObject, "run_pistol");
    m_nAnim_RUN[4] = pServerDE->GetAnimIndex(m_hObject, "run_knife1");
    m_nAnim_RUN[5] = pServerDE->GetAnimIndex(m_hObject, "run_knife1");
    m_nAnim_RUN[6] = pServerDE->GetAnimIndex(m_hObject, "run_knife1");
    m_nAnim_RUN[7] = pServerDE->GetAnimIndex(m_hObject, "run_pistol");
    m_nAnim_RUN[8] = pServerDE->GetAnimIndex(m_hObject, "run_pistol");

    m_nAnim_JUMP[0] = pServerDE->GetAnimIndex(m_hObject, "jmp_rifle");
    m_nAnim_JUMP[1] = pServerDE->GetAnimIndex(m_hObject, "jmp_rifle");
    m_nAnim_JUMP[2] = pServerDE->GetAnimIndex(m_hObject, "jmp_pistol");
    m_nAnim_JUMP[3] = pServerDE->GetAnimIndex(m_hObject, "jmp_pistol");
    m_nAnim_JUMP[4] = pServerDE->GetAnimIndex(m_hObject, "jmp_knife1");
    m_nAnim_JUMP[5] = pServerDE->GetAnimIndex(m_hObject, "jmp_knife1");
    m_nAnim_JUMP[6] = pServerDE->GetAnimIndex(m_hObject, "jmp_knife1");
    m_nAnim_JUMP[7] = pServerDE->GetAnimIndex(m_hObject, "jmp_pistol");
    m_nAnim_JUMP[8] = pServerDE->GetAnimIndex(m_hObject, "jmp_pistol");

    m_nAnim_CROUCH[0] = pServerDE->GetAnimIndex(m_hObject, "crouch_rifle");
    m_nAnim_CROUCH[1] = pServerDE->GetAnimIndex(m_hObject, "crouch_rifle");
    m_nAnim_CROUCH[2] = pServerDE->GetAnimIndex(m_hObject, "crouch_1pistol");
    m_nAnim_CROUCH[3] = pServerDE->GetAnimIndex(m_hObject, "crouch_1pistol");
    m_nAnim_CROUCH[4] = pServerDE->GetAnimIndex(m_hObject, "crouch_knife1");
    m_nAnim_CROUCH[5] = pServerDE->GetAnimIndex(m_hObject, "crouch_knife1");
    m_nAnim_CROUCH[6] = pServerDE->GetAnimIndex(m_hObject, "crouch_knife1");
    m_nAnim_CROUCH[7] = pServerDE->GetAnimIndex(m_hObject, "crouch_1pistol");
    m_nAnim_CROUCH[8] = pServerDE->GetAnimIndex(m_hObject, "crouch_1pistol");

    m_nAnim_CRAWL[0] = pServerDE->GetAnimIndex(m_hObject, "crawl_rifle");
    m_nAnim_CRAWL[1] = pServerDE->GetAnimIndex(m_hObject, "crawl_rifle");
    m_nAnim_CRAWL[2] = pServerDE->GetAnimIndex(m_hObject, "crawl_1pistol");
    m_nAnim_CRAWL[3] = pServerDE->GetAnimIndex(m_hObject, "crawl_1pistol");
    m_nAnim_CRAWL[4] = pServerDE->GetAnimIndex(m_hObject, "crawl_knife1");
    m_nAnim_CRAWL[5] = pServerDE->GetAnimIndex(m_hObject, "crawl_knife1");
    m_nAnim_CRAWL[6] = pServerDE->GetAnimIndex(m_hObject, "crawl_knife1");
    m_nAnim_CRAWL[7] = pServerDE->GetAnimIndex(m_hObject, "crawl_1pistol");
    m_nAnim_CRAWL[8] = pServerDE->GetAnimIndex(m_hObject, "crawl_1pistol");

    m_nAnim_SWIM[0] = pServerDE->GetAnimIndex(m_hObject, "swim_rifle");
    m_nAnim_SWIM[1] = pServerDE->GetAnimIndex(m_hObject, "swim_rifle");
    m_nAnim_SWIM[2] = pServerDE->GetAnimIndex(m_hObject, "swim_pistol");
    m_nAnim_SWIM[3] = pServerDE->GetAnimIndex(m_hObject, "swim_pistol");
    m_nAnim_SWIM[4] = pServerDE->GetAnimIndex(m_hObject, "swim_knife1");
    m_nAnim_SWIM[5] = pServerDE->GetAnimIndex(m_hObject, "swim_knife1");
    m_nAnim_SWIM[6] = pServerDE->GetAnimIndex(m_hObject, "swim_knife1");
    m_nAnim_SWIM[7] = pServerDE->GetAnimIndex(m_hObject, "swim_pistol");
    m_nAnim_SWIM[8] = pServerDE->GetAnimIndex(m_hObject, "swim_pistol");

    m_nAnim_STRAFERIGHT[0] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_rifle");
    m_nAnim_STRAFERIGHT[1] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_rifle");
    m_nAnim_STRAFERIGHT[2] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_1pistol");
    m_nAnim_STRAFERIGHT[3] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_2pistol");
    m_nAnim_STRAFERIGHT[4] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_1pistol");

    m_nAnim_STRAFELEFT[0] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_rifle");
    m_nAnim_STRAFELEFT[1] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_rifle");
    m_nAnim_STRAFELEFT[2] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_1pistol");
    m_nAnim_STRAFELEFT[3] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_2pistol");
    m_nAnim_STRAFELEFT[4] = pServerDE->GetAnimIndex(m_hObject, "strafe_right_1pistol");

    m_nAnim_PICKUP_WEAPON = pServerDE->GetAnimIndex(m_hObject, "pickup_weapon");

    m_nAnim_SWITCH_WEAPON[0] = pServerDE->GetAnimIndex(m_hObject, "switch_weapon_rifle");
    m_nAnim_SWITCH_WEAPON[1] = pServerDE->GetAnimIndex(m_hObject, "switch_weapon_rifle");
    m_nAnim_SWITCH_WEAPON[2] = pServerDE->GetAnimIndex(m_hObject, "switch_weapon_2pistol");

    m_nAnim_FIRE_STAND[0] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_rifle");
    m_nAnim_FIRE_STAND[1] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_autorifle");
    m_nAnim_FIRE_STAND[2] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_1pistol");
    m_nAnim_FIRE_STAND[3] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_2pistol");
    m_nAnim_FIRE_STAND[4] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_knife1");
    m_nAnim_FIRE_STAND[5] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_knife2");
    m_nAnim_FIRE_STAND[6] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_knife3");
    m_nAnim_FIRE_STAND[7] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_grenade");
    m_nAnim_FIRE_STAND[8] = pServerDE->GetAnimIndex(m_hObject, "fire_stand_magic");

    m_nAnim_FIRE_WALK[0] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_rifle");
    m_nAnim_FIRE_WALK[1] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_autorifle");
    m_nAnim_FIRE_WALK[2] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_1pistol");
    m_nAnim_FIRE_WALK[3] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_2pistol");
    m_nAnim_FIRE_WALK[4] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_knife1");
    m_nAnim_FIRE_WALK[5] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_knife2");
    m_nAnim_FIRE_WALK[6] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_knife3");
    m_nAnim_FIRE_WALK[7] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_grenade");
    m_nAnim_FIRE_WALK[8] = pServerDE->GetAnimIndex(m_hObject, "fire_walk_magic");

    m_nAnim_FIRE_RUN[0] = pServerDE->GetAnimIndex(m_hObject, "fire_run_rifle");
    m_nAnim_FIRE_RUN[1] = pServerDE->GetAnimIndex(m_hObject, "fire_run_autorifle");
    m_nAnim_FIRE_RUN[2] = pServerDE->GetAnimIndex(m_hObject, "fire_run_1pistol");
    m_nAnim_FIRE_RUN[3] = pServerDE->GetAnimIndex(m_hObject, "fire_run_2pistol");
    m_nAnim_FIRE_RUN[4] = pServerDE->GetAnimIndex(m_hObject, "fire_run_knife1");
    m_nAnim_FIRE_RUN[5] = pServerDE->GetAnimIndex(m_hObject, "fire_run_knife2");
    m_nAnim_FIRE_RUN[6] = pServerDE->GetAnimIndex(m_hObject, "fire_run_knife3");
    m_nAnim_FIRE_RUN[7] = pServerDE->GetAnimIndex(m_hObject, "fire_run_grenade");
    m_nAnim_FIRE_RUN[8] = pServerDE->GetAnimIndex(m_hObject, "fire_run_magic");

    m_nAnim_FIRE_JUMP[0] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_rifle");
    m_nAnim_FIRE_JUMP[1] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_autorifle");
    m_nAnim_FIRE_JUMP[2] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_1pistol");
    m_nAnim_FIRE_JUMP[3] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_2pistol");
    m_nAnim_FIRE_JUMP[4] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_knife1");
    m_nAnim_FIRE_JUMP[5] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_knife2");
    m_nAnim_FIRE_JUMP[6] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_knife3");
    m_nAnim_FIRE_JUMP[7] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_grenade");
    m_nAnim_FIRE_JUMP[8] = pServerDE->GetAnimIndex(m_hObject, "fire_jump_magic");

    m_nAnim_FIRE_CROUCH[0] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_rifle");
    m_nAnim_FIRE_CROUCH[1] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_autorifle");
    m_nAnim_FIRE_CROUCH[2] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_1pistol");
    m_nAnim_FIRE_CROUCH[3] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_2pistol");
    m_nAnim_FIRE_CROUCH[4] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_knife1");
    m_nAnim_FIRE_CROUCH[5] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_knife2");
    m_nAnim_FIRE_CROUCH[6] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_knife3");
    m_nAnim_FIRE_CROUCH[7] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_grenade");
    m_nAnim_FIRE_CROUCH[8] = pServerDE->GetAnimIndex(m_hObject, "fire_crouch_magic");

    m_nAnim_FIRE_CRAWL[0] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_rifle");
    m_nAnim_FIRE_CRAWL[1] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_autorifle");
    m_nAnim_FIRE_CRAWL[2] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_1pistol");
    m_nAnim_FIRE_CRAWL[3] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_2pistol");
    m_nAnim_FIRE_CRAWL[4] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_knife1");
    m_nAnim_FIRE_CRAWL[5] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_knife2");
    m_nAnim_FIRE_CRAWL[6] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_knife3");
    m_nAnim_FIRE_CRAWL[7] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_grenade");
    m_nAnim_FIRE_CRAWL[8] = pServerDE->GetAnimIndex(m_hObject, "fire_crawl_magic");

    m_nAnim_FALLING = pServerDE->GetAnimIndex(m_hObject, "falling");
    m_nAnim_FALLING_UNCONTROL = pServerDE->GetAnimIndex(m_hObject, "falling_uncontrol");

    m_nAnim_ROLL_FORWARD = pServerDE->GetAnimIndex(m_hObject, "roll_forward");
    m_nAnim_ROLL_RIGHT = pServerDE->GetAnimIndex(m_hObject, "roll_right");
    m_nAnim_ROLL_LEFT = pServerDE->GetAnimIndex(m_hObject, "roll_left");
    m_nAnim_ROLL_BACK = pServerDE->GetAnimIndex(m_hObject, "roll_back");

    m_nAnim_HANDSPRING_FORWARD = pServerDE->GetAnimIndex(m_hObject, "handspring_forward");
    m_nAnim_HANDSPRING_RIGHT = pServerDE->GetAnimIndex(m_hObject, "handspring_right");
    m_nAnim_HANDSPRING_LEFT = pServerDE->GetAnimIndex(m_hObject, "handspring_left");
    m_nAnim_HANDSPRING_BACK = pServerDE->GetAnimIndex(m_hObject, "handspring_back");

    m_nAnim_FLIP_FORWARD = pServerDE->GetAnimIndex(m_hObject, "flip_forward");
    m_nAnim_FLIP_RIGHT = pServerDE->GetAnimIndex(m_hObject, "flip_right");
    m_nAnim_FLIP_LEFT = pServerDE->GetAnimIndex(m_hObject, "flip_left");
    m_nAnim_FLIP_BACK = pServerDE->GetAnimIndex(m_hObject, "flip_back");

    m_nAnim_DODGE_RIGHT = pServerDE->GetAnimIndex(m_hObject, "dodge_right");
    m_nAnim_DODGE_LEFT = pServerDE->GetAnimIndex(m_hObject, "dodge_left");

    m_nAnim_RECOIL[0] = pServerDE->GetAnimIndex(m_hObject, "recoil_head1");
    m_nAnim_RECOIL[1] = pServerDE->GetAnimIndex(m_hObject, "recoil_chest1");
    m_nAnim_RECOIL[2] = pServerDE->GetAnimIndex(m_hObject, "recoil_rchest1");
    m_nAnim_RECOIL[3] = pServerDE->GetAnimIndex(m_hObject, "recoil_lchest1");
    m_nAnim_RECOIL[4] = pServerDE->GetAnimIndex(m_hObject, "recoil_lleg1");
    m_nAnim_RECOIL[5] = pServerDE->GetAnimIndex(m_hObject, "recoil_rleg1");
    m_nAnim_RECOIL[6] = pServerDE->GetAnimIndex(m_hObject, "recoil_head2");
    m_nAnim_RECOIL[7] = pServerDE->GetAnimIndex(m_hObject, "recoil_chest2");
    m_nAnim_RECOIL[8] = pServerDE->GetAnimIndex(m_hObject, "recoil_rchest2");
    m_nAnim_RECOIL[9] = pServerDE->GetAnimIndex(m_hObject, "recoil_lchest2");
    m_nAnim_RECOIL[10] = pServerDE->GetAnimIndex(m_hObject, "recoil_lleg2");
    m_nAnim_RECOIL[11] = pServerDE->GetAnimIndex(m_hObject, "recoil_rleg2");

    m_nAnim_TAUNT[0] = pServerDE->GetAnimIndex(m_hObject, "taunt_dance1");
    m_nAnim_TAUNT[1] = pServerDE->GetAnimIndex(m_hObject, "taunt_dance2");
    m_nAnim_TAUNT[2] = pServerDE->GetAnimIndex(m_hObject, "taunt_dance3");
    m_nAnim_TAUNT[3] = pServerDE->GetAnimIndex(m_hObject, "taunt_dance4");
    m_nAnim_TAUNT[4] = pServerDE->GetAnimIndex(m_hObject, "taunt_flip");
    m_nAnim_TAUNT[5] = pServerDE->GetAnimIndex(m_hObject, "taunt_wave");
    m_nAnim_TAUNT[6] = pServerDE->GetAnimIndex(m_hObject, "taunt_beg");

    m_nAnim_SPOTPLAYER_RIGHT = pServerDE->GetAnimIndex(m_hObject, "spot_right");
    m_nAnim_SPOTPLAYER_LEFT = pServerDE->GetAnimIndex(m_hObject, "spot_left");
    m_nAnim_SPOTPLAYER_POINT = pServerDE->GetAnimIndex(m_hObject, "spot_point");

    // Set the humiliation animations...

    m_nAnim_HUMILIATION[0] = pServerDE->GetAnimIndex(m_hObject, "humiliation_01");
    m_nAnim_HUMILIATION[1] = pServerDE->GetAnimIndex(m_hObject, "humiliation_02");
    m_nAnim_HUMILIATION[2] = pServerDE->GetAnimIndex(m_hObject, "humiliation_03");
    m_nAnim_HUMILIATION[3] = pServerDE->GetAnimIndex(m_hObject, "humiliation_04");

    if (m_nAnim_HUMILIATION[3] == UINT32_MAX)
    {
        m_nAnim_HUMILIATION[3] = pServerDE->GetAnimIndex(m_hObject, "humiliation_05");
    }

    // Gabby and Ishmael's humiliation animations are spelled wrong, so deal with that here...

    if (m_nAnim_HUMILIATION[0] == UINT32_MAX)
        m_nAnim_HUMILIATION[0] = pServerDE->GetAnimIndex(m_hObject, "humilation_01");
    if (m_nAnim_HUMILIATION[1] == UINT32_MAX)
        m_nAnim_HUMILIATION[1] = pServerDE->GetAnimIndex(m_hObject, "humilation_02");
    if (m_nAnim_HUMILIATION[2] == UINT32_MAX)
        m_nAnim_HUMILIATION[2] = pServerDE->GetAnimIndex(m_hObject, "humilation_03");
    if (m_nAnim_HUMILIATION[3] == UINT32_MAX)
        m_nAnim_HUMILIATION[3] = pServerDE->GetAnimIndex(m_hObject, "humilation_05");

    // Playable enemy characters only have 01, 03, and 05--so fill in index 1 if necessary...

    if (m_nAnim_HUMILIATION[1] == UINT32_MAX)
        m_nAnim_HUMILIATION[1] = pServerDE->GetAnimIndex(m_hObject, "humiliation_01");

    // Continue...

    m_nAnim_DEATH[0] = pServerDE->GetAnimIndex(m_hObject, "death_head1");
    m_nAnim_DEATH[1] = pServerDE->GetAnimIndex(m_hObject, "death_chest1");
    m_nAnim_DEATH[2] = pServerDE->GetAnimIndex(m_hObject, "death_rchest1");
    m_nAnim_DEATH[3] = pServerDE->GetAnimIndex(m_hObject, "death_lchest1");
    m_nAnim_DEATH[4] = pServerDE->GetAnimIndex(m_hObject, "death_lleg1");
    m_nAnim_DEATH[5] = pServerDE->GetAnimIndex(m_hObject, "death_rleg1");
    m_nAnim_DEATH[6] = pServerDE->GetAnimIndex(m_hObject, "death_head2");
    m_nAnim_DEATH[7] = pServerDE->GetAnimIndex(m_hObject, "death_chest2");
    m_nAnim_DEATH[8] = pServerDE->GetAnimIndex(m_hObject, "death_rchest2");
    m_nAnim_DEATH[9] = pServerDE->GetAnimIndex(m_hObject, "death_lchest2");
    m_nAnim_DEATH[10] = pServerDE->GetAnimIndex(m_hObject, "death_lleg2");
    m_nAnim_DEATH[11] = pServerDE->GetAnimIndex(m_hObject, "death_rleg2");

    m_nAnim_CORPSE[0] = pServerDE->GetAnimIndex(m_hObject, "corpse_head1");
    m_nAnim_CORPSE[1] = pServerDE->GetAnimIndex(m_hObject, "corpse_chest");
    m_nAnim_CORPSE[2] = pServerDE->GetAnimIndex(m_hObject, "corpse_rchest");
    m_nAnim_CORPSE[3] = pServerDE->GetAnimIndex(m_hObject, "corpse_lchest");
    m_nAnim_CORPSE[4] = pServerDE->GetAnimIndex(m_hObject, "corpse_lleg1");
    m_nAnim_CORPSE[5] = pServerDE->GetAnimIndex(m_hObject, "corpse_rleg1");
    m_nAnim_CORPSE[6] = pServerDE->GetAnimIndex(m_hObject, "corpse_head2");
    m_nAnim_CORPSE[7] = pServerDE->GetAnimIndex(m_hObject, "corpse_chest2");
    m_nAnim_CORPSE[8] = pServerDE->GetAnimIndex(m_hObject, "corpse_rchest2");
    m_nAnim_CORPSE[9] = pServerDE->GetAnimIndex(m_hObject, "corpse_lchest2");
    m_nAnim_CORPSE[10] = pServerDE->GetAnimIndex(m_hObject, "corpse_lleg2");
    m_nAnim_CORPSE[11] = pServerDE->GetAnimIndex(m_hObject, "corpse_rleg2");

    m_nAnim_LIMB[0] = pServerDE->GetAnimIndex(m_hObject, "limb_head");
    m_nAnim_LIMB[1] = pServerDE->GetAnimIndex(m_hObject, "limb_arm");
    m_nAnim_LIMB[2] = pServerDE->GetAnimIndex(m_hObject, "limb_leg");

    m_nAnim_SPECIAL[0] = pServerDE->GetAnimIndex(m_hObject, "special1");
    m_nAnim_SPECIAL[1] = pServerDE->GetAnimIndex(m_hObject, "special2");
    m_nAnim_SPECIAL[2] = pServerDE->GetAnimIndex(m_hObject, "special3");
    m_nAnim_SPECIAL[3] = pServerDE->GetAnimIndex(m_hObject, "special4");
    m_nAnim_SPECIAL[4] = pServerDE->GetAnimIndex(m_hObject, "special5");
    m_nAnim_SPECIAL[5] = pServerDE->GetAnimIndex(m_hObject, "special6");
    m_nAnim_SPECIAL[6] = pServerDE->GetAnimIndex(m_hObject, "special7");
    m_nAnim_SPECIAL[7] = pServerDE->GetAnimIndex(m_hObject, "special8");
    m_nAnim_SPECIAL[8] = pServerDE->GetAnimIndex(m_hObject, "special9");

    // SCHLEGZ 3/11/98 7:33:27 PM: make sure we have valid and appropriate anims
    CheckAnims(m_nAnim_IDLE);
    CheckAnims(m_nAnim_WALK);
    CheckAnims(m_nAnim_RUN);
    CheckAnims(m_nAnim_JUMP);
    CheckAnims(m_nAnim_CROUCH);
    CheckAnims(m_nAnim_CRAWL);
    CheckAnims(m_nAnim_SWIM);
    CheckAnims(m_nAnim_FIRE_STAND);
    CheckAnims(m_nAnim_FIRE_WALK);
    CheckAnims(m_nAnim_FIRE_RUN);
    CheckAnims(m_nAnim_FIRE_JUMP);
    CheckAnims(m_nAnim_FIRE_CROUCH);
    CheckAnims(m_nAnim_FIRE_CRAWL);
    CheckAnims(m_nAnim_RECOIL);
    CheckAnims(m_nAnim_DEATH);
    CheckAnims(m_nAnim_CORPSE);

    return true;
}

bool CAnim_Sound::GenerateHitSpheres(HOBJECT object)
{
    m_fHitSpheres = {};
    hit_nodes_found = {};
    hit_spheres_valid = false;
    hit_sphere_error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !object)
    {
        hit_sphere_error = "Missing model object";
        return false;
    }
    std::array<DVector, NUM_STD_NODES> vNodePos{};
    DRotation rotation;
    for (unsigned i = 0; i < NUM_STD_NODES; ++i)
    {
        hit_nodes_found[i] = server->GetModelNodeTransform(object, szNodes[i], &vNodePos[i], &rotation);
        if (!hit_nodes_found[i])
        {
            // The source leaves node zero uninitialized on failure. Refuse
            // that undefined case; later missing nodes still copy the predecessor.
            if (!i)
            {
                hit_sphere_error = "Missing head node; no hit spheres generated";
                return false;
            }
            vNodePos[i] = vNodePos[i - 1];
            if (hit_sphere_error.empty())
                hit_sphere_error = "Previous-node fallback: ";
            else
                hit_sphere_error += ", ";
            hit_sphere_error += szNodes[i];
        }
    }
    auto distance = [](DVector a, DVector b) { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z); };
    // HEAD/NECK
    m_fHitSpheres[0] = distance(vNodePos[0], vNodePos[1]) * 0.66f;
    m_fHitSpheres[1] = m_fHitSpheres[0];

    // TORSO/PELVIS
    m_fHitSpheres[2] = distance(vNodePos[2], vNodePos[3]) * 0.66f;
    m_fHitSpheres[3] = m_fHitSpheres[2];

    // R_ARM
    m_fHitSpheres[4] = distance(vNodePos[4], vNodePos[5]) * 0.66f;
    m_fHitSpheres[5] = distance(vNodePos[5], vNodePos[6]) * 0.66f;
    m_fHitSpheres[6] = m_fHitSpheres[5];

    // L_ARM
    m_fHitSpheres[7] = distance(vNodePos[7], vNodePos[8]) * 0.66f;
    m_fHitSpheres[8] = distance(vNodePos[8], vNodePos[9]) * 0.66f;
    m_fHitSpheres[9] = m_fHitSpheres[8];

    // L_LEG
    m_fHitSpheres[10] = distance(vNodePos[10], vNodePos[11]) * 0.66f;
    m_fHitSpheres[11] = distance(vNodePos[11], vNodePos[12]) * 0.66f;
    m_fHitSpheres[12] = distance(vNodePos[12], vNodePos[13]) * 0.66f;
    m_fHitSpheres[13] = m_fHitSpheres[12];

    // R_LEG
    m_fHitSpheres[14] = distance(vNodePos[14], vNodePos[15]) * 0.66f;
    m_fHitSpheres[15] = distance(vNodePos[15], vNodePos[16]) * 0.66f;
    m_fHitSpheres[16] = distance(vNodePos[16], vNodePos[17]) * 0.66f;
    m_fHitSpheres[17] = m_fHitSpheres[16];

    hit_spheres_valid = true;
    return true;
}
bool CAnim_Sound::ForceAnimation(HOBJECT object, DDWORD animation)
{
    auto* server = BaseClass::GetServerDE();
    std::string name;
    if (!server || !server->GetModelAnimationName(object, animation, name))
        return false;
    // Preserve the source's intermediate clip zero. Requesting zero while
    // already on zero does not restart it; this is not ResetModelAnimation.
    server->SetModelAnimation(object, 0);
    server->SetModelAnimation(object, animation);
    return true;
}
std::vector<CAnim_Sound::Binding> CAnim_Sound::InspectAnimations() const
{
    return {
        {"m_nAnim_IDLE[0]", m_nAnim_IDLE[0]},
        {"m_nAnim_IDLE[1]", m_nAnim_IDLE[1]},
        {"m_nAnim_IDLE[2]", m_nAnim_IDLE[2]},
        {"m_nAnim_IDLE[3]", m_nAnim_IDLE[3]},
        {"m_nAnim_TALK[0]", m_nAnim_TALK[0]},
        {"m_nAnim_TALK[1]", m_nAnim_TALK[1]},
        {"m_nAnim_TALK[2]", m_nAnim_TALK[2]},
        {"m_nAnim_TALK[3]", m_nAnim_TALK[3]},
        {"m_nAnim_TALK[4]", m_nAnim_TALK[4]},
        {"m_nAnim_WALK[0]", m_nAnim_WALK[0]},
        {"m_nAnim_WALK[1]", m_nAnim_WALK[1]},
        {"m_nAnim_WALK[2]", m_nAnim_WALK[2]},
        {"m_nAnim_WALK[3]", m_nAnim_WALK[3]},
        {"m_nAnim_WALK[4]", m_nAnim_WALK[4]},
        {"m_nAnim_WALK[5]", m_nAnim_WALK[5]},
        {"m_nAnim_WALK[6]", m_nAnim_WALK[6]},
        {"m_nAnim_WALK[7]", m_nAnim_WALK[7]},
        {"m_nAnim_WALK[8]", m_nAnim_WALK[8]},
        {"m_nAnim_WALK_INJURED_RLEG_RIFLE", m_nAnim_WALK_INJURED_RLEG_RIFLE},
        {"m_nAnim_WALK_INJURED_RLEG_PISTOL", m_nAnim_WALK_INJURED_RLEG_PISTOL},
        {"m_nAnim_WALK_INJURED_LLEG_RIFLE", m_nAnim_WALK_INJURED_LLEG_RIFLE},
        {"m_nAnim_WALK_INJURED_LLEG_PISTOL", m_nAnim_WALK_INJURED_LLEG_PISTOL},
        {"m_nAnim_RUN[0]", m_nAnim_RUN[0]},
        {"m_nAnim_RUN[1]", m_nAnim_RUN[1]},
        {"m_nAnim_RUN[2]", m_nAnim_RUN[2]},
        {"m_nAnim_RUN[3]", m_nAnim_RUN[3]},
        {"m_nAnim_RUN[4]", m_nAnim_RUN[4]},
        {"m_nAnim_RUN[5]", m_nAnim_RUN[5]},
        {"m_nAnim_RUN[6]", m_nAnim_RUN[6]},
        {"m_nAnim_RUN[7]", m_nAnim_RUN[7]},
        {"m_nAnim_RUN[8]", m_nAnim_RUN[8]},
        {"m_nAnim_JUMP[0]", m_nAnim_JUMP[0]},
        {"m_nAnim_JUMP[1]", m_nAnim_JUMP[1]},
        {"m_nAnim_JUMP[2]", m_nAnim_JUMP[2]},
        {"m_nAnim_JUMP[3]", m_nAnim_JUMP[3]},
        {"m_nAnim_JUMP[4]", m_nAnim_JUMP[4]},
        {"m_nAnim_JUMP[5]", m_nAnim_JUMP[5]},
        {"m_nAnim_JUMP[6]", m_nAnim_JUMP[6]},
        {"m_nAnim_JUMP[7]", m_nAnim_JUMP[7]},
        {"m_nAnim_JUMP[8]", m_nAnim_JUMP[8]},
        {"m_nAnim_CROUCH[0]", m_nAnim_CROUCH[0]},
        {"m_nAnim_CROUCH[1]", m_nAnim_CROUCH[1]},
        {"m_nAnim_CROUCH[2]", m_nAnim_CROUCH[2]},
        {"m_nAnim_CROUCH[3]", m_nAnim_CROUCH[3]},
        {"m_nAnim_CROUCH[4]", m_nAnim_CROUCH[4]},
        {"m_nAnim_CROUCH[5]", m_nAnim_CROUCH[5]},
        {"m_nAnim_CROUCH[6]", m_nAnim_CROUCH[6]},
        {"m_nAnim_CROUCH[7]", m_nAnim_CROUCH[7]},
        {"m_nAnim_CROUCH[8]", m_nAnim_CROUCH[8]},
        {"m_nAnim_CRAWL[0]", m_nAnim_CRAWL[0]},
        {"m_nAnim_CRAWL[1]", m_nAnim_CRAWL[1]},
        {"m_nAnim_CRAWL[2]", m_nAnim_CRAWL[2]},
        {"m_nAnim_CRAWL[3]", m_nAnim_CRAWL[3]},
        {"m_nAnim_CRAWL[4]", m_nAnim_CRAWL[4]},
        {"m_nAnim_CRAWL[5]", m_nAnim_CRAWL[5]},
        {"m_nAnim_CRAWL[6]", m_nAnim_CRAWL[6]},
        {"m_nAnim_CRAWL[7]", m_nAnim_CRAWL[7]},
        {"m_nAnim_CRAWL[8]", m_nAnim_CRAWL[8]},
        {"m_nAnim_SWIM[0]", m_nAnim_SWIM[0]},
        {"m_nAnim_SWIM[1]", m_nAnim_SWIM[1]},
        {"m_nAnim_SWIM[2]", m_nAnim_SWIM[2]},
        {"m_nAnim_SWIM[3]", m_nAnim_SWIM[3]},
        {"m_nAnim_SWIM[4]", m_nAnim_SWIM[4]},
        {"m_nAnim_SWIM[5]", m_nAnim_SWIM[5]},
        {"m_nAnim_SWIM[6]", m_nAnim_SWIM[6]},
        {"m_nAnim_SWIM[7]", m_nAnim_SWIM[7]},
        {"m_nAnim_SWIM[8]", m_nAnim_SWIM[8]},
        {"m_nAnim_STRAFERIGHT[0]", m_nAnim_STRAFERIGHT[0]},
        {"m_nAnim_STRAFERIGHT[1]", m_nAnim_STRAFERIGHT[1]},
        {"m_nAnim_STRAFERIGHT[2]", m_nAnim_STRAFERIGHT[2]},
        {"m_nAnim_STRAFERIGHT[3]", m_nAnim_STRAFERIGHT[3]},
        {"m_nAnim_STRAFERIGHT[4]", m_nAnim_STRAFERIGHT[4]},
        {"m_nAnim_STRAFERIGHT[5]", m_nAnim_STRAFERIGHT[5]},
        {"m_nAnim_STRAFERIGHT[6]", m_nAnim_STRAFERIGHT[6]},
        {"m_nAnim_STRAFERIGHT[7]", m_nAnim_STRAFERIGHT[7]},
        {"m_nAnim_STRAFERIGHT[8]", m_nAnim_STRAFERIGHT[8]},
        {"m_nAnim_STRAFELEFT[0]", m_nAnim_STRAFELEFT[0]},
        {"m_nAnim_STRAFELEFT[1]", m_nAnim_STRAFELEFT[1]},
        {"m_nAnim_STRAFELEFT[2]", m_nAnim_STRAFELEFT[2]},
        {"m_nAnim_STRAFELEFT[3]", m_nAnim_STRAFELEFT[3]},
        {"m_nAnim_STRAFELEFT[4]", m_nAnim_STRAFELEFT[4]},
        {"m_nAnim_STRAFELEFT[5]", m_nAnim_STRAFELEFT[5]},
        {"m_nAnim_STRAFELEFT[6]", m_nAnim_STRAFELEFT[6]},
        {"m_nAnim_STRAFELEFT[7]", m_nAnim_STRAFELEFT[7]},
        {"m_nAnim_STRAFELEFT[8]", m_nAnim_STRAFELEFT[8]},
        {"m_nAnim_PICKUP_WEAPON", m_nAnim_PICKUP_WEAPON},
        {"m_nAnim_SWITCH_WEAPON[0]", m_nAnim_SWITCH_WEAPON[0]},
        {"m_nAnim_SWITCH_WEAPON[1]", m_nAnim_SWITCH_WEAPON[1]},
        {"m_nAnim_SWITCH_WEAPON[2]", m_nAnim_SWITCH_WEAPON[2]},
        {"m_nAnim_FIRE_STAND[0]", m_nAnim_FIRE_STAND[0]},
        {"m_nAnim_FIRE_STAND[1]", m_nAnim_FIRE_STAND[1]},
        {"m_nAnim_FIRE_STAND[2]", m_nAnim_FIRE_STAND[2]},
        {"m_nAnim_FIRE_STAND[3]", m_nAnim_FIRE_STAND[3]},
        {"m_nAnim_FIRE_STAND[4]", m_nAnim_FIRE_STAND[4]},
        {"m_nAnim_FIRE_STAND[5]", m_nAnim_FIRE_STAND[5]},
        {"m_nAnim_FIRE_STAND[6]", m_nAnim_FIRE_STAND[6]},
        {"m_nAnim_FIRE_STAND[7]", m_nAnim_FIRE_STAND[7]},
        {"m_nAnim_FIRE_STAND[8]", m_nAnim_FIRE_STAND[8]},
        {"m_nAnim_FIRE_WALK[0]", m_nAnim_FIRE_WALK[0]},
        {"m_nAnim_FIRE_WALK[1]", m_nAnim_FIRE_WALK[1]},
        {"m_nAnim_FIRE_WALK[2]", m_nAnim_FIRE_WALK[2]},
        {"m_nAnim_FIRE_WALK[3]", m_nAnim_FIRE_WALK[3]},
        {"m_nAnim_FIRE_WALK[4]", m_nAnim_FIRE_WALK[4]},
        {"m_nAnim_FIRE_WALK[5]", m_nAnim_FIRE_WALK[5]},
        {"m_nAnim_FIRE_WALK[6]", m_nAnim_FIRE_WALK[6]},
        {"m_nAnim_FIRE_WALK[7]", m_nAnim_FIRE_WALK[7]},
        {"m_nAnim_FIRE_WALK[8]", m_nAnim_FIRE_WALK[8]},
        {"m_nAnim_FIRE_RUN[0]", m_nAnim_FIRE_RUN[0]},
        {"m_nAnim_FIRE_RUN[1]", m_nAnim_FIRE_RUN[1]},
        {"m_nAnim_FIRE_RUN[2]", m_nAnim_FIRE_RUN[2]},
        {"m_nAnim_FIRE_RUN[3]", m_nAnim_FIRE_RUN[3]},
        {"m_nAnim_FIRE_RUN[4]", m_nAnim_FIRE_RUN[4]},
        {"m_nAnim_FIRE_RUN[5]", m_nAnim_FIRE_RUN[5]},
        {"m_nAnim_FIRE_RUN[6]", m_nAnim_FIRE_RUN[6]},
        {"m_nAnim_FIRE_RUN[7]", m_nAnim_FIRE_RUN[7]},
        {"m_nAnim_FIRE_RUN[8]", m_nAnim_FIRE_RUN[8]},
        {"m_nAnim_FIRE_JUMP[0]", m_nAnim_FIRE_JUMP[0]},
        {"m_nAnim_FIRE_JUMP[1]", m_nAnim_FIRE_JUMP[1]},
        {"m_nAnim_FIRE_JUMP[2]", m_nAnim_FIRE_JUMP[2]},
        {"m_nAnim_FIRE_JUMP[3]", m_nAnim_FIRE_JUMP[3]},
        {"m_nAnim_FIRE_JUMP[4]", m_nAnim_FIRE_JUMP[4]},
        {"m_nAnim_FIRE_JUMP[5]", m_nAnim_FIRE_JUMP[5]},
        {"m_nAnim_FIRE_JUMP[6]", m_nAnim_FIRE_JUMP[6]},
        {"m_nAnim_FIRE_JUMP[7]", m_nAnim_FIRE_JUMP[7]},
        {"m_nAnim_FIRE_JUMP[8]", m_nAnim_FIRE_JUMP[8]},
        {"m_nAnim_FIRE_CROUCH[0]", m_nAnim_FIRE_CROUCH[0]},
        {"m_nAnim_FIRE_CROUCH[1]", m_nAnim_FIRE_CROUCH[1]},
        {"m_nAnim_FIRE_CROUCH[2]", m_nAnim_FIRE_CROUCH[2]},
        {"m_nAnim_FIRE_CROUCH[3]", m_nAnim_FIRE_CROUCH[3]},
        {"m_nAnim_FIRE_CROUCH[4]", m_nAnim_FIRE_CROUCH[4]},
        {"m_nAnim_FIRE_CROUCH[5]", m_nAnim_FIRE_CROUCH[5]},
        {"m_nAnim_FIRE_CROUCH[6]", m_nAnim_FIRE_CROUCH[6]},
        {"m_nAnim_FIRE_CROUCH[7]", m_nAnim_FIRE_CROUCH[7]},
        {"m_nAnim_FIRE_CROUCH[8]", m_nAnim_FIRE_CROUCH[8]},
        {"m_nAnim_FIRE_CRAWL[0]", m_nAnim_FIRE_CRAWL[0]},
        {"m_nAnim_FIRE_CRAWL[1]", m_nAnim_FIRE_CRAWL[1]},
        {"m_nAnim_FIRE_CRAWL[2]", m_nAnim_FIRE_CRAWL[2]},
        {"m_nAnim_FIRE_CRAWL[3]", m_nAnim_FIRE_CRAWL[3]},
        {"m_nAnim_FIRE_CRAWL[4]", m_nAnim_FIRE_CRAWL[4]},
        {"m_nAnim_FIRE_CRAWL[5]", m_nAnim_FIRE_CRAWL[5]},
        {"m_nAnim_FIRE_CRAWL[6]", m_nAnim_FIRE_CRAWL[6]},
        {"m_nAnim_FIRE_CRAWL[7]", m_nAnim_FIRE_CRAWL[7]},
        {"m_nAnim_FIRE_CRAWL[8]", m_nAnim_FIRE_CRAWL[8]},
        {"m_nAnim_FALLING", m_nAnim_FALLING},
        {"m_nAnim_FALLING_UNCONTROL", m_nAnim_FALLING_UNCONTROL},
        {"m_nAnim_ROLL_FORWARD", m_nAnim_ROLL_FORWARD},
        {"m_nAnim_ROLL_RIGHT", m_nAnim_ROLL_RIGHT},
        {"m_nAnim_ROLL_LEFT", m_nAnim_ROLL_LEFT},
        {"m_nAnim_ROLL_BACK", m_nAnim_ROLL_BACK},
        {"m_nAnim_HANDSPRING_FORWARD", m_nAnim_HANDSPRING_FORWARD},
        {"m_nAnim_HANDSPRING_RIGHT", m_nAnim_HANDSPRING_RIGHT},
        {"m_nAnim_HANDSPRING_LEFT", m_nAnim_HANDSPRING_LEFT},
        {"m_nAnim_HANDSPRING_BACK", m_nAnim_HANDSPRING_BACK},
        {"m_nAnim_FLIP_FORWARD", m_nAnim_FLIP_FORWARD},
        {"m_nAnim_FLIP_RIGHT", m_nAnim_FLIP_RIGHT},
        {"m_nAnim_FLIP_LEFT", m_nAnim_FLIP_LEFT},
        {"m_nAnim_FLIP_BACK", m_nAnim_FLIP_BACK},
        {"m_nAnim_DODGE_RIGHT", m_nAnim_DODGE_RIGHT},
        {"m_nAnim_DODGE_LEFT", m_nAnim_DODGE_LEFT},
        {"m_nAnim_RECOIL[0]", m_nAnim_RECOIL[0]},
        {"m_nAnim_RECOIL[1]", m_nAnim_RECOIL[1]},
        {"m_nAnim_RECOIL[2]", m_nAnim_RECOIL[2]},
        {"m_nAnim_RECOIL[3]", m_nAnim_RECOIL[3]},
        {"m_nAnim_RECOIL[4]", m_nAnim_RECOIL[4]},
        {"m_nAnim_RECOIL[5]", m_nAnim_RECOIL[5]},
        {"m_nAnim_RECOIL[6]", m_nAnim_RECOIL[6]},
        {"m_nAnim_RECOIL[7]", m_nAnim_RECOIL[7]},
        {"m_nAnim_RECOIL[8]", m_nAnim_RECOIL[8]},
        {"m_nAnim_RECOIL[9]", m_nAnim_RECOIL[9]},
        {"m_nAnim_RECOIL[10]", m_nAnim_RECOIL[10]},
        {"m_nAnim_RECOIL[11]", m_nAnim_RECOIL[11]},
        {"m_nAnim_TAUNT[0]", m_nAnim_TAUNT[0]},
        {"m_nAnim_TAUNT[1]", m_nAnim_TAUNT[1]},
        {"m_nAnim_TAUNT[2]", m_nAnim_TAUNT[2]},
        {"m_nAnim_TAUNT[3]", m_nAnim_TAUNT[3]},
        {"m_nAnim_TAUNT[4]", m_nAnim_TAUNT[4]},
        {"m_nAnim_TAUNT[5]", m_nAnim_TAUNT[5]},
        {"m_nAnim_TAUNT[6]", m_nAnim_TAUNT[6]},
        {"m_nAnim_SPOTPLAYER_RIGHT", m_nAnim_SPOTPLAYER_RIGHT},
        {"m_nAnim_SPOTPLAYER_LEFT", m_nAnim_SPOTPLAYER_LEFT},
        {"m_nAnim_SPOTPLAYER_POINT", m_nAnim_SPOTPLAYER_POINT},
        {"m_nAnim_HUMILIATION[0]", m_nAnim_HUMILIATION[0]},
        {"m_nAnim_HUMILIATION[1]", m_nAnim_HUMILIATION[1]},
        {"m_nAnim_HUMILIATION[2]", m_nAnim_HUMILIATION[2]},
        {"m_nAnim_HUMILIATION[3]", m_nAnim_HUMILIATION[3]},
        {"m_nAnim_HUMILIATION[4]", m_nAnim_HUMILIATION[4]},
        {"m_nAnim_HUMILIATION[5]", m_nAnim_HUMILIATION[5]},
        {"m_nAnim_HUMILIATION[6]", m_nAnim_HUMILIATION[6]},
        {"m_nAnim_HUMILIATION[7]", m_nAnim_HUMILIATION[7]},
        {"m_nAnim_HUMILIATION[8]", m_nAnim_HUMILIATION[8]},
        {"m_nAnim_HUMILIATION[9]", m_nAnim_HUMILIATION[9]},
        {"m_nAnim_DEATH[0]", m_nAnim_DEATH[0]},
        {"m_nAnim_DEATH[1]", m_nAnim_DEATH[1]},
        {"m_nAnim_DEATH[2]", m_nAnim_DEATH[2]},
        {"m_nAnim_DEATH[3]", m_nAnim_DEATH[3]},
        {"m_nAnim_DEATH[4]", m_nAnim_DEATH[4]},
        {"m_nAnim_DEATH[5]", m_nAnim_DEATH[5]},
        {"m_nAnim_DEATH[6]", m_nAnim_DEATH[6]},
        {"m_nAnim_DEATH[7]", m_nAnim_DEATH[7]},
        {"m_nAnim_DEATH[8]", m_nAnim_DEATH[8]},
        {"m_nAnim_DEATH[9]", m_nAnim_DEATH[9]},
        {"m_nAnim_DEATH[10]", m_nAnim_DEATH[10]},
        {"m_nAnim_DEATH[11]", m_nAnim_DEATH[11]},
        {"m_nAnim_CORPSE[0]", m_nAnim_CORPSE[0]},
        {"m_nAnim_CORPSE[1]", m_nAnim_CORPSE[1]},
        {"m_nAnim_CORPSE[2]", m_nAnim_CORPSE[2]},
        {"m_nAnim_CORPSE[3]", m_nAnim_CORPSE[3]},
        {"m_nAnim_CORPSE[4]", m_nAnim_CORPSE[4]},
        {"m_nAnim_CORPSE[5]", m_nAnim_CORPSE[5]},
        {"m_nAnim_CORPSE[6]", m_nAnim_CORPSE[6]},
        {"m_nAnim_CORPSE[7]", m_nAnim_CORPSE[7]},
        {"m_nAnim_CORPSE[8]", m_nAnim_CORPSE[8]},
        {"m_nAnim_CORPSE[9]", m_nAnim_CORPSE[9]},
        {"m_nAnim_CORPSE[10]", m_nAnim_CORPSE[10]},
        {"m_nAnim_CORPSE[11]", m_nAnim_CORPSE[11]},
        {"m_nAnim_LIMB[0]", m_nAnim_LIMB[0]},
        {"m_nAnim_LIMB[1]", m_nAnim_LIMB[1]},
        {"m_nAnim_LIMB[2]", m_nAnim_LIMB[2]},
        {"m_nAnim_SPECIAL[0]", m_nAnim_SPECIAL[0]},
        {"m_nAnim_SPECIAL[1]", m_nAnim_SPECIAL[1]},
        {"m_nAnim_SPECIAL[2]", m_nAnim_SPECIAL[2]},
        {"m_nAnim_SPECIAL[3]", m_nAnim_SPECIAL[3]},
        {"m_nAnim_SPECIAL[4]", m_nAnim_SPECIAL[4]},
        {"m_nAnim_SPECIAL[5]", m_nAnim_SPECIAL[5]},
        {"m_nAnim_SPECIAL[6]", m_nAnim_SPECIAL[6]},
        {"m_nAnim_SPECIAL[7]", m_nAnim_SPECIAL[7]},
        {"m_nAnim_SPECIAL[8]", m_nAnim_SPECIAL[8]},
    };
}
