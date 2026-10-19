#pragma once
#include "Projectile.h"
class CBugSprayProjectile : public CProjectile
{
  public:
    explicit CBugSprayProjectile(bool alternate = false);
    bool AdvanceFlight(double) override;
    std::uint64_t trail = 0;
    OpenBlood2::SceneExplosionReport effect;

  protected:
    bool InitialUpdate() override;
    void AddExplosion(DVector, DVector) override;
    bool alternate_ = false;
};
class CBugSprayAltProjectile final : public CBugSprayProjectile
{
  public:
    CBugSprayAltProjectile() : CBugSprayProjectile(true) {}
};
