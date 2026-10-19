#include "game/original/AmmoPickups.h"
#include "game/original/B2BaseClass.h"
#include "game/original/BoneLeech.h"
#include "game/original/BuiltinModel.h"
#include "game/original/BuiltinSprite.h"
#include "game/original/CameraObj.h"
#include "game/original/CameraSpot.h"
#include "game/original/CinematicTrigger.h"
#include "game/original/CivilianAI.h"
#include "game/original/ClientExplosionSFX.h"
#include "game/original/ClientLightFX.h"
#include "game/original/ClientParticleStreamSFX.h"
#include "game/original/ClientSplatFX.h"
#include "game/original/ConversationTrigger.h"
#include "game/original/Corpse.h"
#include "game/original/CultistAI.h"
#include "game/original/DestructableModel.h"
#include "game/original/DetailSprite.h"
#include "game/original/Door.h"
#include "game/original/DrudgeLord.h"
#include "game/original/ExitHint.h"
#include "game/original/ExitTrigger.h"
#include "game/original/Fanatic.h"
#include "game/original/FlareProjectile.h"
#include "game/original/GameStartPoint.h"
#include "game/original/Gib.h"
#include "game/original/Gideon.h"
#include "game/original/Grenade.h"
#include "game/original/HandWeaponModel.h"
#include "game/original/ItemPickups.h"
#include "game/original/Key.h"
#include "game/original/KeyFramer.h"
#include "game/original/KeyPickup.h"
#include "game/original/MadScientistAI.h"
#include "game/original/NapalmProjectile.h"
#include "game/original/ObjectivesTrigger.h"
#include "game/original/PathPoint.h"
#include "game/original/PolyGrid.h"
#include "game/original/PowerupPickups.h"
#include "game/original/Projectile.h"
#include "game/original/Prophet.h"
#include "game/original/ProximityBomb.h"
#include "game/original/RandomSoundFX.h"
#include "game/original/RemoteBomb.h"
#include "game/original/RotatingBrush.h"
#include "game/original/RotatingDoor.h"
#include "game/original/ShikariAI.h"
#include "game/original/SingularityProjectile.h"
#include "game/original/SkyObjects.h"
#include "game/original/SmellHint.h"
#include "game/original/SoulDrudge.h"
#include "game/original/Sound.h"
#include "game/original/SoundFX.h"
#include "game/original/Spawner.h"
#include "game/original/Tchernotronic.h"
#include "game/original/TeslaProjectile.h"
#include "game/original/Trigger.h"
#include "game/original/TripLaser.h"
#include "game/original/VolumeBrush.h"
#include "game/original/WeaponPickups.h"
#include "game/original/WorldModelDebris.h"
#include "game/original/WorldProperties.h"
#include "game/server_adapter.h"

// Names formerly provided by BloodServerShell.cpp. There is no client shell
// or duplicate symbol table in M1.
char g_szVarDifficulty[] = "GAME_DIFFICULTY";
char g_szVarGameType[] = "GAME_GAMETYPE";

namespace OpenBlood2
{
    namespace
    {
        template <class T> std::unique_ptr<BaseClass> construct()
        {
            return std::make_unique<T>();
        }
        // Original registration defaults. Optional BaseClass reader properties are
        // declared without injecting values that would override ObjectCreateStruct.
        const ClassInfo base{"BaseClass",
                             nullptr,
                             construct<BaseClass>,
                             {{"Name", std::string("noname")},
                              {"Pos", DVector{}},
                              {"Rotation", DRotation{}},
                              {"Flags", std::int32_t(0), false},
                              {"Filename", std::string{}, false},
                              {"Skin", std::string{}, false},
                              {"Visible", false, false},
                              {"Shadow", false, false},
                              {"RotateableSprite", false, false},
                              {"Chromakey", false, false},
                              {"Solid", false, false},
                              {"Gravity", false, false},
                              {"TouchNotify", false, false},
                              {"Rayhit", false, false},
                              // Retired editor flag on older DAT rows (BB_BloodArena, BB_Bodies,
                              // BB_Midgard). No original class declares or reads it: B2BaseClass reads
                              // TypeActionMode. Like the engine prop list, keep it unread, never defaulted.
                              {"TypeAction", false, false}}};
        const ClassInfo sky_model{"DemoSkyWorldModel",
                                  &base,
                                  construct<DemoSkyWorldModel>,
                                  {{"SkyDims", DVector{}},
                                   {"Flags", std::int32_t(1)},
                                   {"Index", std::int32_t(0)},
                                   {"InnerPercentX", .1f},
                                   {"InnerPercentY", .1f},
                                   {"InnerPercentZ", .1f}}};
        const ClassInfo sky_pointer{"SkyPointer", &base, construct<SkyPointer>, []
                                    {
                                        auto p = sky_model.properties;
                                        p.push_back({"SkyObjectName", std::string{}, true, 98});
                                        return p;
                                    }()};
        const ClassInfo builtin_model{
            "Model",
            &base,
            construct<BuiltinModel>,
            {{"Filename", std::string{}}, {"Skin", std::string{}}, {"Flags", std::int32_t(FLAG_VISIBLE)}}};
        const ClassInfo builtin_container{"Container",
                                          &base,
                                          construct<BuiltinContainer>,
                                          {{"Flags", std::int32_t(FLAG_VISIBLE)}, {"ContainerCode", std::int32_t(0)}}};
        const ClassInfo builtin_sprite{"Sprite",
                                       &base,
                                       construct<BuiltinSprite>,
                                       {{"Filename", std::string{}},
                                        {"Flags", std::int32_t(FLAG_VISIBLE)},
                                        {"Scale", DVector{1, 1, 1}},
                                        {"Color", DVector{255, 255, 255}},
                                        {"Alpha", 1.0f}}};
        const ClassInfo shockwave_projectile{
            "CBehemothShockwaveProjectile", &base, construct<CBehemothShockwaveProjectile>, {}};
        const ClassInfo remote{"CRemoteBomb", &base, construct<CRemoteBomb>, {}};
        const ClassInfo proximity{"CProximityBomb", &base, construct<CProximityBomb>, {}};
        const ClassInfo grenade{"CGrenade", &base, construct<CGrenade>, {}};
        const ClassInfo flare_projectile{"CFlareProjectile", &base, construct<CFlareProjectile>, {}};
        const ClassInfo tesla_projectile{"CTeslaProjectile", &base, construct<CTeslaProjectile>, {}};
        const ClassInfo tesla_bolt{"CTeslaBoltProjectile", &base, construct<CTeslaBoltProjectile>, {}};
        const ClassInfo tesla_ball{"CTeslaBallProjectile", &base, construct<CTeslaBallProjectile>, {}};
        const ClassInfo napalm_projectile{"CNapalmProjectile", &base, construct<CNapalmProjectile>, {}};
        const ClassInfo loogie{"CShikariLoogieProjectile", &base, construct<CShikariLoogieProjectile>, {}};
        const ClassInfo singularity_projectile{"CSingularityProjectile", &base, construct<CSingularityProjectile>, {}};
        const ClassInfo flare_alt{"CFlareAltProjectile", &base, construct<CFlareAltProjectile>, {}};
        const ClassInfo flare_burst{"CFlareBurstProjectile", &base, construct<CFlareBurstProjectile>, {}};
        const ClassInfo energy_projectile{"CEnergyBlastProjectile", &base, construct<CEnergyBlastProjectile>, {}};
        const ClassInfo howitzer_shell{"CHowitzerShell", &base, construct<CHowitzerShell>, {}};
        const ClassInfo hand_model{"CHandWeaponModel", &base, construct<CHandWeaponModel>, {}};
        const ClassInfo b2base{
            "B2BaseClass",
            &base,
            construct<B2BaseClass>,
            {
                {"GameSettings", std::int32_t(0), false}, // Original PROP_DEFINEGROUP editor marker.
                {"DifficultyEasy", true},
                {"DifficultyMed", true},
                {"DifficultyHard", true},
                {"TypeCustom", true},
                {"TypeSingle", true},
                {"TypeActionMode", true},
                {"TypeBloodBath", true},
                {"TypeCTF", true},
                {"TypeCoop", true},
                {"DifficultyInsane", true, false} // Read by original code, absent from its class registration.
            }};
        // Original BEGIN_CLASS(CGib): destructable aggregate plus world-file Init props.
        const ClassInfo gib{"CGib",
                            &b2base,
                            construct<CGib>,
                            {{"DamageTriggerTarget", std::string{}},
                             {"DamageTriggerMessage", std::string{}},
                             {"DeathTriggerTarget", std::string{}},
                             {"DeathTriggerMessage", std::string{}},
                             {"TriggerDestroyOnly", false},
                             {"SpawnObject", std::string{}, true, MAX_GP_STRING_LEN},
                             {"SpawnObjectVel", DVector{}},
                             {"InitFilename", std::string{}},
                             {"InitSkin", std::string{}},
                             {"InitFlags", std::int32_t(FLAG_VISIBLE | FLAG_RAYHIT | FLAG_GRAVITY | FLAG_SHADOW)},
                             {"InitAnim", std::string{}},
                             {"HitPoints", 20.0f},
                             {"ObjectMass", 10.0f},
                             {"Kickable", true}}};
        const ClassInfo corpse{"CCorpse", &b2base, construct<CCorpse>, {}};
        const ClassInfo camera_object{"CameraObj",
                                      &b2base,
                                      construct<CameraObj>,
                                      {{"Type", std::int32_t(0)},
                                       {"ActiveTime", -1.0f},
                                       {"AllowPlayerMovement", false},
                                       {"StartActive", false},
                                       {"IsListener", true},
                                       {"HidePlayer", false},
                                       {"TypeActionMode", false}}};
        const ClassInfo detail_sprite{"DetailSprite",
                                      &b2base,
                                      construct<DetailSprite>,
                                      {{"Filename", std::string{}},
                                       {"ScaleX", 1.0f},
                                       {"ScaleY", 1.0f},
                                       {"FlushWithWorld", false},
                                       {"Rotatable", true},
                                       {"Chromakey", false},
                                       {"Repeat", false},
                                       {"RepeatX", std::int32_t(1)},
                                       {"RepeatY", std::int32_t(1)},
                                       {"SpacingX", 0.0f},
                                       {"SpacingY", 0.0f}}};
        // Original BEGIN_CLASS(Tchernotronic): hidden Technoberg model/skin, destroy trigger.
        const ClassInfo tchernotronic{"Tchernotronic",
                                      &b2base,
                                      construct<Tchernotronic>,
                                      {{"Filename", std::string("Models\\WorldObjects\\Technoberg.abc")},
                                       {"Skin", std::string("Skins\\WorldObjects\\Technoberg.dtx")},
                                       {"DestroyTriggerTarget", std::string{}},
                                       {"DestroyTriggerMessage", std::string{}}}};
        const ClassInfo trip_laser{"TripLaser",
                                   &b2base,
                                   construct<TripLaser>,
                                   {{"Filename", std::string("Models\\default.abc")},
                                    {"Skin", std::string("Skins\\ammo\\beamred.dtx")},
                                    {"Dims", DVector{1, 20, 1}},
                                    {"Color", DVector{255, 0, 0}},
                                    {"Alpha", .5f},
                                    {"StartOn", false},
                                    {"TouchTriggerTarget", std::string{}},
                                    {"TouchTriggerMessage", std::string{}},
                                    {"Sound", std::string{}},
                                    {"TouchSound", std::string{}},
                                    {"Damage", 10.0f}}};
        const ClassInfo spawner{"Spawner",
                                &b2base,
                                construct<Spawner>,
                                // Original GenericProp/szSpawn buffers: MAX_GP_STRING_LEN.
                                {{"SpawnObject", std::string{}, true, MAX_GP_STRING_LEN},
                                 {"MinVelocity", DVector{0, 500, 0}},
                                 {"MaxVelocity", DVector{0, 500, 0}},
                                 {"Sound", std::string{}},
                                 {"SoundRadius", 500.0f},
                                 {"UseTriggerObjPos", true},
                                 {"StartActive", false},
                                 {"RespawnCount", std::int32_t(-1)},
                                 {"RespawnRate", 1.0f},
                                 {"CreateRiftEffect", false}}};
        const ClassInfo sound{"Sound",
                              &base,
                              construct<Sound>,
                              {{"Filename", std::string{}},
                               {"Priority", std::int32_t(0)},
                               {"VoiceType", std::int32_t(0)},
                               {"OuterRadius", 100.0f},
                               {"InnerRadius", 10.0f},
                               {"Volume", std::int32_t(100)},
                               {"Ambient", true},
                               {"FileStream", false}}};
        const ClassInfo sound_fx{"SoundFX",
                                 &b2base,
                                 construct<SoundFX>,
                                 {{"StartOn", true},
                                  {"RampUpSound", std::string{}},
                                  {"RampDownSound", std::string{}},
                                  {"Sound", std::string{}},
                                  {"Priority", std::int32_t(0)},
                                  {"VoiceType", std::int32_t(0)},
                                  {"OuterRadius", 500.0f},
                                  {"InnerRadius", 100.0f},
                                  {"Volume", std::int32_t(100)},
                                  {"Ambient", true},
                                  {"FileStream", false}}};
        const ClassInfo random_sound{"RandomSoundFX", &b2base, construct<RandomSoundFX>, []
                                     {
                                         std::vector<PropertySpec> p{
                                             {"InitiallyOn", true},         {"PositionalSound", true},
                                             {"SequentialPlay", false},     {"LoopSequence", false},
                                             {"PosRangeRadius", 0.0f},      {"SoundRadius", 1000.0f},
                                             {"Volume", std::int32_t(100)}, {"Ambient", true},
                                             {"MinWaitTime", 60.0f},        {"MaxWaitTime", 60.0f}};
                                         for (unsigned i = 1; i <= 10; ++i)
                                             p.push_back({"SoundFile" + std::to_string(i), std::string{}});
                                         return p;
                                     }()};
        const ClassInfo cinematic_trigger{
            "CinematicTrigger", &b2base, construct<CinematicTrigger>, []
            {
                std::vector<PropertySpec> p{{"TypeActionMode", false}};
                for (unsigned i = 1; i <= 10; ++i)
                {
                    const auto n = std::to_string(i);
                    p.push_back({"Delay" + n, 0.0f});
                    for (const char* name :
                         {"Sound", "WhoPlaysSound", "StartSoundTriggerTarget", "StartSoundTriggerMsg"})
                        p.push_back({name + n, std::string{}});
                }
                return p;
            }()};
        const ClassInfo keyframer{"KeyFramer",
                                  &b2base,
                                  construct<KeyFramer>,
                                  {{"ObjectName", std::string{}},
                                   {"BaseKeyName", std::string{}},
                                   {"StartActive", false},
                                   {"Looping", false}}};
        const ClassInfo client_sfx{"CClientSFX", &b2base, construct<CClientSFX>, {}};
        const ClassInfo client_splat{"CClientSplatFX", &client_sfx, construct<CClientSplatFX>, {}};
        const ClassInfo particle_stream{"CClientParticleStreamSFX",
                                        &client_sfx,
                                        construct<CClientParticleStreamSFX>,
                                        {{"SystemRadius", 1000.f},
                                         {"PositionRadius", 1.f},
                                         {"MinVelocity", 0.f},
                                         {"MaxVelocity", 25.f},
                                         {"NumParticles", std::int32_t(1)},
                                         {"VelSpread", 0.f},
                                         {"MinColor", DVector{255, 255, 255}},
                                         {"MaxColor", DVector{255, 255, 255}},
                                         {"SystemAlpha", 1.f},
                                         {"MinLifetime", 1.f},
                                         {"MaxLifetime", 2.f},
                                         {"RampTime", 0.f},
                                         {"AddDelay", .1f},
                                         {"Gravity", 15.f},
                                         {"RampAmount", false},
                                         {"RampOffset", false},
                                         {"RampVelocity", false},
                                         {"RampLifetime", false},
                                         {"InitiallyOn", false},
                                         {"ParticleFile", std::string("SpriteTextures\\drop32_1.dtx")},
                                         {"SoundRadius", 200.f},
                                         {"RampUpSound", std::string{}},
                                         {"LoopSound", std::string{}},
                                         {"RampDownSound", std::string{}}}};
        const ClassInfo scene_explosion{"CClientExplosionSFX",
                                        &client_sfx,
                                        construct<CClientExplosionSFX>,
                                        {{"ExplosionType", std::int32_t(0)},
                                         {"Damage", 50.0f},
                                         {"DamageRadius", 200.0f},
                                         {"Direction", DVector{0, 1, 0}},
                                         {"WaveFile", std::string("Sounds\\exp_tnt.wav")}}};
        const ClassInfo client_light{"ClientLightFX", &client_sfx, construct<ClientLightFX>, []
                                     {
                                         std::vector<PropertySpec> p{{"HitPoints", 1.0f},
                                                                     {"LightSwitch", true},
                                                                     {"LifeTime", 0.0f},
                                                                     {"Color", DVector{255, 255, 255}},
                                                                     {"IntensityMin", .5f},
                                                                     {"IntensityMax", 1.0f},
                                                                     {"IntensityWaveform", std::int32_t(0)},
                                                                     {"IntensityFreq", 4.0f},
                                                                     {"IntensityPhase", 0.0f},
                                                                     {"RadiusMin", 200.0f},
                                                                     {"RadiusMax", 500.0f},
                                                                     {"RadiusWaveform", std::int32_t(0)},
                                                                     {"RadiusFreq", 4.0f},
                                                                     {"RadiusPhase", 0.0f},
                                                                     {"TriggerDestroyOnly", false},
                                                                     {"CanDamage", true},
                                                                     {"SpawnObjectVel", DVector{}}};
                                         for (auto name : {"RampUpSound", "RampDownSound", "DamageTriggerTarget",
                                                           "DamageTriggerMessage", "DeathTriggerTarget",
                                                           "DeathTriggerMessage"})
                                             p.push_back({name, std::string{}});
                                         p.push_back({"SpawnObject", std::string{}, true, MAX_GP_STRING_LEN});
                                         for (auto name : {"CastShadowsFlag", "SolidLightFlag", "OnlyLightWorldFlag",
                                                           "DontLightBackfacingFlag", "FogLightFlag"})
                                             p.push_back({name, false});
                                         return p;
                                     }()};
        const ClassInfo light_SquareWaveLightFX{
            "SquareWaveLightFX",
            &client_light,
            construct<SquareWaveLightFX>,
            {{"IntensityWaveform", std::int32_t(1)}, {"RadiusWaveform", std::int32_t(1)}}};
        const ClassInfo light_SawWaveLightFX{
            "SawWaveLightFX",
            &client_light,
            construct<SawWaveLightFX>,
            {{"IntensityWaveform", std::int32_t(2)}, {"RadiusWaveform", std::int32_t(2)}}};
        const ClassInfo light_RampUpWaveLightFX{
            "RampUpWaveLightFX",
            &client_light,
            construct<RampUpWaveLightFX>,
            {{"IntensityWaveform", std::int32_t(3)}, {"RadiusWaveform", std::int32_t(3)}}};
        const ClassInfo light_RampDownWaveLightFX{
            "RampDownWaveLightFX",
            &client_light,
            construct<RampDownWaveLightFX>,
            {{"IntensityWaveform", std::int32_t(4)}, {"RadiusWaveform", std::int32_t(4)}}};
        const ClassInfo light_SineWaveLightFX{
            "SineWaveLightFX",
            &client_light,
            construct<SineWaveLightFX>,
            {{"IntensityWaveform", std::int32_t(5)}, {"RadiusWaveform", std::int32_t(5)}}};
        const ClassInfo light_Flicker1WaveLightFX{
            "Flicker1WaveLightFX",
            &client_light,
            construct<Flicker1WaveLightFX>,
            {{"IntensityWaveform", std::int32_t(6)}, {"RadiusWaveform", std::int32_t(6)}}};
        const ClassInfo light_Flicker2WaveLightFX{
            "Flicker2WaveLightFX",
            &client_light,
            construct<Flicker2WaveLightFX>,
            {{"IntensityWaveform", std::int32_t(7)}, {"RadiusWaveform", std::int32_t(7)}}};
        const ClassInfo light_Flicker3WaveLightFX{
            "Flicker3WaveLightFX",
            &client_light,
            construct<Flicker3WaveLightFX>,
            {{"IntensityWaveform", std::int32_t(8)}, {"RadiusWaveform", std::int32_t(8)}}};
        const ClassInfo light_Flicker4WaveLightFX{
            "Flicker4WaveLightFX",
            &client_light,
            construct<Flicker4WaveLightFX>,
            {{"IntensityWaveform", std::int32_t(9)}, {"RadiusWaveform", std::int32_t(9)}}};
        const ClassInfo light_StrobeWaveLightFX{
            "StrobeWaveLightFX",
            &client_light,
            construct<StrobeWaveLightFX>,
            {{"IntensityWaveform", std::int32_t(10)}, {"RadiusWaveform", std::int32_t(10)}}};
        const ClassInfo light_SearchWaveLightFX{
            "SearchWaveLightFX",
            &client_light,
            construct<SearchWaveLightFX>,
            {{"IntensityWaveform", std::int32_t(11)}, {"RadiusWaveform", std::int32_t(11)}}};
        const ClassInfo light_FlickerLight{
            "FlickerLight",
            &client_light,
            construct<FlickerLight>,
            {{"IntensityWaveform", std::int32_t(7)}, {"RadiusWaveform", std::int32_t(7)}}};
        const ClassInfo light_GlowingLight{
            "GlowingLight",
            &client_light,
            construct<GlowingLight>,
            {{"IntensityWaveform", std::int32_t(5)}, {"RadiusWaveform", std::int32_t(5)}}};
        const ClassInfo start{"StartPoint", &base, construct<StartPoint>, {}};
        const ClassInfo game_start{"GameStartPoint",
                                   &start,
                                   construct<GameStartPoint>,
                                   {{"Multiplayer", false},
                                    {"TriggerTarget", std::string{}},
                                    {"TriggerMessage", std::string{}},
                                    {"TeamID", std::int32_t(0)}}};
        const ClassInfo path{"PathPoint",
                             &b2base,
                             construct<PathPoint>,
                             {{"ActionTarget", std::string{}}, {"ActionMessage", std::string{}}}};
        const ClassInfo exit_hint{"ExitHint", &b2base, construct<ExitHint>, {}};
        const ClassInfo smell{"SmellHint", &b2base, construct<SmellHint>, {}};
        const ClassInfo character{"CBaseCharacter", &b2base, construct<CBaseCharacter>, {}};
        template <class T, AI_Mgr::NPCType Type> std::unique_ptr<BaseClass> construct_npc()
        {
            auto actor = std::make_unique<T>();
            actor->ConfigureRuntime(Type);
            return actor;
        }
        const ClassInfo ai_manager{"AI_Mgr", &character, construct<AI_Mgr>, []
                                   {
                                       std::vector<PropertySpec> p{{"SpotTriggerTarget", std::string{}},
                                                                   {"SpotTriggerMessage", std::string{}},
                                                                   {"TriggerRelayTarget", std::string{}},
                                                                   {"DamageTriggerTarget", std::string{}},
                                                                   {"DamageTriggerMessage", std::string{}},
                                                                   {"DeathTriggerTarget", std::string{}},
                                                                   {"DeathTriggerMessage", std::string{}},
                                                                   {"TriggerDestroyOnly", false},
                                                                   {"SpawnObject", std::string{}, true, MAX_GP_STRING_LEN},
                                                                   {"SpawnObjectVel", DVector{}},
                                                                   {"CanDamage", true}};
                                       for (const auto* name :
                                            {"Bullets", "BMG", "Shells", "Grenades", "Rockets", "Flares", "Charges",
                                             "Cells", "Fuel", "Proximity_Bombs"})
                                           p.push_back({name, 0.0f, false});
                                       for (int i = 1; i <= 5; ++i)
                                           p.push_back({"AIWeapon" + std::to_string(i), std::string{}, false, 31});
                                       return p;
                                   }()};
        const ClassInfo cultist{"CultistAI",
                                &ai_manager,
                                construct_npc<CultistAI, AI_Mgr::NPCType::Cultist>,
                                {{"RandomHitPoints", 0.0f},
                                 {"AIState", std::string("IDLE"), true, 31},
                                 {"AIWeapon1", std::string("BERETTA"), true, 31},
                                 {"Bullets", 150.0f},
                                 {"Flares", 20.0f},
                                 {"Male", true},
                                 {"Dims", DVector{19, 41, 14}},
                                 {"ClownSkin", false},
                                 {"RobeSkin", false}}};
        const ClassInfo civilian{"CivilianAI",
                                 &ai_manager,
                                 construct_npc<CivilianAI, AI_Mgr::NPCType::Civilian>,
                                 {{"RandomHitPoints", 0.0f},
                                  {"AIState", std::string("IDLE"), true, 31},
                                  {"AIWeapon1", std::string("NONE"), true, 31},
                                  {"Male", true},
                                  {"LabTech", false},
                                  {"Scared", false},
                                  {"Dims", DVector{21, 37, 21}},
                                  {"SororitySkin", false}}};
        const ClassInfo fanatic{"Fanatic",
                                &ai_manager,
                                construct_npc<Fanatic, AI_Mgr::NPCType::Fanatic>,
                                {{"RandomHitPoints", 0.0f},
                                 {"AIState", std::string("IDLE"), true, 31},
                                 {"AIWeapon1", std::string("BERETTA"), true, 31},
                                 {"AIWeapon2", std::string("COMBAT"), true, 31},
                                 {"AIWeapon3", std::string("ASSAULTRIFLE"), true, 31},
                                 {"Bullets", 300.0f},
                                 {"BMG", 150.0f},
                                 {"Shells", 50.0f},
                                 {"Grenades", 50.0f},
                                 {"Rockets", 25.0f},
                                 {"Flares", 40.0f},
                                 {"Cells", 200.0f},
                                 {"Charges", 120.0f},
                                 {"Fuel", 100.0f},
                                 {"Proximity_Bombs", 25.0f},
                                 {"Dims", DVector{21, 38, 19}}}};
        // Original class registrations: RandomHitPoints, AIState and hidden Dims.
        const ClassInfo bone_leech{
            "BoneLeech",
            &ai_manager,
            construct_npc<BoneLeech, AI_Mgr::NPCType::BoneLeech>,
            {{"RandomHitPoints", 0.0f}, {"AIState", std::string("IDLE"), true, 31}, {"Dims", DVector{2, 1.5f, 11}}}};
        const ClassInfo drudge_lord{
            "DrudgeLord",
            &ai_manager,
            construct_npc<DrudgeLord, AI_Mgr::NPCType::DrudgeLord>,
            {{"RandomHitPoints", 0.0f}, {"AIState", std::string("IDLE"), true, 31}, {"Dims", DVector{31, 51, 25}}}};
        const ClassInfo soul_drudge{
            "SoulDrudge",
            &ai_manager,
            construct_npc<SoulDrudge, AI_Mgr::NPCType::SoulDrudge>,
            {{"RandomHitPoints", 50.0f}, {"AIState", std::string("IDLE"), true, 31}, {"Dims", DVector{21, 36, 18}}}};
        // Original BEGIN_CLASS(ShikariAI): RandomHitPoints, AIState and hidden Dims only.
        const ClassInfo shikari{"ShikariAI",
                                &ai_manager,
                                construct_npc<ShikariAI, AI_Mgr::NPCType::Shikari>,
                                {{"RandomHitPoints", 50.0f},
                                 {"AIState", std::string("IDLE"), true, 31},
                                 {"Dims", DVector{19, 30, 24}}}};
        // Original BEGIN_CLASS(MadScientistAI). AI_Mgr::ReadProp never reads HitPoints,
        // ArmorPoints or the digitless AIWeapon: constructor values apply.
        const ClassInfo mad_scientist{"MadScientistAI",
                                      &ai_manager,
                                      construct_npc<MadScientistAI, AI_Mgr::NPCType::MadScientist>,
                                      {{"HitPoints", 30.0f},
                                       {"RandomHitPoints", 0.0f},
                                       {"ArmorPoints", 0.0f},
                                       {"AIState", std::string("IDLE"), true, 31},
                                       {"AIWeapon", std::string("NONE"), true, 31}}};
        const ClassInfo prophet{"Prophet",
                                &ai_manager,
                                construct_npc<Prophet, AI_Mgr::NPCType::Prophet>,
                                {{"RandomHitPoints", 0.0f},
                                 {"AIState", std::string("IDLE"), true, 31},
                                 {"AIWeapon1", std::string("BERETTA"), true, 31},
                                 {"AIWeapon2", std::string("COMBAT"), true, 31},
                                 {"AIWeapon3", std::string("ASSAULTRIFLE"), true, 31},
                                 {"AIWeapon4", std::string("COMBAT"), true, 31},
                                 {"AIWeapon5", std::string("ASSAULTRIFLE"), true, 31},
                                 {"Bullets", 750.0f},
                                 {"BMG", 150.0f},
                                 {"Shells", 150.0f},
                                 {"Grenades", 50.0f},
                                 {"Rockets", 100.0f},
                                 {"Flares", 100.0f},
                                 {"Cells", 600.0f},
                                 {"Charges", 400.0f},
                                 {"Fuel", 200.0f},
                                 {"Dims", DVector{19, 43, 15}}}};
        const ClassInfo gideon{
            "Gideon",
            &ai_manager,
            construct_npc<Gideon, AI_Mgr::NPCType::Gideon>,
            {{"RandomHitPoints", 0.0f}, {"AIState", std::string("IDLE"), true, 31}, {"Enhanced", false}}};
        const ClassInfo player{"CPlayerObj", &character, construct<CPlayerObj>, {}};
        const ClassInfo world_properties{
            "WorldProperties",
            &b2base,
            construct<WorldProperties>,
            {{"WorldName", std::string{}, true, 499},
             {"Author", std::string{}, true, 499},
             {"CDTrack", std::string{}, true, 499},
             {"InstrumentFiles", std::string{}, true, 499},
             {"AmbientList", std::string{}, true, 499},
             {"CruisingList", std::string{}, true, 499},
             {"HarddrivingList", std::string{}, true, 499},
             {"MusicDirectory", std::string{}, true, 499},
             {"StartCutscene", std::string{}, true, 499},
             {"EndCutscene", std::string{}, true, 499},
             {"PanSkyTexture", std::string{}, true, 499},
             {"EnvironmentMap", std::string{}, true, 499},
             {"FogEnable", false},
             {"FogColor", DVector{127, 127, 127}},
             {"FogNearZ", 1.0f},
             {"FogFarZ", 5000.0f},
             {"SkyFog", false},
             {"SkyFogNearZ", 100.0f},
             {"SkyFogFarZ", 1000.0f},
             {"FarZ", 5000.0f},
             {"ModelAdd", DVector{}},
             {"ModelDirAdd", DVector{}},
             {"PanSky", false},
             {"PanSkyOffsetX", 10.0f},
             {"PanSkyOffsetZ", 10.0f},
             {"PanSkyScaleX", 10.0f},
             {"PanSkyScaleZ", 10.0f},
             {"Gravity", DVector{0, -2000, 0}},
             {"SoftSky", std::string("textures\\environmentmaps\\clouds\\clouds"), true, 499}}};
        const ClassInfo polygrid{"PolyGrid",
                                 &client_sfx,
                                 construct<PolyGrid>,
                                 {{"Dims", DVector{}},
                                  {"Color1", DVector{255, 255, 255}},
                                  {"Color2", DVector{255, 255, 255}},
                                  {"SpriteSurfaceName", std::string("Sprites\\VolumeBrushSprites\\Water1.spr")},
                                  {"XScaleMin", 15.f},
                                  {"XScaleMax", 15.f},
                                  {"YScaleMin", 15.f},
                                  {"YScaleMax", 15.f},
                                  {"XScaleDuration", 10.f},
                                  {"YScaleDuration", 10.f},
                                  {"XPan", 10.f},
                                  {"YPan", 10.f},
                                  {"Alpha", .7f},
                                  {"NumPolies", std::int32_t(160)},
                                  {"PlasmaType", std::int32_t(1)},
                                  {"Ring1Rate", std::int32_t(50)},
                                  {"Ring2Rate", std::int32_t(10)},
                                  {"Ring3Rate", std::int32_t(30)},
                                  {"Ring4Rate", std::int32_t(20)}}};
        const ClassInfo volume{"VolumeBrush",
                               &b2base,
                               construct<VolumeBrush>,
                               {{"Visible", false},
                                {"Viscosity", 0.0f},
                                {"Hidden", false},
                                {"ShowSurface", true},
                                {"SpriteSurfaceName", std::string{}},
                                {"FogEnable", false},
                                {"FogFarZ", 300.0f},
                                {"FogNearZ", -100.0f},
                                {"FogColor", DVector{}},
                                {"XScaleMin", 15.0f},
                                {"XScaleMax", 25.0f},
                                {"YScaleMin", 15.0f},
                                {"YScaleMax", 25.0f},
                                {"XScaleDuration", 60.0f},
                                {"YScaleDuration", 60.0f},
                                {"SurfaceHeight", 2.0f},
                                {"SurfaceAlpha", .7f},
                                {"SurfaceColor1", DVector{255, 255, 255}},
                                {"SurfaceColor2", DVector{255, 255, 255}},
                                {"Code", std::int32_t(CC_NOTHING)},
                                {"Current", DVector{}},
                                {"Damage", 0.0f},
                                {"DamageType", std::int32_t(0)},
                                {"NumSurfacePolies", std::int32_t(160)},
                                {"Locked", false},
                                {"UnlockKeyRemove", false},
                                {"UnlockKeyName", std::string{}}}};
        const ClassInfo water{"Water",
                              &volume,
                              construct<Water>,
                              {{"Viscosity", .5f},
                               {"Code", std::int32_t(CC_WATER)},
                               {"DamageType", std::int32_t(6)},
                               {"SpriteSurfaceName", std::string("Sprites\\WaterBlue.spr")}}};
        const ClassInfo blood{"Blood",
                              &volume,
                              construct<Blood>,
                              {{"Viscosity", .8f},
                               {"Code", std::int32_t(CC_BLOOD)},
                               {"DamageType", std::int32_t(6)},
                               {"SpriteSurfaceName", std::string("Sprites\\WaterBlood.spr")}}};
        const ClassInfo acid{"Acid",
                             &volume,
                             construct<Acid>,
                             {{"Viscosity", .5f},
                              {"Code", std::int32_t(CC_ACID)},
                              {"SpriteSurfaceName", std::string("Sprites\\WaterAcid.spr")}}};
        const ClassInfo ladder{"Ladder",
                               &volume,
                               construct<Ladder>,
                               {{"Code", std::int32_t(CC_LADDER)},
                                {"ShowSurface", false},
                                {"SurfaceHeight", 0.0f},
                                {"SurfaceColor1", DVector{}},
                                {"SurfaceColor2", DVector{}},
                                {"XScaleDuration", 10.0f},
                                {"YScaleDuration", 10.0f}}};
        const ClassInfo pickup{"PickupObject",
                               &b2base,
                               construct<PickupObject>,
                               {{"PickupTriggerTarget", std::string{}}, {"PickupTriggerMessage", std::string{}}}};
        const ClassInfo item_pickup{"ItemPickup", &pickup, construct<ItemPickup>, {}};
        const ClassInfo charged_pickup{"ItemPickupCharged", &item_pickup, construct<ItemPickupCharged>, {}};
        const ClassInfo item_weapon{"ItemPickupWeapon", &item_pickup, construct<ItemPickupWeapon>, {}};
        const ClassInfo remote_pickup{"RemotesPU", &item_weapon, construct<RemotesPU>, {}};
        const ClassInfo proximity_pickup{"ProximitiesPU", &item_weapon, construct<ProximitiesPU>, {}};
        const ClassInfo flashlight_pickup{"FlashlightPU", &charged_pickup, construct<FlashlightPU>, {}};
        const ClassInfo medkit_pickup{"MedKitPU", &charged_pickup, construct<MedKitPU>, {}};
        const ClassInfo binoculars_pickup{"BinocularsPU", &charged_pickup, construct<BinocularsPU>, {}};
        const ClassInfo eye_pickup{"TheEyePU", &charged_pickup, construct<TheEyePU>, {}};
        const ClassInfo goggles_pickup{"NightGogglesPU", &charged_pickup, construct<NightGogglesPU>, {}};
        const ClassInfo weapon_pickup{"WeaponPickup", &pickup, construct<WeaponPickup>, {}};
        const ClassInfo bugspray_pickup{"BugSprayPU", &weapon_pickup, construct<BugSprayPU>, {}};
        const ClassInfo beretta_pickup{"BerettaPU", &weapon_pickup, construct<BerettaPU>, {}};
        const ClassInfo assault_pickup{"AssaultRiflePU", &weapon_pickup, construct<AssaultRiflePU>, {}};
        const ClassInfo flare_pickup{"FlareGunPU", &weapon_pickup, construct<FlareGunPU>, {}};
        const ClassInfo tesla_pickup{"TeslaCannonPU", &weapon_pickup, construct<TeslaCannonPU>, {}};
        const ClassInfo shotgun_pickup{"ShotgunPU", &weapon_pickup, construct<ShotgunPU>, {}};
        const ClassInfo singularity_pickup{"SingularityPU", &weapon_pickup, construct<SingularityPU>, {}};
        const ClassInfo mac10_pickup{"SubMachineGunPU", &weapon_pickup, construct<SubMachineGunPU>, {}};
        const ClassInfo sniper_pickup{"SniperRiflePU", &weapon_pickup, construct<SniperRiflePU>, {}};
        const ClassInfo ammo_pickup{"AmmoPickup", &pickup, construct<AmmoPickup>, {}};
        const ClassInfo powerup_pickup{"PowerupPickup", &pickup, construct<PowerupPickup>, {}};
        const ClassInfo health_base{"HealthBase", &powerup_pickup, construct<HealthBase>, {}};
        const ClassInfo armor_base{"ArmorBase", &powerup_pickup, construct<ArmorBase>, {}};
        const ClassInfo enhancement_base{"EnhancementBase", &powerup_pickup, construct<EnhancementBase>, {}};
        const ClassInfo stealth_pickup{"StealthPU", &enhancement_base, construct<StealthPU>, {}};
        const ClassInfo willpower_pickup{"InvulnerabilityPU", &enhancement_base, construct<InvulnerabilityPU>, {}};
        const ClassInfo bullet_ammo{"BulletAmmoPU", &ammo_pickup, construct<BulletAmmoPU>, {}};
        const ClassInfo shell_ammo{"ShellAmmoPU", &ammo_pickup, construct<ShellAmmoPU>, {}};
        const ClassInfo bmg_ammo{"BMGAmmoPU", &ammo_pickup, construct<BMGAmmoPU>, {}};
        const ClassInfo flare_ammo{"FlareAmmoPU", &ammo_pickup, construct<FlareAmmoPU>, {}};
        const ClassInfo diebugdie_ammo{"DieBugDieAmmoPU", &ammo_pickup, construct<DieBugDieAmmoPU>, {}};
        const ClassInfo howitzer_ammo{"HowitzerAmmoPU", &ammo_pickup, construct<HowitzerAmmoPU>, {}};
        const ClassInfo fuel_ammo{"FuelAmmoPU", &ammo_pickup, construct<FuelAmmoPU>, {}};
        const ClassInfo battery_ammo{"BatteryAmmoPU", &ammo_pickup, construct<BatteryAmmoPU>, {}};
        const ClassInfo health_pickup{"HealthPU", &health_base, construct<HealthPU>, {}};
        const ClassInfo megahealth_pickup{"MegaHealthPU", &health_base, construct<MegaHealthPU>, {}};
        const ClassInfo ward_pickup{"WardPU", &armor_base, construct<WardPU>, {}};
        const ClassInfo necroward_pickup{"NecroWardPU", &armor_base, construct<NecroWardPU>, {}};
        const ClassInfo key_pickup{"KeyPickup",
                                   &b2base,
                                   construct<KeyPickup>,
                                   {{"ObjectName", std::string("SuperKey")},
                                    {"ResourceNum", std::int32_t(0)},
                                    {"IconFile", std::string("key.pcx")},
                                    {"HiIconFile", std::string("key_h.pcx")},
                                    {"ModelFile", std::string("models/powerups/key.abc")},
                                    {"ModelSkin", std::string("skins/powerups/key.dtx")},
                                    {"UseCount", std::int32_t(1)},
                                    {"PickupTriggerTarget", std::string{}},
                                    {"PickupTriggerMessage", std::string{}}}};
        const ClassInfo trigger{
            "Trigger", &b2base, construct<Trigger>, []
            {
                std::vector<PropertySpec> properties{{"Dims", DVector{}},
                                                     {"ResetTime", 0.0f},
                                                     {"ActivationSound", std::string{}},
                                                     {"SoundRadius", 200.0f},
                                                     {"TouchActivate", true},
                                                     {"PlayerActivate", true},
                                                     {"AIActivate", true},
                                                     {"ObjectActivate", false},
                                                     {"TriggerRelayActivate", true},
                                                     {"NamedObjectActivate", false},
                                                     {"ActivationObjectName", std::string{}},
                                                     {"ActivationCount", std::int32_t(1)},
                                                     {"Locked", false},
                                                     {"LockedSound", std::string{}},
                                                     {"UnlockedSound", std::string{}},
                                                     {"UnlockKeyName", std::string("Key")},
                                                     {"LockedMsg", std::string{}, false},
                                                     {"UnlockedMsg", std::string{}, false},
                                                     {"AdditionalTargets", std::int32_t(0), false}};
                for (unsigned i = 1; i <= 10; ++i)
                {
                    auto n = std::to_string(i);
                    properties.push_back({"TargetName" + n, std::string{}, true, MAX_GP_STRING_LEN});
                    properties.push_back({"MessageName" + n, std::string{}, true, MAX_GP_STRING_LEN});
                    properties.push_back({i == 1 ? "MessageDelay" : "MessageDelay" + n, 0.0f});
                }
                return properties;
            }()};
        const ClassInfo exit_trigger{"ExitTrigger",
                                     &trigger,
                                     construct<ExitTrigger>,
                                     {{"NextWorld", std::string{}},
                                      {"StartPointName", std::string("start")},
                                      {"EndEpisode", false},
                                      {"EndOfEpisode", false, false},
                                      {"SubWorld", false},
                                      {"TouchActivate", false},
                                      {"AIActivate", false}}};
        const ClassInfo objectives_trigger{
            "ObjectivesTrigger",
            &trigger,
            construct<ObjectivesTrigger>,
            {{"ResourceNum", std::int32_t(0)},
             {"TitleToDisplay", std::string("Objective #1:"), true, MAX_GP_STRING_LEN},
             {"TextToDisplay", std::string("Use the '|' character to force a line break..."), true, MAX_GP_STRING_LEN},
             {"Sound", std::string("Sounds\\Thunder.wav")},
             {"TouchActivate", false},
             {"AIActivate", false},
             {"TriggerRelayActivate", false}}};
        const ClassInfo conversation_trigger{
            "ConversationTrigger", &trigger, construct<ConversationTrigger>, []
            {
                std::vector<PropertySpec> p{
                    {"CharacterType", std::int32_t(0)}, {"RepeatLastMessage", false}, {"LoopToFirstMessage", false},
                    {"TouchActivate", false},           {"AIActivate", false},        {"ObjectActivate", false},
                    {"TriggerRelayActivate", false}};
                for (unsigned i = 0; i < 8; ++i)
                    p.push_back({std::string("WaveFile") + (i ? std::to_string(i + 1) : ""),
                                 std::string(i ? "" : "default.wav"), true, 199});
                return p;
            }()};
        const ClassInfo big_trigger{"BigTrigger", &trigger, construct<BigTrigger>, {}};
        const ClassInfo toggle_trigger{"ToggleTrigger", &trigger, construct<ToggleTrigger>, []
                                       {
                                           std::vector<PropertySpec> properties{{"StartOn", false}};
                                           for (unsigned i = 1; i <= 10; ++i)
                                               properties.push_back(
                                                   {"OffMessageName" + std::to_string(i), std::string{}});
                                           return properties;
                                       }()};
        const ClassInfo door{"Door",
                             &b2base,
                             construct<Door>,
                             {{"Visible", true},
                              {"Solid", true},
                              {"Speed", 0.0f},
                              {"MoveDist", 0.0f},
                              {"WaitTime", 0.0f},
                              {"OpenDelay", 0.0f},
                              {"ClosingSpeed", 0.0f},
                              {"MoveDir", DVector{}},
                              {"TriggerDims", DVector{}},
                              {"OpenBusySound", std::string{}},
                              {"OpenStopSound", std::string{}},
                              {"CloseBusySound", std::string{}},
                              {"CloseStopSound", std::string{}},
                              {"PortalName", std::string{}},
                              {"ActivationObjectName", std::string{}},
                              {"LockedSound", std::string{}},
                              {"UnlockedSound", std::string{}},
                              {"SoundRadius", 1000.0f},
                              {"Waveform", std::int32_t(0)},
                              {"SelfTriggerOpts", std::int32_t(0)},
                              {"UnlockKeyName", std::string("Key")},
                              {"BoxPhysics", true},
                              {"PlayerActivate", true},
                              {"AIActivate", true},
                              {"TriggerRelayActivate", true},
                              {"TriggerClosed", false},
                              {"StartOpen", false},
                              {"RemainsOpen", false},
                              {"SelfTrigger", false},
                              {"FireThrough", false},
                              {"TouchActivate", false},
                              {"ObjectActivate", false},
                              {"NamedObjectActivate", false},
                              {"Locked", false}}};
        const ClassInfo destructable_brush{
            "CDestructableBrush", &b2base, construct<CDestructableBrush>, []
            {
                std::vector<PropertySpec> p{{"Visible", true},
                                            {"Solid", true},
                                            {"Gravity", false},
                                            {"DebrisStuff", std::int32_t(0), false},
                                            {"HitPoints", 0.0f},
                                            {"ObjectMass", 100.0f},
                                            {"Destructable", true},
                                            {"Pushable", false},
                                            {"BoxPhysics", true},
                                            {"AllowMarks", true},
                                            {"FireThrough", false},
                                            {"SurfaceType", std::int32_t(0)},
                                            {"TriggerDestroyOnly", false},
                                            {"CanDamage", true},
                                            {"SpawnObjectVel", DVector{}},
                                            {"Amount", std::int32_t(10)},
                                            {"Scale", 1.0f},
                                            {"ExplodeDamage", 100.0f},
                                            {"DamageRadius", 200.0f}};
                for (auto name : {"DamageTriggerTarget", "DamageTriggerMessage", "DeathTriggerTarget",
                                  "DeathTriggerMessage", "CustomTexture", "CustomSound", "CustomModel"})
                    p.push_back({name, std::string{}});
                p.push_back({"SpawnObject", std::string{}, true, MAX_GP_STRING_LEN});
                for (auto name : {"Stone", "Metal", "Wood", "Energy", "Glass", "Terrain", "Plastic", "Flesh", "Liquid",
                                  "Exploding", "Custom"})
                    p.push_back({name, false});
                return p;
            }()};
        const ClassInfo destructable_model{
            "CDestructableModel", &b2base, construct<CDestructableModel>, []
            {
                std::vector<PropertySpec> p{{"DebrisStuff", std::int32_t(0), false},
                                            {"Visible", true},
                                            {"Solid", true},
                                            {"Gravity", true},
                                            {"Shadow", true},
                                            {"DestroyVisible", true},
                                            {"DestroySolid", true},
                                            {"DestroyGravity", true},
                                            {"HitPoints", 100.0f},
                                            {"DamageHitPoints", 50.0f},
                                            {"ObjectMass", 100.0f},
                                            {"Alpha", 1.0f},
                                            {"Destructable", true},
                                            {"Pushable", false},
                                            {"SurfaceType", std::int32_t(0)},
                                            {"ModelScale", 1.0f},
                                            {"TintColor", DVector{}},
                                            {"Chrome", false},
                                            {"TriggerDestroyOnly", false},
                                            {"CanDamage", true},
                                            {"SpawnObjectVel", DVector{}},
                                            {"Amount", std::int32_t(10)},
                                            {"Scale", 1.0f},
                                            {"ExplodeDamage", 100.0f},
                                            {"DamageRadius", 200.0f}};
                for (auto name : {"InitDims", "DamageDims", "DestroyDims"})
                    p.push_back({name, DVector{}});
                for (auto name :
                     {"InitFilename", "InitSkin", "DamageFilename", "DamageSkin", "DestroyFilename", "DestroySkin",
                      "SlidingSound", "DamageTriggerTarget", "DamageTriggerMessage", "DeathTriggerTarget",
                      "DeathTriggerMessage", "CustomTexture", "CustomSound", "CustomModel"})
                    p.push_back({name, std::string{}});
                p.push_back({"SpawnObject", std::string{}, true, MAX_GP_STRING_LEN});
                for (auto name : {"Stone", "Metal", "Wood", "Energy", "Glass", "Terrain", "Plastic", "Flesh", "Liquid",
                                  "Exploding", "Custom"})
                    p.push_back({name, false});
                return p;
            }()};
        const ClassInfo rotating_brush{"RotatingBrush", &destructable_brush, construct<RotatingBrush>, []
                                       {
                                           std::vector<PropertySpec> p{{"RotatingStuff", std::int32_t(0), false},
                                                                       {"AllowMarks", false},
                                                                       {"StartOn", true},
                                                                       {"SoundRadius", 1000.0f},
                                                                       {"SpinUpSound", std::string{}},
                                                                       {"BusySound", std::string{}},
                                                                       {"SpinDownSound", std::string{}}};
                                           for (auto axis : {"X", "Y", "Z"})
                                           {
                                               for (auto suffix : {"AxisRevTime", "AxisSpinUpTime", "AxisSpinDownTime"})
                                                   p.push_back({std::string(axis) + suffix, 0.0f});
                                               p.push_back({std::string(axis) + "RotateForward", true});
                                           }
                                           return p;
                                       }()};
        const ClassInfo world_debris{"WorldModelDebris", &destructable_brush, construct<WorldModelDebris>, []
                                     {
                                         auto p = rotating_brush.properties;
                                         for (auto& prop : p)
                                             if (prop.name == "AllowMarks")
                                                 prop.value = true;
                                         p.push_back({"Visible", false});
                                         p.push_back({"Solid", false});
                                         p.push_back({"Gravity", false});
                                         p.push_back({"InitialVelocity", DVector{}});
                                         p.push_back({"GravityMultiplier", 1.0f});
                                         return p;
                                     }()};
        const ClassInfo rotating_door{"RotatingDoor",
                                      &door,
                                      construct<RotatingDoor>,
                                      {{"RotationPoint", DVector{}},
                                       {"RotationAngles", DVector{}},
                                       {"BoxPhysics", false},
                                       {"PushPlayerBack", true}}};
        const ClassInfo camera{"CameraSpot", &b2base, construct<CameraSpot>, {}};
        const ClassInfo conveyor{"Conveyor",
                                 &volume,
                                 construct<Conveyor>,
                                 {{"Code", std::int32_t(CC_CONVEYOR)},
                                  {"ShowSurface", false},
                                  {"SurfaceHeight", 0.0f},
                                  {"SurfaceColor1", DVector{}},
                                  {"SurfaceColor2", DVector{}},
                                  {"XScaleDuration", 10.0f},
                                  {"YScaleDuration", 10.0f}}};
        const ClassInfo damage{"Damage",
                               &volume,
                               construct<::Damage>,
                               {{"Code", std::int32_t(CC_DAMAGE)},
                                {"Damage", 100.0f},
                                {"ShowSurface", false},
                                {"SurfaceHeight", 0.0f},
                                {"SurfaceColor1", DVector{}},
                                {"SurfaceColor2", DVector{}},
                                {"XScaleDuration", 10.0f},
                                {"YScaleDuration", 10.0f}}};
        const ClassInfo freefall{"FreeFall",
                                 &volume,
                                 construct<FreeFall>,
                                 {{"Code", std::int32_t(CC_FREEFALL)},
                                  {"Damage", 9999999.0f},
                                  {"DamageType", std::int32_t(DamageType::FreeFall)},
                                  {"ShowSurface", false},
                                  {"SurfaceHeight", 5.0f},
                                  {"SurfaceColor1", DVector{}},
                                  {"SurfaceColor2", DVector{}},
                                  {"XScaleDuration", 10.0f},
                                  {"YScaleDuration", 10.0f}}};
        const ClassInfo key{"Key",
                            &b2base,
                            construct<Key>,
                            {{"TimeStamp", 0.0f},
                             {"SoundName", std::string{}},
                             {"SoundRadius", 0.0f},
                             {"LoopSound", false},
                             {"MessageTarget", std::string{}},
                             {"MessageName", std::string{}},
                             {"BPrintMessage", std::string{}}}};
    } // namespace
    bool register_original_classes(Objects& objects)
    {
        return objects.register_class(base) && objects.register_class(sky_model) &&
               objects.register_class(sky_pointer) && objects.register_class(builtin_model) && objects.register_class(builtin_container) &&
               objects.register_class(builtin_sprite) && objects.register_class(b2base) &&
               objects.register_class(camera) && objects.register_class(client_sfx) &&
               objects.register_class(client_splat) && objects.register_class(particle_stream) &&
               objects.register_class(scene_explosion) && objects.register_class(client_light) &&
               objects.register_class(light_SquareWaveLightFX) && objects.register_class(light_SawWaveLightFX) &&
               objects.register_class(light_RampUpWaveLightFX) && objects.register_class(light_RampDownWaveLightFX) &&
               objects.register_class(light_SineWaveLightFX) && objects.register_class(light_Flicker1WaveLightFX) &&
               objects.register_class(light_Flicker2WaveLightFX) && objects.register_class(light_Flicker3WaveLightFX) &&
               objects.register_class(light_Flicker4WaveLightFX) && objects.register_class(light_StrobeWaveLightFX) &&
               objects.register_class(light_SearchWaveLightFX) && objects.register_class(light_FlickerLight) &&
               objects.register_class(light_GlowingLight) && objects.register_class(camera_object) &&
               objects.register_class(cinematic_trigger) && objects.register_class(random_sound) &&
               objects.register_class(sound_fx) && objects.register_class(detail_sprite) &&
               objects.register_class(spawner) && objects.register_class(trip_laser) &&
               objects.register_class(tchernotronic) && objects.register_class(sound) &&
               objects.register_class(keyframer) && objects.register_class(key) && objects.register_class(start) &&
               objects.register_class(game_start) && objects.register_class(path) &&
               objects.register_class(world_properties) && objects.register_class(smell) &&
               objects.register_class(exit_hint) && objects.register_class(character) &&
               objects.register_class(ai_manager) && objects.register_class(cultist) &&
               objects.register_class(civilian) && objects.register_class(gideon) && objects.register_class(fanatic) &&
               objects.register_class(prophet) && objects.register_class(bone_leech) &&
               objects.register_class(drudge_lord) && objects.register_class(soul_drudge) &&
               objects.register_class(shikari) && objects.register_class(mad_scientist) &&
               objects.register_class(player) && objects.register_class(polygrid) && objects.register_class(volume) &&
               objects.register_class(water) && objects.register_class(blood) && objects.register_class(acid) &&
               objects.register_class(ladder) && objects.register_class(conveyor) && objects.register_class(damage) &&
               objects.register_class(freefall) && objects.register_class(trigger) &&
               objects.register_class(key_pickup) && objects.register_class(pickup) &&
               objects.register_class(item_pickup) && objects.register_class(item_weapon) &&
               objects.register_class(proximity_pickup) && objects.register_class(remote_pickup) &&
               objects.register_class(charged_pickup) && objects.register_class(medkit_pickup) &&
               objects.register_class(flashlight_pickup) && objects.register_class(eye_pickup) &&
               objects.register_class(goggles_pickup) && objects.register_class(binoculars_pickup) &&
               objects.register_class(weapon_pickup) && objects.register_class(bugspray_pickup) &&
               objects.register_class(beretta_pickup) && objects.register_class(assault_pickup) &&
               objects.register_class(flare_pickup) && objects.register_class(tesla_pickup) &&
               objects.register_class(shotgun_pickup) && objects.register_class(singularity_pickup) &&
               objects.register_class(mac10_pickup) && objects.register_class(sniper_pickup) &&
               objects.register_class(ammo_pickup) && objects.register_class(powerup_pickup) &&
               objects.register_class(health_base) && objects.register_class(armor_base) &&
               objects.register_class(bullet_ammo) && objects.register_class(shell_ammo) &&
               objects.register_class(bmg_ammo) && objects.register_class(flare_ammo) &&
               objects.register_class(diebugdie_ammo) && objects.register_class(howitzer_ammo) &&
               objects.register_class(fuel_ammo) && objects.register_class(battery_ammo) &&
               objects.register_class(health_pickup) && objects.register_class(megahealth_pickup) &&
               objects.register_class(enhancement_base) && objects.register_class(stealth_pickup) &&
               objects.register_class(willpower_pickup) &&
               objects.register_class(ward_pickup) && objects.register_class(necroward_pickup) &&
               objects.register_class(exit_trigger) && objects.register_class(objectives_trigger) &&
               objects.register_class(conversation_trigger) && objects.register_class(big_trigger) &&
               objects.register_class(toggle_trigger) && objects.register_class(door) &&
               objects.register_class(rotating_door) && objects.register_class(destructable_brush) &&
               objects.register_class(destructable_model) && objects.register_class(rotating_brush) &&
               objects.register_class(world_debris) && objects.register_class(hand_model) &&
               objects.register_class(gib) && objects.register_class(corpse) && objects.register_class(proximity) &&
               objects.register_class(remote) && objects.register_class(grenade) &&
               objects.register_class(flare_projectile) && objects.register_class(flare_alt) &&
               objects.register_class(tesla_projectile) && objects.register_class(tesla_bolt) &&
               objects.register_class(tesla_ball) && objects.register_class(napalm_projectile) &&
               objects.register_class(loogie) && objects.register_class(singularity_projectile) &&
               objects.register_class(flare_burst) && objects.register_class(energy_projectile) &&
               objects.register_class(howitzer_shell) && objects.register_class(shockwave_projectile);
    }
    HOBJECT GameSession::create_example(std::string_view name)
    {
        Properties properties{{"Name", std::string("M1_") + std::string(name)}, {"Pos", DVector{32, 16, -64}}};
        if (name == "Key")
        {
            properties.emplace("TimeStamp", 1.5f);
            properties.emplace("MessageTarget", std::string("M1_CameraSpot"));
            properties.emplace("MessageName", std::string("TRIGGER"));
        }
        return objects.create(objects.find_class(name), {}, Creation::World, properties);
    }
} // namespace OpenBlood2
