#pragma once
#include "client/world.h"
#include "engine/container.h"
#include "engine/particles.h"
#include "engine/scene_effects.h"
#include <variant>
namespace OpenBlood2
{
    enum class EffectKind : std::uint8_t
    {
        ExplosionSprite,
        WarpSprite,
        WarpParticles,
        Lightning,
        Sparks,
        BloodSpurt,
        BloodSplat,
        BallisticSprite,
        BallisticSplash,
        WeaponFlash,
        WeaponSmoke,
        ExplosionRing,
        ExplosionTrail,
        ExplosionFlame,
        ObjectParticles,
        ParticleStream,
        ExplosionModel,
        ExplosionFragment,
        FleshGib,
        DestructionDebris,
        Laser,
        Casing,
        PolyGrid,
        Tracer
    };
    struct EffectId
    {
        EffectKind kind = EffectKind::ExplosionSprite;
        std::uint64_t value = 0;
        std::uint32_t part = 0;
        auto operator<=>(const EffectId&) const = default;
    };
    struct EffectSprite
    {
        std::shared_ptr<const Sprite> sprite;
        SpriteTrack track;
        NodePose pose;
        DVector scale{1, 1, 1}, color{1, 1, 1};
        float alpha = 1;
        DDWORD flags = FLAG_VISIBLE;
        bool camera_half_turn = false;
        std::vector<DVector> clip_polygon;
    };
    struct EffectParticles
    {
        std::string texture;
        std::vector<Particle> particles; // Copied values; no emitter, callback or random generator.
        DVector origin{};
        float half_extent = 1, alpha = 1;
        bool batched = false; // One whole-system draw for the existing blood/splash path.
    };
    struct EffectBillboard
    {
        EffectId id;
        std::variant<EffectSprite, EffectParticles> value;
    };
    struct EffectModel
    {
        EffectId id;
        std::shared_ptr<const Model> model;
        AnimationTrack animation;
        std::string skin, label;
        NodePose pose;
        DVector scale{1, 1, 1};
        float alpha = 1;
        bool visible = true, require_skin = true, translucent = false;
    };
    struct EffectPolyGrid
    {
        EffectId id{EffectKind::PolyGrid};
        ObjectId owner;
        NodePose pose;
        DVector dimensions;
        unsigned width = 0;
        std::vector<std::int8_t> heights;
        std::array<DVector, 256> palette;
        std::array<float, 2> scale{}, pan{};
        float alpha = 1;
        std::string texture, error;
        bool visible = false;
    };
    struct EffectTracer
    {
        EffectId id{EffectKind::Tracer};
        DVector center{}, forward{}, right{};
    };
    struct EffectFog
    {
        bool enabled = false;
        float near = 0, far = 0;
        DVector color{};
    };
    struct EffectFogVolume
    {
        ObjectId owner;
        std::shared_ptr<const ContainerShape> shape; // Client-owned deep copy of posed geometry.
        EffectFog fog;
        bool visible = false;
    };
    struct ClientEffectsFrame
    {
        std::uint64_t world = 0, sequence = 0;
        double time = 0;
        std::vector<EffectBillboard> billboards;
        std::vector<EffectModel> models;
        std::vector<EffectPolyGrid> polygrids;
        std::vector<EffectTracer> tracers;
        std::vector<SceneEffectLight> lights;
        std::size_t fog_lights_unsupported = 0, shadow_lights_unshadowed = 0;
        EffectFog fog;
        // World FarZ (truncated; 0 when unset or not positive) and the truncated SkyFog
        // SkyFogNearZ/FarZ interval (`sky_fog` only when SkyFog is set and far > near).
        float far_z = 0;
        bool sky_fog = false;
        float sky_fog_near = 0, sky_fog_far = 0;
        PointLight flashlight; // Dedicated light path, separate from ordinary effect-light budgets.
        std::vector<EffectFogVolume> volumes;
    };
    // Complete local presentation values, independent of original effect/game objects.
    // Immutable decoded models/sprites are shared locally; mutable arrays are copied.
    class ClientEffects
    {
      public:
        static constexpr std::size_t record_limit = 65536, particle_limit = 1048576, height_limit = 1048576;
        bool begin(std::uint64_t world);
        // drop_invalid (the local solo publisher): a malformed cosmetic item is
        // removed and counted, like the renderer's per-entry diagnostics, instead
        // of refusing the frame. Stale frames and record limits always refuse.
        bool receive(ClientEffectsFrame&&, std::string& error, bool drop_invalid = false);
        std::uint64_t dropped_items = 0;
        std::string drop_error;
        const ClientEffectsFrame& frame() const
        {
            return frame_;
        }

      private:
        ClientEffectsFrame frame_;
    };
} // namespace OpenBlood2
