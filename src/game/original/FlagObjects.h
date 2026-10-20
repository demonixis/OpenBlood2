#pragma once
#include "B2BaseClass.h"
#include <array>
#include <string>
class FlagObject;
// Original static flag pointers and match counters, exchanged with the owning world.
struct CTFWorldState
{
    std::array<FlagObject*, 2> flags{};
    std::array<int, 2> team_scores{};
    float last_need_flag = 0;
    int flag_value = 5;
    bool use_team_size = false;
};
extern CTFWorldState g_ctf;
class FlagObject : public B2BaseClass
{
  public:
    FlagObject() : B2BaseClass(OT_MODEL) {}
    ~FlagObject() override;
    int team = 0;
    DVector stand_position{};
    DRotation stand_rotation{};
    bool in_stand = false, on_ground = false, with_player = false;
    HOBJECT carrier = nullptr;
    std::string error, sound_error;
    OpenBlood2::SoundId voice = 0;
    std::uint64_t grabs = 0, returns = 0, drops = 0;
    bool IsOtherFlagInStand() const;
    void GiveToPlayer(HOBJECT);
    bool ReturnToFlagStand();
    bool DropToGround();
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    HSTRING grab_string = nullptr;
};
class FlagStand : public B2BaseClass
{
  public:
    FlagStand() : B2BaseClass(OT_MODEL) {}
    ~FlagStand() override;
    int team = 0;
    HOBJECT flag = nullptr;
    std::string error;
    bool SpawnFlag();
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    HSTRING give_string = nullptr;
};
