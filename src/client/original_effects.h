#pragma once
#include "client/effect_runtime.h"
#include "client/line_effects.h"
#include "game/blood_splats.h"
#include "game/blood_spurts.h"
#include "game/explosion_flames.h"
#include "game/explosion_fragments.h"
#include "game/explosion_lights.h"
#include "game/explosion_models.h"
#include "game/explosion_rings.h"
#include "game/explosion_sprites.h"
#include "game/flesh_gibs.h"
#include "game/laser_beams.h"
#include "game/lightning.h"
#include "game/object_particles.h"
#include "game/original/LightFX.h"
#include "game/particle_streams.h"
#include "game/polygrids.h"
#include "game/shotgun_effects.h"
#include "game/sparks.h"
#include "game/warp_gates.h"
#include "game/weapon_sprites.h"
namespace OpenBlood2
{
    class OriginalEffects final : public EffectRuntime
    {
      public:
        void begin_world(std::uint64_t epoch) override
        {
            world = epoch;
            reset_world();
        }
        void reset_world() override;
        bool advance_lights(float time, const ClientWorld&);
        std::uint64_t world = 0;
        ExplosionSprites explosion_sprites;
        Sparks sparks;
        LightningBolts lightning;
        WarpGates warp_gates;
        BloodSpurts blood_spurts;
        BloodSplats blood_splats;
        ShotgunEffects shotgun_effects;
        WeaponSprites weapon_sprites;
        ExplosionModels explosion_models;
        LaserBeams laser_beams;
        ObjectParticles object_particles;
        PolyGrids polygrids;
        ParticleStreams particle_streams;
        ExplosionLights explosion_lights;
        ExplosionRings explosion_rings;
        ExplosionFlames explosion_flames;
        FleshGibs flesh_gibs, destruction_debris;
        ExplosionFragments explosion_fragments;
        std::vector<CLightFX> light_effects;
        std::vector<DetachedEffectLine> detached_lines;
        std::uint64_t next_line_id = 1;
    };
} // namespace OpenBlood2
