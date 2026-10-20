#pragma once
#include "engine/model.h"
#include "engine/particles.h"
#include "engine/sprite.h"
#include <array>
#include <random>
namespace OpenBlood2
{
    struct WarpSpriteSettings
    {
        float min_scale = .1f, max_scale = 1, alpha = .75f;
        unsigned ramp_up = 1, ramp_down = 2;
        bool align = false;
        std::string resource = "Sprites\\rift.spr";
    };
    struct WarpParticleSettings
    {
        float radius = 5000, position_radius = 100, min_velocity = 10, max_velocity = 25;
        float alpha = .5f, min_life = 4, max_life = 5, delay = .25f, gravity = 15;
        DVector offset{}, rotations{}, min_color{50, 50, 50}, max_color{255, 255, 255};
        unsigned count = 10, emit = 1, ramp_up = 1, ramp_down = 1;
        bool align = false;
        std::string resource = "SpriteTextures\\smoke64_3.dtx";
    };
    struct WarpGateSettings
    {
        float ramp_up = 5, ramp_down = 5;
        WarpSpriteSettings sprite;
        std::array<WarpParticleSettings, 2> particles;
        WarpGateSettings()
        {
            particles[1].resource = "SpriteTextures\\lensflare_1.dtx";
        }
    };
    bool valid_warp_gate(const WarpGateSettings&, std::string&);
} // namespace OpenBlood2
class CWarpGateSpriteFX
{
  public:
    OpenBlood2::WarpSpriteSettings settings;
    std::shared_ptr<const OpenBlood2::Sprite> sprite;
    OpenBlood2::SpriteTrack track;
    OpenBlood2::NodePose pose;
    float up = 5, down = 5, scale = 1, alpha = 1;
    double trigger_time = 0, last_time = 0, next_update = 0;
    bool first = true, started = false;
    std::string error;
    bool CreateObject(const OpenBlood2::WarpGateSettings&, OpenBlood2::SpriteStore&, OpenBlood2::NodePose, double);
    bool Update(double, OpenBlood2::NodePose, bool on);
};
class CWarpGateParticleFX
{
  public:
    OpenBlood2::WarpParticleSettings settings;
    std::shared_ptr<const OpenBlood2::Sprite> sprite;
    OpenBlood2::SpriteTrack track;
    std::string texture, error;
    OpenBlood2::NodePose pose;
    std::vector<OpenBlood2::Particle> particles;
    float up = 5, down = 5, alpha = 1;
    double trigger_time = 0, last_time = 0, last_add = 0, next_update = 0;
    bool first = true, started = false;
    std::uint64_t emitted = 0, bursts = 0;
    bool CreateObject(const OpenBlood2::WarpParticleSettings&, float, float, OpenBlood2::SpriteStore&,
                      const OpenBlood2::Resources&, OpenBlood2::NodePose, double);
    bool Update(double, OpenBlood2::NodePose, bool, std::minstd_rand&, std::size_t budget);

  private:
    bool AddParticles(std::minstd_rand&, std::size_t);
};
