#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/original/LegacyEffects.h"
#include "game/original/SingularityProjectile.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace OpenBlood2;
#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #condition << '\n';                                       \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{
    bool registry()
    {
        GameSession game;
        CHECK(game.initialize());
        constexpr std::array names{"CBulletImpactSFX",
                                   "CClientBloodTrail",
                                   "CClientCastLineSFX",
                                   "CClientDebugLine",
                                   "CClientGibFX",
                                   "CClientLaserBeamSFX",
                                   "CClientLaserFX",
                                   "CClientMarkSFX",
                                   "CClientSmokeTrail",
                                   "CClientSparksSFX",
                                   "CClientSplashSFX",
                                   "CClientTracer",
                                   "CClientWeaponSFX",
                                   "CFireballProjectile",
                                   "CFlayerChain",
                                   "CFlayerPortal",
                                   "CGasGrenade",
                                   "CGooProjectile",
                                   "CImpact",
                                   "CImpactRicochet",
                                   "CModelObject",
                                   "CNightmareFireball",
                                   "CProjectile",
                                   "CShellCasing",
                                   "CSingularityAltProjectile",
                                   "CViewWeaponModel",
                                   "CombatShotgunPU",
                                   "CraneControl",
                                   "Explosion",
                                   "FireFX",
                                   "FlayerPU",
                                   "LightFX",
                                   "Nightmare",
                                   "Rain",
                                   "RotatingSprite",
                                   "Script",
                                   "SoccerBall",
                                   "SoccerGoal",
                                   "WreckingBall"};
        static_assert(names.size() == 39);
        for (const auto* name : names)
            if (!game.objects.find_class(name))
            {
                std::cerr << "Missing registration for " << name << '\n';
                return false;
            }
        return true;
    }
    bool setup(GameSession& game, const char* root, bool demo = false)
    {
        Config config;
        config.data_dir = root;
        config.demo = demo;
        std::string error;
        CHECK(game.initialize());
        CHECK(game.resources.mount_game(config, error));
        WorldModel room;
        room.points = {{-1000, 0, -1000}, {1000, 0, -1000}, {1000, 0, 1000},  {-1000, 0, 1000},
                       {-1000, 0, 800},   {1000, 0, 800},   {1000, 800, 800}, {-1000, 800, 800}};
        room.surfaces.resize(1);
        room.surfaces[0].flags = 1;
        room.polygons = {{0, {0, 1, 2, 3}}, {0, {4, 7, 6, 5}}};
        CHECK(game.collision.build(room, error));
        game.server.collision_ready = true;
        return true;
    }
    template <class T> T* create(GameSession& game, const char* name, ObjectCreateStruct state = {})
    {
        return dynamic_cast<T*>(game.server.HandleToObject(game.objects.create(game.objects.find_class(name), state)));
    }
    bool trigger(GameSession& game, HOBJECT sender, HOBJECT receiver, const char* text)
    {
        auto message = game.server.StartMessageToObject(game.server.HandleToObject(sender), receiver, 1000);
        auto string = game.server.CreateString(text);
        CHECK(message && string && game.server.WriteToMessageHString(message, string) == DE_OK);
        game.server.FreeString(string);
        return game.server.EndMessage(message) == DE_OK;
    }
    bool projectiles(const char* root)
    {
        GameSession game;
        CHECK(setup(game, root));
        ObjectCreateStruct start;
        start.m_Pos = {0, 100, 0};
        auto* shooter = create<BaseClass>(game, "BaseClass", start);
        CHECK(shooter);
        const auto base = game.objects.find_class("CProjectile");
        CHECK(base && Objects::is_kind_of(game.objects.find_class("CGrenade"), base));
        start.m_Pos.z = 700;
        auto* standard = create<CProjectile>(game, "CProjectile", start);
        CHECK(standard && standard->Setup({0, 0, 1}, 0, 10, 400, 0, shooter->m_hObject));
        const auto standard_handle = standard->m_hObject;
        CHECK(game.objects.get(standard_handle)->model &&
              game.objects.get(standard_handle)->state.m_ObjectType == OT_MODEL &&
              (game.server.GetObjectFlags(standard_handle) & FLAG_RAYHIT));
        CHECK(game.server.advance_projectiles(.3) && standard->contacted && game.objects.advance(.02));
        CHECK(!game.server.IsObjectAlive(standard_handle) && game.server.scene_explosions.empty());

        start.m_Pos.z = 700;
        auto* fire = create<CFireballProjectile>(game, "CFireballProjectile", start);
        CHECK(fire && fire->Setup({0, 0, 1}, 0, 10, 400, 0, shooter->m_hObject));
        const auto fire_handle = fire->m_hObject;
        CHECK(game.objects.get(fire_handle)->sprite && game.server.GetLightEffect(fire_handle));
        CHECK(game.server.object_particles.reports.back().recipe == 3 && fire->trail_error.empty());
        CHECK(game.objects.advance(.02) && game.objects.get(fire_handle)->state.m_Scale.x == .25f);
        CHECK(game.server.advance_projectiles(.3) && fire->contacted && game.objects.advance(.02));
        CHECK(!game.server.IsObjectAlive(fire_handle) && game.server.scene_explosions.back().type == 12);

        start.m_Pos = {100, 100, 0};
        auto* goo = create<CGooProjectile>(game, "CGooProjectile", start);
        CHECK(goo && goo->Setup({0, 0, 1}, WEAP_GIDEON_GOO, 20, 400, 0, shooter->m_hObject));
        const auto goo_handle = goo->m_hObject;
        CHECK(game.server.object_particles.reports.back().recipe == 31 && goo->trail_error.empty());
        CHECK(game.server.advance_projectiles(.1));
        CHECK(game.objects.get(goo_handle)->state.m_Pos.y < start.m_Pos.y &&
              std::abs(game.objects.get(goo_handle)->state.m_Pos.z - 40.f) < .01f);
        CHECK(game.objects.remove(goo_handle) && game.objects.finish_frame());

        start.m_Pos = {500, 100, 0};
        auto* hole = create<CSingularityAltProjectile>(game, "CSingularityAltProjectile", start);
        CHECK(hole && hole->Setup({0, 0, 1}, 0, 10, 0, 0, shooter->m_hObject));
        const auto hole_handle = hole->m_hObject;
        CHECK(game.objects.advance(.01) && hole->armed && hole->object_fx_unavailable == 0);
        CHECK(game.server.object_particles.reports[game.server.object_particles.reports.size() - 2].recipe == 10 &&
              game.server.object_particles.reports.back().recipe == 11);
        const auto model = game.objects.create(game.objects.find_class("CDestructableModel"), {}, Creation::World,
                                               {{"InitFilename", std::string("models/characters/caleb.abc")},
                                                {"InitSkin", std::string("skins/characters/caleb1.dtx")},
                                                {"Pos", DVector{150, 100, 0}},
                                                {"Pushable", true},
                                                {"Gravity", false}});
        CHECK(model && game.objects.advance(.11));
        CHECK(hole->pulls == 1 && game.objects.get(model)->velocity.x < 0);
        CHECK(game.objects.remove(shooter->m_hObject) && game.objects.finish_frame() && game.objects.advance(.11));
        CHECK(!game.server.IsObjectAlive(hole_handle));
        return true;
    }
    bool effects(const char* root)
    {
        GameSession game;
        CHECK(setup(game, root));
        ObjectCreateStruct start;
        start.m_Pos = {0, 100, 0};
        auto* sender = create<BaseClass>(game, "BaseClass", start);
        CHECK(sender);
        const auto emitter = game.objects.create_props(game.objects.find_class("CClientLaserBeamSFX"), start,
                                                       "BeamType 5;Sound \"\";Damage 0;");
        CHECK(emitter && trigger(game, sender->m_hObject, emitter, "FIRE"));
        const auto* beam = dynamic_cast<CClientLaserBeamSFX*>(game.server.HandleToObject(emitter));
        CHECK(beam && beam->effect && std::abs(beam->destination.z - 800) < .01f);
        CHECK(game.server.laser_beams.active.back().type == 5);

        const auto light = game.objects.create_props(game.objects.find_class("LightFX"), start,
                                                     "LightSwitch FALSE;Color1 \"255 64 0\";RadiusMin 75;LifeTime 1;");
        CHECK(light && !(game.server.GetObjectUserFlags(light) & 1));
        CHECK(trigger(game, sender->m_hObject, light, "ON") && (game.server.GetObjectUserFlags(light) & 1));
        const auto* presented = game.server.GetLightEffect(light);
        CHECK(presented && presented->radius == 75 && presented->color.x == 1 && presented->color.z == 0);

        auto* spark = create<CClientSparksSFX>(game, "CClientSparksSFX", start);
        CHECK(spark);
        const auto spark_handle = spark->m_hObject;
        SparkSettings recipe{
            {}, {0, 0, 1}, {200, 200, 200}, {200, 200, 0}, 4, .4f, .7f, 500, -500, "spritetextures/particle1.dtx"};
        std::string error;
        CHECK(spark->Setup(recipe, error) && error.empty());

        legacy_copy(start.m_Filename, "models/characters/caleb.abc");
        legacy_copy(start.m_SkinName, "skins/characters/caleb1.dtx");
        auto* model = create<CModelObject>(game, "CModelObject", start);
        CHECK(model && model->Setup(.5f, {0, 1, 0}, false, false));
        const auto model_handle = model->m_hObject;
        legacy_copy(start.m_Filename, "sprites/blood1.spr");
        start.m_SkinName[0] = 0;
        auto* impact = create<CImpact>(game, "CImpact", start);
        CHECK(impact && impact->Setup({.1f, .1f, 0}, {.5f, .5f, 0}, .4f, true, true));
        const auto impact_handle = impact->m_hObject;
        auto* ricochet = create<CImpactRicochet>(game, "CImpactRicochet", start);
        CHECK(ricochet && game.objects.get(ricochet->m_hObject)->state.m_Scale.x == .25f);
        const auto ricochet_handle = ricochet->m_hObject;
        const auto rotating = game.objects.create(
            game.objects.find_class("RotatingSprite"), {}, Creation::World,
            {{"Filename", std::string("sprites/blood1.spr")}, {"Pos", DVector{0, 100, 0}}, {"ZAxisRevTime", 1.f}});
        CHECK(rotating && game.objects.advance(.11) && !game.server.IsObjectAlive(spark_handle));
        CHECK(game.objects.get(model_handle)->state.m_Rotation.m_Vec.y != 0);
        CHECK(game.objects.advance(.11) && game.objects.get(impact_handle)->state.m_Scale.x > .1f &&
              game.objects.get(impact_handle)->alpha < 1 && game.objects.get(rotating)->state.m_Rotation.m_Vec.z != 0);
        for (int i = 0; i < 10; ++i)
            CHECK(game.objects.advance(.11));
        CHECK(!game.server.IsObjectAlive(light) && !game.server.IsObjectAlive(model_handle) &&
              !game.server.IsObjectAlive(impact_handle) && !game.server.IsObjectAlive(ricochet_handle));
        return true;
    }
    bool attachments(const char* root)
    {
        GameSession game;
        CHECK(setup(game, root));
        ObjectCreateStruct state;
        state.m_Pos = {0, 3, 0};
        auto* owner = create<BaseClass>(game, "BaseClass", state);
        auto* trail = create<CClientBloodTrail>(game, "CClientBloodTrail", state);
        CHECK(owner && trail && trail->Setup(owner->m_hObject));
        const auto trail_id = trail->m_hObject;
        CHECK(game.objects.advance(.03) && trail->splats == 1 && !game.server.blood_splats.active.empty());

        state.m_Pos = {50, 100, 800};
        auto* mark = create<CClientMarkSFX>(game, "CClientMarkSFX", state);
        CHECK(mark && mark->Setup({0, 0, -1}, AMMO_BULLET, SURFTYPE_METAL, owner->m_hObject));
        const auto mark_id = mark->m_hObject;
        const auto mark_effect = mark->effect;
        CHECK(mark_effect && game.server.shotgun_effects.sprites.back().id == mark_effect);
        state.m_Pos.x += 100;
        auto* impact = create<CBulletImpactSFX>(game, "CBulletImpactSFX", state);
        std::string error;
        CHECK(impact && impact->Setup({0, 0, -1}, .05f, "sprites/blthole.spr", error));
        const auto impact_id = impact->m_hObject;

        state.m_Pos = {0, 100, 0};
        auto* laser = create<CClientLaserFX>(game, "CClientLaserFX", state);
        auto* tracer = create<CClientTracer>(game, "CClientTracer", state);
        CHECK(laser && tracer && laser->Setup(owner->m_hObject) && tracer->Setup({0, 100, 800}, owner->m_hObject));
        const auto laser_id = laser->m_hObject;
        const auto tracer_id = tracer->m_hObject;
        CHECK(game.objects.advance(.02) && laser->visible && laser->lines[0].to.z > 800 &&
              !game.server.IsObjectAlive(tracer_id) && !game.server.IsObjectAlive(impact_id));

        legacy_copy(state.m_Filename, "models/ammo/bshell.abc");
        legacy_copy(state.m_SkinName, "skins/ammo/bshell.dtx");
        auto* casing = create<CShellCasing>(game, "CShellCasing", state);
        CHECK(casing && casing->Setup(owner->m_hObject, true));
        const auto casing_id = casing->m_hObject;
        CHECK(game.objects.get(casing_id)->model && game.objects.get(casing_id)->velocity.x < 0 &&
              !(game.server.GetObjectFlags(casing_id) & FLAG_VISIBLE));
        CHECK(game.objects.advance(.01) && (game.server.GetObjectFlags(casing_id) & FLAG_VISIBLE));
        CHECK(game.objects.remove(owner->m_hObject) && game.objects.finish_frame() && game.objects.finish_frame());
        CHECK(!game.server.IsObjectAlive(trail_id) && !game.server.IsObjectAlive(mark_id) &&
              !game.server.IsObjectAlive(laser_id));
        CHECK(std::none_of(game.server.shotgun_effects.sprites.begin(), game.server.shotgun_effects.sprites.end(),
                           [mark_effect](const auto& sprite) { return sprite.id == mark_effect; }));
        for (unsigned i = 0; i < 205; ++i)
            CHECK(game.objects.advance(.1));
        CHECK(!game.server.IsObjectAlive(casing_id));
        return true;
    }
    bool demo(const char* root)
    {
        GameSession game;
        CHECK(setup(game, root, true));
        auto* gib = create<CClientGibFX>(game, "CClientGibFX");
        CHECK(gib);
        const auto id = gib->m_hObject;
        DebrisEffect settings;
        settings.position = {0, 20, 0};
        settings.velocity = {0, -1, 0};
        settings.dimensions = {5, 5, 5};
        settings.flags = SURFTYPE_STONE / 10;
        CHECK(gib->Setup(settings) && game.server.debris_effects.back().velocity.y == 1 &&
              game.server.debris_effects.back().event && !game.client_effects.destruction_debris.active.empty());
        CHECK(game.objects.advance(.01) && !game.server.IsObjectAlive(id));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "registry")
        return registry() ? 0 : 1;
    if (argc != 3)
        return 2;
    const std::string_view name = argv[1];
    if (name == "projectiles")
        return projectiles(argv[2]) ? 0 : 1;
    if (name == "effects")
        return effects(argv[2]) ? 0 : 1;
    if (name == "attachments")
        return attachments(argv[2]) ? 0 : 1;
    if (name == "demo")
        return demo(argv[2]) ? 0 : 1;
    return 2;
}
