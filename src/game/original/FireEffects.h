#pragma once
#include "B2BaseClass.h"
#include <array>

class Explosion final : public B2BaseClass
{
  public:
    ~Explosion() override;
    bool Setup(const char* sound, float sound_radius, float duration, const char* skin, float damage_radius,
               float max_damage, float min_scale, float max_scale, bool mark = false, bool sparks = false,
               bool smoke = false);
    bool SetupLight(bool enabled, DVector color, float min_radius, float max_radius);
    bool SetupShockwave(const char* sprite, DVector minimum, DVector maximum, float duration);
    bool Explode(float delay = 0);
    HOBJECT model = nullptr, light = nullptr, shockwave = nullptr;
    OpenBlood2::SoundId voice = 0;
    std::string sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProperties();
    bool Tick();
    void CreateExplosion(DVector position);
    void CreateShockwave(DVector position);
    std::string model_file = "Models\\Explosions\\Explosion_3.abc";
    std::string skin_file = "Skins\\Explosions\\Explosion_1.dtx", sound_file, shockwave_file;
    float sound_radius = 1000, duration = 1.5f, min_scale = .1f, max_scale = 1, delay = 0, start = 0;
    float shockwave_duration = 0, light_min = 100, light_max = 300;
    DVector rotation_period{}, shockwave_min{.1f, .1f, 1}, shockwave_max{1, 1, 1}, light_color{1, .5f, 0};
    bool first = true, create_light = true, create_shockwave = true, create_mark = false;
};

class FireFX final : public B2BaseClass
{
  public:
    ~FireFX() override;
    bool Setup(HOBJECT target, float burn_time);
    HOBJECT target = nullptr, light = nullptr;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    struct Flame
    {
        HOBJECT sprite = nullptr;
        DVector position{}, velocity{};
        float start = 0, duration = 0;
    };
    bool BindTarget();
    bool Tick();
    void RemoveFlames();
    bool AddFlame(Flame&, DVector position, float now);
    std::array<Flame, 20> flames_{};
    std::string target_name_;
    unsigned next_flame_ = 0;
    bool first_ = true, dead_ = false;
    float death_time_ = 0;
};
