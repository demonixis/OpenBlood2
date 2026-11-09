#pragma once
#include "Destructable.h"
#include "cpp_engineobjects_de.h"
#include <array>

// Original ObjectDLL LightFX. The server advances its three independent authored cycles.
class LightFX final : public BaseClass
{
  public:
    LightFX();
    ~LightFX() override;
    bool SetConstant(DVector color, float radius);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    struct Cycle
    {
        enum Stage
        {
            Init,
            Minimum,
            RampUp,
            Maximum,
            RampDown
        };
        float low, high, value;
        float hold_min = 0, hold_max = 0, up = 0, down = 0, at = 0;
        int remaining = 0;
        Stage stage = Init;
        int Advance(float now, bool intensity);
    };
    bool ReadProperties();
    bool Publish();
    void SetOn(bool);
    bool Tick();

    CDestructable damage;
    bool on = true;
    float lifetime = 0, start = 0, hit_points = 1, color_time = 0;
    std::array<DVector, 3> colors{{{255, 255, 255}, {255, 255, 255}, {255, 255, 255}}};
    std::array<float, 3> color_times{};
    int color_cycles = 0;
    unsigned color_stage = 0;
    DVector color{1, 1, 1};
    Cycle intensity{.5f, 1, 1}, radius{500, 0, 500};
    std::string ramp_up, ramp_down;
};
