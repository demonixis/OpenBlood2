#pragma once
#include "cpp_aggregate_de.h"
#include "game/mover_messages.h"
#include <string>
class Rotating : public Aggregate
{
  public:
    enum State
    {
        Off,
        Normal,
        SpinUp,
        SpinDown
    };
    State state = Off;
    HOBJECT object = nullptr;
    DVector velocity{}, normal_velocity{}, sign{1, 1, 1}, spin_up{}, spin_down{}, time_left{};
    DVector angles{}, pose_angles{};
    float last_time = 0, start_time = 0, sound_radius = 1000;
    std::string spin_up_sound, busy_sound, spin_down_sound, last_sound;
    bool sound_loop = false, valid = true;
    unsigned sound_requests = 0, commands = 0;
    OpenBlood2::MoverResult last_move = OpenBlood2::MoverResult::Moved;
    void Init(HOBJECT);
    void SetNormalRotation();
    void SetSpinUp();
    void SetSpinDown();
    void SetOff();

  protected:
    DDWORD EngineMessageFn(LPBASECLASS, DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(LPBASECLASS, HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    void ReadProp();
    void UpdateNormalRotation();
    void UpdateSpin(bool up);
    void StartSound(const std::string&, bool loop);
};
