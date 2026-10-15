#pragma once
#include "B2BaseClass.h"
#include "game/mover_messages.h"
#include <string>

// Selected original Door.cpp/Door.h rules, with typed translation results.
// Audio, portals and save restoration remain pending.
class Door : public B2BaseClass
{
  public:
    enum State : unsigned
    {
        Closed = 1,
        Closing = 2,
        Open = 3,
        Opening = 4
    };
    Door() : B2BaseClass(OT_WORLDMODEL) {}
    ~Door() override;
    HOBJECT trigger = nullptr;
    State state = Closed;
    float speed = 0, closing_speed = 0, move_dist = 0, wait_time = 0, open_delay = .01f, open_time = 0;
    DVector move_dir{}, closed_pos{}, open_pos{}, trigger_dims{};
    unsigned waveform = 0;
    bool box_physics = true, trigger_closed = false, start_open = false, remains_open = false;
    bool self_trigger = false, fire_through = false, portal_open = false, started_moving = false;
    bool touch_activate = false, player_activate = true, ai_activate = true, object_activate = false;
    bool relay_activate = true, named_activate = false, locked = false;
    std::string open_busy, open_stop, close_busy, close_stop, portal, activation_name;
    std::string locked_sound, unlocked_sound, key_name, last_sound;
    float sound_radius = 1000;
    bool sound_loop = false;
    std::uint64_t triggers = 0, sound_requests = 0;
    OpenBlood2::MoverResult last_move = OpenBlood2::MoverResult::Moved;
    static float GetWaveValue(float speed, float percent, unsigned wave);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  protected:
    bool ReadProp(ObjectCreateStruct&);
    bool SpawnTrigger();
    void SetTriggerActive(bool);
    void SetOpen(bool sound = true);
    void SetClosed(bool sound = true);
    void SetOpening();
    void SetClosing();
    virtual void Move(bool opening);
    void StartSound(const std::string&, bool loop);
};
