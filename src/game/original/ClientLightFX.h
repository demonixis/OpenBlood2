#pragma once
#include "ClientSFX.h"
#include "Destructable.h"
#include "game/light_messages.h"
class ClientLightFX : public CClientSFX
{
  public:
    ClientLightFX();
    ~ClientLightFX() override;
    bool on = true, dynamic = true;
    float lifetime = -1, start_time = 0, initial_delay = 0, hit_points = 1;
    OpenBlood2::LightSettings settings;
    CDestructable damage;
    std::string last_command;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    bool InitialUpdate();
};
class SquareWaveLightFX : public ClientLightFX
{
};
class SawWaveLightFX : public ClientLightFX
{
};
class RampUpWaveLightFX : public ClientLightFX
{
};
class RampDownWaveLightFX : public ClientLightFX
{
};
class SineWaveLightFX : public ClientLightFX
{
};
class Flicker1WaveLightFX : public ClientLightFX
{
};
class Flicker2WaveLightFX : public ClientLightFX
{
};
class Flicker3WaveLightFX : public ClientLightFX
{
};
class Flicker4WaveLightFX : public ClientLightFX
{
};
class StrobeWaveLightFX : public ClientLightFX
{
};
class SearchWaveLightFX : public ClientLightFX
{
};
class FlickerLight : public ClientLightFX
{
};
class GlowingLight : public ClientLightFX
{
};
