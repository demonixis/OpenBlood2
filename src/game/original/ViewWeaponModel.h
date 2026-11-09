#pragma once
#include "cpp_engineobjects_de.h"
#include "engine/audio.h"
#include "game/weapon_messages.h"
#include <optional>

class CPlayerObj;

class CViewWeaponModel final : public BaseClass
{
  public:
    CViewWeaponModel() : BaseClass(OT_MODEL) {}
    ~CViewWeaponModel() override;
    bool Init(HOBJECT owner, bool left);
    void UseModelKeys(DBOOL enabled);
    void SetVisible(DBOOL visible);
    bool QueueFire(const OpenBlood2::WeaponFire&);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    CPlayerObj* CurrentOwner() const;
    void StopLoop();
    void PlayKeySound(std::string_view filename, bool loop);
    void OnStringKey(const ArgList&);

    HOBJECT owner_ = nullptr;
    bool left_ = false;
    DBYTE weapon_ = 0;
    OpenBlood2::SoundId loop_ = 0;
    std::optional<OpenBlood2::WeaponFire> pending_fire_;
};
