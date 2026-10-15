#pragma once
#include "Weapon.h"
#include "engine/model_store.h"
#include <random>
class ServerDE;
class CBloodServerShell;
namespace blood2 { struct WeaponInput { bool primary=false,alternate=false,left=false; }; }

// Selected CViewWeapon client animation/firing path, backed by an owned ABC
// track instead of a legacy local renderer object. No server HOBJECT is forged.
class CViewWeapon {
public:
    bool Create(DBYTE,bool,blood2::ModelStore&,blood2::Resources&,std::string&);
    bool Update(ServerDE&,CBloodServerShell&,HCLIENT,const blood2::WeaponInput&,DVector,DVector,double,std::string&);
    DBYTE type=0;
    bool left=false;
    std::shared_ptr<const blood2::Model> model;
    blood2::AnimationTrack animation;
    WeaponState state=WS_HOLSTERED;
    std::string last_key, pending_sound;
    bool sound_indexed=false; // Request only; audio output is not ported.
    std::uint64_t fire_keys=0,shots=0;
    FireResult last_result=FireResult::Invalid;
private:
    bool PlayAnimation(std::uint32_t);
    std::uint32_t rest_=UINT32_MAX,idle_=UINT32_MAX,draw_=UINT32_MAX,holster_=UINT32_MAX;
    std::array<std::uint32_t,2> start_{UINT32_MAX,UINT32_MAX},fire_{UINT32_MAX,UINT32_MAX},stop_{UINT32_MAX,UINT32_MAX};
    float last_shot_time_=0,idle_start_=0,idle_delay_=20;
    bool last_alt_=false,last_firing_=false;
    DBYTE next_seed_=2;
    std::minstd_rand random_{1};
    blood2::Resources* resources_=nullptr;
};
