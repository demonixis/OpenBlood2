#pragma once
#include "B2BaseClass.h"
#include "PolyGridFX.h"
#include "game/container_messages.h"
#include "game/weapon_messages.h"
#include <string>

// Selective VolumeBrush.cpp / VolumeBrushTypes.cpp source port; see container_contracts.md.
class VolumeBrush : public B2BaseClass
{
  public:
    VolumeBrush();
    OpenBlood2::ContainerInfo info;
    float damage = 0, fog_near = -100, fog_far = 300, surface_height = 2, surface_alpha = .7f;
    OpenBlood2::DamageType damage_type = OpenBlood2::DamageType::Normal;
    DVector fog_color{};
    bool fog = false, show_surface = true, locked = false, remove_key = false;
    std::string sprite, unlock_key;
    OpenBlood2::PolyGridSettings surface;
    HOBJECT surface_object = nullptr; // Independent runtime object, as in original VolumeBrush.
    std::string surface_error;
    std::uint64_t physics_calls = 0, damage_events = 0, triggers = 0;
    virtual void UpdatePhysics(OpenBlood2::ContainerPhysics&);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp(ObjectCreateStruct&);
    void CreateSurface();
    float last_damage_time_ = 0;
    DDWORD saved_flags_ = 0;
};
class Water : public VolumeBrush
{
};
class Blood : public VolumeBrush
{
};
class Acid : public VolumeBrush
{
};
class Ladder : public VolumeBrush
{
  public:
    // Original override marks characters on ladders; their local MoveMgr owns
    // the resulting motion. It does not apply generic VolumeBrush damping/damage.
    void UpdatePhysics(OpenBlood2::ContainerPhysics&) override;
};
class Conveyor : public VolumeBrush
{
  public:
    Conveyor()
    {
        info.code = CC_CONVEYOR;
    }
    void UpdatePhysics(OpenBlood2::ContainerPhysics&) override;
};
class Damage : public VolumeBrush
{
  public:
    Damage()
    {
        info.code = CC_DAMAGE;
    }
};
class FreeFall : public VolumeBrush
{
  public:
    FreeFall()
    {
        info.code = CC_FREEFALL;
        info.gravity = 0;
    }
};
