#include "game/original/Script.h"
#include "game/original/FireEffects.h"
#include "game/original/Trigger.h"
#include "game/server_adapter.h"
#include "audio_fixture.h"
#include <cmath>
#include <fstream>
#include <iostream>

using namespace OpenBlood2;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << '\n';                                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{
    bool mount(GameSession& game, const std::filesystem::path& root)
    {
        CHECK(game.initialize());
        test_resource::Directory scripts;
        const auto file = [&](const char* name, std::string_view text)
        {
            scripts.files[name] = {text.begin(), text.end()};
        };
        file("scene.txt", "; quoted names, definitions and comma separators\r\n"
                          "define TARGET = 3\r\n"
                          "START\r\n"
                          "OBJECTASSIGN #TARGET, \"fixture target\"\r\n"
                          "OBJECTFLAGVISIBLE #TARGET 1\r\n"
                          "WAIT 0.25\r\n"
                          "OBJECTTELEPORT #TARGET, 0, 5, 0\r\n"
                          "OBJECTCREATE 5, BaseClass, 7, 8, 9\r\n"
                          "OBJECTCREATENEAR 6 BaseClass #TARGET\r\n"
                          "OBJECTFLAGS 5 777\r\n"
                          "OBJECTSCALE 6 2 3 4\r\n"
                          "OBJECTASSIGN 7 relay\r\n"
                          "SENDTRIGGER 7 TRIGGER\r\n"
                          "WAIT 0.2\r\n"
                          "SENDTRIGGERNAMED namedRelay TRIGGER\r\n"
                          "OBJECTDESTROY 6\r\n"
                          "OBJECTDESTROY 5\r\n"
                          "END\r\n");
        file("unsupported.txt", "START\nOBJECTCREATE 5 BaseClass 0 1 2\nCAMERASELECT 0\nEND\n");
        file("badslot.txt", "START\nOBJECTTELEPORT 30 0 0 0\nEND\n");
        file("motion.txt", "OBJECTASSIGN 0 mover\n"
                           "OBJECTASSIGN 1 destination\n"
                           "OBJECTMOVETOOBJ 0 .5 1 0 0 0\n"
                           "PLAYSOUNDPOS 3 7 8 9 cue.wav 100 1\n"
                           "WAIT .3\n"
                           "PLAYSOUNDOBJ 3 0 cue.wav 200 1\n"
                           "WAIT .3\n"
                           "KILLSOUND 3\n"
                           "PLAYSOUNDOBJ 4 1 cue.wav 500 0\n"
                           "END\n");
        file("effects.txt", "OBJECTASSIGN 0 source\n"
                             "OBJECTCREATEMODELNEAR 1 0 marker.abc marker.dtx\n"
                             "OBJECTCREATE_EXPLOSIONNEAR 0 .2 1000 0 .1 .7\n"
                             "WAIT .1\n"
                             "OBJECTCREATE_EXPLOSIONAT 7 8 9 .2 1000 0 .1 .7\n"
                             "OBJECTGIB 0\n"
                             "END\n");
        scripts.files["cue.wav"] = test_audio::wave();
        scripts.files["marker.abc"] = test_model::abc().b;
        scripts.files["marker.dtx"] = {1, 2, 3};
        scripts.children["models"].children["explosions"].files["explosion_3.abc"] = test_model::abc().b;
        scripts.children["skins"].children["explosions"].files["explosion_1.dtx"] = {1, 2, 3};
        scripts.children["sounds"].files["exp_tnt.wav"] = test_audio::wave();
        for (unsigned index = 1; index <= 3; ++index)
        {
            const auto gib = "gib" + std::to_string(index);
            scripts.children["models"].children["gibs"].children["flesh"].files[gib + ".abc"] = test_model::abc().b;
            scripts.children["skins"].children["gibs"].children["flesh"].files[gib + ".dtx"] = {1, 2, 3};
        }
        test_resource::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        const auto [offset, size] = test_resource::write_dir(archive, scripts);
        archive.patch(131, offset);
        archive.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        CHECK(!ec);
        const auto path = root / "scripts.rez";
        {
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(archive.b.data()), static_cast<std::streamsize>(archive.b.size()));
            CHECK(stream.good());
        }
        std::string error;
        CHECK(game.resources.mount(path, error));
        WorldModel floor;
        floor.points = {{-1000, 0, -1000}, {1000, 0, -1000}, {1000, 0, 1000}, {-1000, 0, 1000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(game.collision.build(floor, error));
        game.server.collision_ready = true;
        return true;
    }

    HOBJECT named(GameSession& game, std::string_view name)
    {
        for (const auto id : game.objects.snapshot())
            if (const auto* record = game.objects.get(id); record && record->life == Life::Alive && record->state.m_Name == name)
                return record->handle;
        return nullptr;
    }

    bool run(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(mount(game, root));
        const auto target = game.objects.create(game.objects.find_class("BaseClass"), {}, Creation::World,
                                                {{"Name", std::string("fixture target")}, {"Pos", DVector{1, 2, 3}}});
        const auto relay_handle = game.objects.create(game.objects.find_class("Trigger"), {}, Creation::World,
                                                      {{"Name", std::string("relay")}, {"ObjectActivate", true}});
        const auto named_handle = game.objects.create(game.objects.find_class("Trigger"), {}, Creation::World,
                                                      {{"Name", std::string("namedRelay")}, {"ObjectActivate", true}});
        const auto effect = game.objects.create(game.objects.find_class("Script"), {}, Creation::World,
                                                {{"ScriptName", std::string("scene.txt")}, {"StartTime", .125f}});
        auto* relay = dynamic_cast<Trigger*>(game.server.HandleToObject(relay_handle));
        auto* named_relay = dynamic_cast<Trigger*>(game.server.HandleToObject(named_handle));
        auto* script = dynamic_cast<Script*>(game.server.HandleToObject(effect));
        CHECK(target && relay && named_relay && script && game.objects.advance(.12) && script->executed == 0);
        CHECK(game.objects.advance(.02) && script->executed == 4 && (game.server.GetObjectFlags(target) & FLAG_VISIBLE));
        CHECK(game.objects.advance(.24) && !named(game, "ScriptObj5"));
        CHECK(game.objects.advance(.02) && !script->failures && relay->activations == 1);
        const auto created = named(game, "ScriptObj5"), near = named(game, "ScriptObj6");
        CHECK(created && near);
        const auto* body = game.objects.get(near);
        CHECK(body && body->state.m_Pos.y == 7 && body->state.m_Scale.y == 3 &&
              game.server.GetObjectFlags(created) == (FLAG_VISIBLE | FLAG_GOTHRUWORLD));
        CHECK(game.objects.advance(.21) && !script->failures && named_relay->activations == 1 && !game.server.IsObjectAlive(near) &&
              !game.server.IsObjectAlive(created) && game.server.IsObjectAlive(effect));
        const auto executed = script->executed;
        CHECK(game.objects.advance(1) && script->executed == executed);
        return true;
    }

    bool motion(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(mount(game, root));
        const auto base = game.objects.find_class("BaseClass");
        const auto mover = game.objects.create(base, {}, Creation::World,
                                                {{"Name", std::string("mover")}, {"Pos", DVector{0, 100, 0}}});
        const auto destination = game.objects.create(base, {}, Creation::World,
                                                      {{"Name", std::string("destination")}, {"Pos", DVector{100, 100, 0}},
                                                       {"Rotation", DRotation{{0, 1, 0}, 0}}});
        const auto effect = game.objects.create(game.objects.find_class("Script"), {}, Creation::World,
                                                {{"ScriptName", std::string("motion.txt")}});
        const auto* script = dynamic_cast<const Script*>(game.server.HandleToObject(effect));
        CHECK(mover && destination && script && game.objects.advance(.01) && script->executed == 5);
        const auto& voices = game.server.audio.voices();
        CHECK(voices.size() == 1);
        const auto fixed = voices.begin()->second;
        CHECK(!fixed.request.speaker && fixed.owner == effect && fixed.position.x == 7 && fixed.position.y == 8 &&
              fixed.request.loop && fixed.request.inner_radius == 10 && fixed.request.priority == 2);
        DVector shifted{150, 100, 0};
        CHECK(game.server.TeleportObject(destination, &shifted) == DE_OK);
        CHECK(game.objects.advance(.25) && script->executed == 5);
        CHECK(std::abs(game.objects.get(mover)->state.m_Pos.x - 50.f) < .01f &&
              std::abs(game.objects.get(mover)->state.m_Rotation.m_Vec.y - .7071068f) < .01f);
        CHECK(game.objects.advance(.06) && script->executed == 7 && !script->failures && !game.server.audio.get(fixed.id));
        CHECK(voices.size() == 1 && voices.begin()->second.request.speaker == mover &&
              voices.begin()->second.request.inner_radius == 20 && voices.begin()->second.request.loop);
        CHECK(game.objects.advance(.31) && script->executed == 10 && !script->failures);
        CHECK(game.objects.get(mover)->state.m_Pos.x == 100 && game.objects.get(mover)->state.m_Rotation.m_Vec.y == 1);
        CHECK(voices.size() == 1 && voices.begin()->second.request.speaker == destination &&
              !voices.begin()->second.request.loop && voices.begin()->second.auto_remove);
        CHECK(game.server.advance_audio(.2) && voices.empty());
        return true;
    }

    bool effects(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(mount(game, root));
        const DVector center{10, 20, 30};
        const auto source = game.objects.create(game.objects.find_class("BaseClass"), {}, Creation::World,
                                                {{"Name", std::string("source")}, {"Pos", center}});
        DVector dims{4, 8, 6};
        CHECK(source && game.server.SetObjectDims(source, &dims) == DE_OK);
        const auto effect = game.objects.create(game.objects.find_class("Script"), {}, Creation::World,
                                                {{"ScriptName", std::string("effects.txt")}});
        const auto* script = dynamic_cast<const Script*>(game.server.HandleToObject(effect));
        CHECK(script && game.objects.advance(.01) && script->executed == 4 && !script->failures);
        const auto* model = game.objects.get(named(game, "ScriptObj1"));
        const auto near = named(game, "ScriptObjExp");
        const auto* explosion = dynamic_cast<const Explosion*>(game.server.HandleToObject(near));
        const auto inside = [&](DVector p)
        {
            return p.x >= 4 && p.x <= 16 && p.y >= 16 && p.y <= 28 && std::abs(p.z - 30) < .001f;
        };
        CHECK(model && model->state.m_ObjectType == OT_MODEL);
        CHECK(inside(model->state.m_Pos));
        CHECK(explosion && inside(game.objects.get(near)->state.m_Pos));
        CHECK(game.objects.advance(.02) && explosion->model && explosion->voice && explosion->light);
        CHECK(game.objects.advance(.1) && script->executed == 7 && !script->failures && !game.server.IsObjectAlive(source));
        CHECK(game.server.flesh_gibs.reports.size() == 1);
        const auto& gibs = game.server.flesh_gibs.reports[0];
        CHECK(gibs.source == "source" && gibs.requested >= 3 && gibs.requested <= 8 && gibs.created > 0 &&
              gibs.position.x == center.x && gibs.dimensions.y == dims.y && gibs.velocity.y >= 200 && gibs.velocity.y <= 500);
        const Explosion* exact = nullptr;
        for (auto id : game.objects.snapshot())
            if (const auto* body = game.objects.get(id); body && body->handle != near &&
                same_name(body->state.m_Name, "ScriptObjExp") && body->state.m_Pos.x == 7)
                exact = dynamic_cast<const Explosion*>(body->game.get());
        CHECK(exact && game.objects.advance(.02) && exact->model && exact->voice && exact->light);
        CHECK(game.objects.advance(.25) && !game.server.IsObjectAlive(near) && game.server.IsObjectAlive(model->handle));
        return true;
    }

    bool refuses(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(mount(game, root));
        auto type = game.objects.find_class("Script");
        for (const auto& [file, reason] : {std::pair{"unsupported.txt", "unsupported command CAMERASELECT"},
                                          std::pair{"badslot.txt", "invalid argument 1"},
                                          std::pair{"missing.txt", "Script resource is missing"}})
        {
            CHECK(!game.objects.create(type, {}, Creation::World, {{"ScriptName", std::string(file)}}));
            CHECK(!game.objects.diagnostics().empty() && game.objects.diagnostics().back().find(reason) != std::string::npos);
        }
        CHECK(game.objects.advance(1) && !named(game, "ScriptObj5"));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    if (mode == "run")
        return run(argv[2]) ? 0 : 1;
    if (mode == "motion")
        return motion(argv[2]) ? 0 : 1;
    if (mode == "effects")
        return effects(argv[2]) ? 0 : 1;
    return mode == "refuses" && refuses(argv[2]) ? 0 : 1;
}
