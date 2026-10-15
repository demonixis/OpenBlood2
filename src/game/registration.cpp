#include "game/original/B2BaseClass.h"
#include "game/original/CameraSpot.h"
#include "game/original/GameStartPoint.h"
#include "game/original/Key.h"
#include "game/original/PathPoint.h"
#include "game/original/SmellHint.h"
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
                              {"Rayhit", false, false}}};
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
        const ClassInfo smell{"SmellHint", &b2base, construct<SmellHint>, {}};
        const ClassInfo character{"CBaseCharacter", &b2base, construct<CBaseCharacter>, {}};
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
        const ClassInfo camera{"CameraSpot", &b2base, construct<CameraSpot>, {}};
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
        return objects.register_class(base) && objects.register_class(b2base) && objects.register_class(camera) &&
               objects.register_class(key) && objects.register_class(start) && objects.register_class(game_start) &&
               objects.register_class(path) && objects.register_class(world_properties) &&
               objects.register_class(smell) && objects.register_class(character) && objects.register_class(player);
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
