#include "game/server_adapter.h"
#include "game/original/B2BaseClass.h"
#include "game/original/CameraSpot.h"
#include "game/original/Key.h"

// Names formerly provided by BloodServerShell.cpp. There is no client shell
// or duplicate symbol table in M1.
char g_szVarDifficulty[] = "GAME_DIFFICULTY";
char g_szVarGameType[] = "GAME_GAMETYPE";

namespace blood2 {
namespace {
template<class T> std::unique_ptr<BaseClass> construct() { return std::make_unique<T>(); }
// Original registration defaults. Optional BaseClass reader properties are
// declared without injecting values that would override ObjectCreateStruct.
const ClassInfo base{"BaseClass", nullptr, construct<BaseClass>, {
    {"Name", std::string("noname")}, {"Pos", DVector{}}, {"Rotation", DRotation{}},
    {"Flags", std::int32_t(0), false}, {"Filename", std::string{}, false}, {"Skin", std::string{}, false},
    {"Visible", false, false}, {"Shadow", false, false}, {"RotateableSprite", false, false},
    {"Chromakey", false, false}, {"Solid", false, false}, {"Gravity", false, false},
    {"TouchNotify", false, false}, {"Rayhit", false, false}
}};
const ClassInfo b2base{"B2BaseClass", &base, construct<B2BaseClass>, {
    {"DifficultyEasy", true}, {"DifficultyMed", true}, {"DifficultyHard", true},
    {"TypeCustom", true}, {"TypeSingle", true}, {"TypeActionMode", true},
    {"TypeBloodBath", true}, {"TypeCTF", true}, {"TypeCoop", true},
    {"DifficultyInsane", true, false} // Read by original code, absent from its class registration.
}};
const ClassInfo camera{"CameraSpot", &b2base, construct<CameraSpot>, {}};
const ClassInfo key{"Key", &b2base, construct<Key>, {
    {"TimeStamp", 0.0f}, {"SoundName", std::string{}}, {"SoundRadius", 0.0f}, {"LoopSound", false},
    {"MessageTarget", std::string{}}, {"MessageName", std::string{}}, {"BPrintMessage", std::string{}}
}};
}
bool register_original_classes(Objects& objects) {
    return objects.register_class(base) && objects.register_class(b2base) &&
        objects.register_class(camera) && objects.register_class(key);
}
HOBJECT GameSession::create_example(std::string_view name) {
    Properties properties{{"Name", std::string("M1_") + std::string(name)}, {"Pos", DVector{32, 16, -64}}};
    if (name == "Key") {
        properties.emplace("TimeStamp", 1.5f);
        properties.emplace("MessageTarget", std::string("M1_CameraSpot"));
        properties.emplace("MessageName", std::string("TRIGGER"));
    }
    return objects.create(objects.find_class(name), {}, Creation::World, properties);
}
}
