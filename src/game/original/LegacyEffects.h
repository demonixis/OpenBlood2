#pragma once
#include "ClientSFX.h"
#include "SparksFX.h"
#include "client/line_effects.h"
#include "game/ballistic_settings.h"
#include "game/brush_messages.h"
#include <array>

class CClientLaserBeamSFX final : public CClientSFX
{
  public:
    DVector source{}, destination{};
    std::uint64_t effect = 0;
    OpenBlood2::RadiusDamageReport targets;
    std::string error, sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool Valid() const;
    void Fire(HOBJECT);
    DBYTE type = 0;
    float damage = 100, radius = 100, sound_radius = 500;
    std::string sound = "Sounds\\Thunder.wav";
};

class TransientClientSFX : public CClientSFX
{
  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};

class CClientSparksSFX final : public TransientClientSFX
{
  public:
    std::uint64_t Setup(OpenBlood2::SparkSettings, std::string& error);
};

class CBulletImpactSFX final : public TransientClientSFX
{
  public:
    std::uint64_t Setup(DVector normal, float scale, std::string sprite, std::string& error);
};

class CClientMarkSFX final : public CClientSFX
{
  public:
    ~CClientMarkSFX() override;
    bool Setup(DVector normal, DBYTE ammo, SurfaceType, HOBJECT link);
    std::uint64_t effect = 0;
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    HOBJECT target = nullptr;
    bool owns_effect = false;
};

class CClientGibFX final : public TransientClientSFX
{
  public:
    bool Setup(OpenBlood2::DebrisEffect);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};

class CClientBloodTrail final : public CClientSFX
{
  public:
    bool Setup(HOBJECT parent);
    unsigned splats = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    HOBJECT parent_ = nullptr;
};

class CClientTracer final : public TransientClientSFX
{
  public:
    bool Setup(DVector destination, HOBJECT gun);
};

class CClientLaserFX final : public CClientSFX
{
  public:
    bool Setup(HOBJECT gun);
    std::array<OpenBlood2::LineEffectGeometry, 5> lines;
    bool visible = false;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    HOBJECT gun_ = nullptr;
    void Update();
};

class CShellCasing final : public BaseClass
{
  public:
    CShellCasing() : BaseClass(OT_MODEL) {}
    bool Setup(HOBJECT shooter, bool left);
    unsigned bounces = 2;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    void Bounce(HOBJECT);
    bool Update();
    bool RandomSpin();
    HOBJECT shooter_ = nullptr;
    float pitch = 0, yaw = 0, pitch_speed = 0, yaw_speed = 0, expires = 0;
    bool first = true, configured = false;
};

class CModelObject final : public BaseClass
{
  public:
    CModelObject() : BaseClass(OT_MODEL) {}
    bool Setup(float life, DVector periods, bool stop_on_ground, bool random_rotation);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    bool Tick();
    DVector angles{}, speed{};
    float lifetime = 0, start = 0, last_update = 0;
    bool first = true, rotating = false, stop_rotation = true;
};

class CImpact final : public BaseClass
{
  public:
    CImpact() : BaseClass(OT_SPRITE) {}
    bool Setup(DVector minimum, DVector maximum, float lifetime, bool rotateable, bool fade);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    bool Tick();
    DVector low{}, high{}, color{1, 1, 1};
    float lifetime = .3f, start = 0, alpha = 1;
    bool first = true, scale_changes = true, fade = false;
};

class CImpactRicochet final : public BaseClass
{
  public:
    CImpactRicochet() : BaseClass(OT_SPRITE) {}

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    float start = 0;
};
