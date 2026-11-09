#pragma once
#include "B2BaseClass.h"
#include "engine/audio.h"

class SoccerBall final : public B2BaseClass
{
  public:
    SoccerBall() : B2BaseClass(OT_MODEL)
    {
    }
    ~SoccerBall() override;
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

    HOBJECT last_player = nullptr;
    float expires = 0;
    bool scored = false;
    std::string sound_error;

  private:
    void Update();
    void Touch(HOBJECT, float force);
    bool initialized = false, standing = false, bounced = false;
    float radius = 1;
    DVector last_position{}, last_normal{0, 1, 0};
};

class SoccerGoal final : public B2BaseClass
{
  public:
    SoccerGoal() : B2BaseClass(OT_WORLDMODEL)
    {
    }
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    void ReplaceLostBall();

    int team = 1;
    unsigned scores = 0;
    std::string sound_error, effect_error, score_error;

  private:
    bool ReadProperties(ObjectCreateStruct&, float reason);
    void Touch(HOBJECT);
    void SendScoreTrigger();
    void Update();
    unsigned replacements = 0;
    bool wait_one_frame = false, directional = false, box_physics = true;
    DVector direction{0, 0, 1};
    float sound_radius = 1000;
    std::string sound, score_target, score_message;
};
