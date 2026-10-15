#include "camera_fixture.h"
#include "engine/container.h"
#include "game/game_settings.h"
#include "game/original/CameraObj.h"
#include "game/original/KeyFramer.h"
#include "player_fixture.h"
#include <functional>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool close(DVector a, DVector b)
    {
        auto d = vector_subtract(a, b);
        return vector_dot(d, d) < .0001f;
    }
    HOBJECT create(GameSession& g, const char* type, Properties p = {})
    {
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, p);
    }
    KeyFramer* framer(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<KeyFramer*>(g.server.HandleToObject(h));
    }
    HOBJECT path(GameSession& g, bool active = true, bool loop = false)
    {
        return create(g, "KeyFramer",
                      {{"Name", std::string("controller")},
                       {"ObjectName", std::string("target")},
                       {"BaseKeyName", std::string("k")},
                       {"StartActive", active},
                       {"Looping", loop}});
    }
    HOBJECT key(GameSession& g, unsigned i, DVector p, float time, Properties props = {})
    {
        props.emplace("Name", "k" + std::to_string(i));
        props.emplace("Pos", p);
        props.emplace("TimeStamp", time);
        return create(g, "Key", props);
    }
    bool command(GameSession& g, HOBJECT h, const char* value)
    {
        auto text = g.server.CreateString(value);
        auto m = g.server.StartMessageToObject(nullptr, h, 1000);
        auto written = g.server.WriteToMessageHString(m, text);
        auto sent = g.server.EndMessage(m);
        g.server.FreeString(text);
        return written == DE_OK && sent == DE_OK;
    }
    bool defaults(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto h = create(g, "KeyFramer");
        auto* f = framer(g, h);
        CHECK(f && !f->active && !f->looping && !f->initialized && g.objects.get(h)->scheduled);
        CHECK(command(g, h, "ON") && !f->active);
        CHECK(g.step(.01) && f->initialized && !f->error.empty());
        CHECK(!g.objects.get(h)->scheduled && f->keys.empty());
        CHECK(!g.objects.create(g.objects.find_class("KeyFramer"), {}, Creation::Restore));
        auto string = g.objects.create_props(g.objects.find_class("KeyFramer"), {},
                                             "ObjectName target; BaseKeyName k; StartActive true");
        CHECK(string && framer(g, string)->object_name.empty() && !framer(g, string)->start_active);
        g.server.set_game_settings(1, GAMETYPE_ACTION);
        auto filtered = create(g, "KeyFramer", {{"TypeActionMode", false}});
        CHECK(filtered && g.objects.get(filtered)->life == Life::Pending && g.step(.01) && !g.objects.get(filtered));
        CHECK(g.unload_world() && !g.server.string_count() && !g.server.list_count() && !g.server.message_count());
        return true;
    }
    bool timeline(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto target = create(g, "BaseClass", {{"Name", std::string("target")}, {"Pos", DVector{10, 100, 0}}});
        auto k0 = key(g, 0, {0, 100, 0}, 0), k1 = key(g, 1, {100, 100, 0}, 1), k3 = key(g, 3, {300, 100, 0}, 1);
        auto duplicate = key(g, 0, {0, 100, 0}, 0, {{"BPrintMessage", std::string("duplicate")}});
        auto wrong = create(g, "BaseClass", {{"Name", std::string("k0")}});
        auto h = path(g);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && f->keys.size() == 2 && f->active && f->time == 0 && f->processed == 0);
        CHECK(g.objects.get(k0) && !g.objects.get(k1) && g.objects.get(k3) && !g.objects.get(duplicate) &&
              g.objects.get(wrong) && !g.server.string_count() && f->keys[0].print == "duplicate");
        CHECK(g.step(.25) && close(g.objects.get(target)->state.m_Pos, {35, 100, 0}));
        CHECK(f->processed == 1 && f->targets[0].moves == 1);
        CHECK(command(g, h, "TRIGGER") && f->time == .25f); // Already active ON does not restart.
        CHECK(g.step(.25) && close(g.objects.get(target)->state.m_Pos, {60, 100, 0}));
        CHECK(g.step(.5) && !f->active && f->processed == 2 && f->time == 0);
        CHECK(close(g.objects.get(target)->state.m_Pos, {60, 100, 0})); // No final endpoint snap in source.
        CHECK(g.step(.01) && !g.objects.get(h)->scheduled);
        CHECK(command(g, h, "on") && g.step(.25) && close(g.objects.get(target)->state.m_Pos, {35, 100, 0}));
        CHECK(command(g, h, "OFF") && !f->active && !g.objects.get(h)->scheduled);
        CHECK(g.step(.25) && close(g.objects.get(target)->state.m_Pos, {35, 100, 0}));
        CHECK(g.unload_world() && !g.server.list_count() && !g.server.string_count());
        return true;
    }
    bool looping(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto target = create(g, "BaseClass", {{"Name", std::string("target")}, {"Pos", DVector{0, 100, 0}}});
        CHECK(key(g, 0, {0, 100, 0}, 0) && key(g, 1, {100, 100, 0}, 1));
        auto h = path(g, true, true);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && g.step(.25) && close(g.objects.get(target)->state.m_Pos, {25, 100, 0}));
        CHECK(g.step(3) && f->processed == 2 && f->active && f->time == 0 &&
              close(g.objects.get(target)->state.m_Pos, {25, 100, 0}));
        CHECK(g.step(.25) && f->processed == 3 && f->time == .25f);
        f->Pause();
        CHECK(g.step(.5) && !g.objects.get(h)->scheduled && f->time == .25f);
        f->Resume();
        CHECK(g.step(.5) && f->active && !g.objects.get(h)->scheduled &&
              f->time == .25f); // Source Resume doesn't rearm.
        CHECK(command(g, h, "OFF") && command(g, h, "ON") && g.step(.25) && f->time == .25f);
        return true;
    }
    bool rotation(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto a = create(g, "CameraObj", {{"Name", std::string("target")}, {"Pos", DVector{10, 100, 0}}});
        auto b = create(
            g, "BaseClass",
            {{"Name", std::string("target")}, {"Pos", DVector{30, 100, 0}}, {"Rotation", EulerAngles{{0, .3f, 0}}}});
        auto look = create(g, "BaseClass", {{"Pos", DVector{10, 100, 50}}});
        CHECK(dynamic_cast<CameraObj*>(g.server.HandleToObject(a))->SetLinkObject(look));
        CHECK(key(g, 0, {0, 100, 0}, 0) &&
              key(g, 1, {100, 100, 0}, 1, {{"Rotation", EulerAngles{{0, 1.5707963f, 0}}}}));
        auto h = path(g);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && g.step(.25));
        CHECK(close(rotate_vector({0, 0, 1}, g.objects.get(a)->state.m_Rotation),
                    {0, 0, 1})); // Linked camera keeps own rotation.
        CHECK(g.objects.remove(a) && g.objects.finish_frame() && f->targets.size() == 1 && f->targets[0].object == b);
        CHECK(g.step(.25) &&
              close(g.objects.get(b)->state.m_Pos, {80, 100, 0})); // Removal keeps second target's own offset.
        auto middle = rotation_from_euler({0, .78539815f, 0}), offset = rotation_from_euler({0, .3f, 0});
        DRotation expected{vector_add(middle.m_Vec, offset.m_Vec), middle.m_Spin + offset.m_Spin - 1};
        const float norm = std::sqrt(vector_dot(expected.m_Vec, expected.m_Vec) + expected.m_Spin * expected.m_Spin);
        expected = {vector_scale(expected.m_Vec, 1 / norm), expected.m_Spin / norm};
        CHECK(close(rotate_vector({0, 0, 1}, g.objects.get(b)->state.m_Rotation), rotate_vector({0, 0, 1}, expected)));
        CHECK(g.objects.remove(b) && g.objects.finish_frame() && f->targets.empty() && g.step(.1));
        return true;
    }
    bool events(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto camera = create(g, "CameraObj", {{"Name", std::string("target")}});
        CHECK(key(g, 0, {}, 0,
                  {{"MessageTarget", std::string("target")},
                   {"MessageName", std::string("ON")},
                   {"SoundName", std::string("sound.wav")},
                   {"SoundRadius", 123.0f},
                   {"LoopSound", true},
                   {"BPrintMessage", std::string("hello")}}));
        CHECK(key(g, 1, {}, 1, {{"MessageTarget", std::string("absent")}, {"MessageName", std::string("ON")}}));
        auto h = path(g);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && g.server.string_count() == 0);
        CHECK(g.step(.5) && dynamic_cast<CameraObj*>(g.server.HandleToObject(camera))->IsActive());
        CHECK(f->keys[0].sound_radius == 123 && f->keys[0].loop_sound && f->last_sound == "sound.wav" &&
              f->sound_requests == 1);
        CHECK(f->last_print == "hello" && f->print_requests == 1 && f->message_keys == 1 &&
              f->last_message_matches == 1);
        CHECK(g.step(.5) && f->unresolved_messages == 1 && f->message_keys == 2 && f->last_message_matches == 0);
        CHECK(!g.server.string_count() && !g.server.message_count() && !g.server.list_count());
        return true;
    }
    bool reentrant(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto target = create(g, "BaseClass", {{"Name", std::string("target")}});
        CHECK(key(g, 0, {}, 0, {{"MessageTarget", std::string("controller")}, {"MessageName", std::string("OFF")}}));
        CHECK(key(g, 1, {100, 0, 0}, 1));
        auto h = path(g);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && g.step(.5) && !f->active && !g.objects.get(h)->scheduled && f->processed == 1);
        CHECK(close(g.objects.get(target)->state.m_Pos, {}));
        f->keys[0].message = "REMOVE";
        CHECK(command(g, h, "ON") && g.step(.5) && !g.objects.get(h));
        CHECK(g.objects.get(target) && !g.server.string_count() && !g.server.message_count() && !g.server.list_count());
        return true;
    }
    bool validation(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = create(g, "BaseClass");
        DRotation out;
        const auto a = rotation_from_euler({0, 0, 0}), b = rotation_from_euler({0, 1.5707963f, 0});
        CHECK(g.server.InterpolateRotation(out, a, b, .5f) == DE_OK);
        CHECK(close(rotate_vector({0, 0, 1}, out), {.70710678f, 0, .70710678f}));
        const DRotation opposite{vector_scale(b.m_Vec, -1), -b.m_Spin};
        CHECK(g.server.InterpolateRotation(out, b, opposite, .5f) == DE_OK &&
              close(rotate_vector({0, 0, 1}, out), {1, 0, 0}));
        const auto saved = out;
        const DRotation DRotationZero{{}, 0};
        CHECK(g.server.InterpolateRotation(out, a, b, std::numeric_limits<float>::quiet_NaN()) == DE_INVALIDPARAMS);
        CHECK(g.server.InterpolateRotation(out, a, {{}, 0}, .5f) == DE_INVALIDPARAMS && out.m_Spin == saved.m_Spin);
        CHECK(g.server.MoveKeyframedObject(h, {10, 20, 30}, &b) == MoverResult::Moved);
        auto* r = g.objects.get(h);
        const auto pos = r->state.m_Pos;
        CHECK(g.server.MoveKeyframedObject(h, {1, 2, 3}, &DRotationZero) == MoverResult::Invalid);
        CHECK(close(r->state.m_Pos, pos));
        r->container = std::make_shared<ContainerShape>();
        CHECK(g.server.MoveKeyframedObject(h, {1, 2, 3}, &b) == MoverResult::Unavailable && close(r->state.m_Pos, pos));
        r->container.reset();
        r->world_model = std::make_shared<ContainerShape>();
        CHECK(g.server.MoveKeyframedObject(h, {1, 2, 3}, &b) == MoverResult::Unavailable && close(r->state.m_Pos, pos));
        r->world_model.reset();
        r->client_movement = true;
        CHECK(g.server.MoveKeyframedObject(h, {1, 2, 3}, &b) == MoverResult::Unavailable);
        r->client_movement = false;
        CHECK(g.objects.remove(h) && g.server.SetObjectRotation(h, &a) == DE_INVALIDPARAMS &&
              g.server.MoveKeyframedObject(h, {}, &b) == MoverResult::Invalid);
        CHECK(g.objects.finish_frame() && g.server.MoveKeyframedObject(h, {}, &b) == MoverResult::Invalid);
        return true;
    }
    bool limits(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        for (unsigned i = 0; i < 1025; ++i)
            CHECK(key(g, i, {}, 0));
        auto h = path(g);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && f->keys.empty() && f->error.find("1024") != std::string::npos &&
              g.objects.snapshot().size() == 1026);
        CHECK(g.objects.clear());
        auto k = key(g, 0, {}, 0, {{"SoundRadius", -1.0f}});
        h = path(g);
        f = framer(g, h);
        CHECK(g.step(.01) && !f->error.empty() && g.objects.get(k));
        CHECK(g.objects.clear());
        k = key(g, 0, {}, 0);
        h = create(g, "KeyFramer",
                   {{"Name", std::string("target")},
                    {"ObjectName", std::string("target")},
                    {"BaseKeyName", std::string("k")}});
        f = framer(g, h);
        CHECK(g.step(.01) && f->error.find("Self-target") != std::string::npos && g.objects.get(k));
        CHECK(g.objects.clear());
        for (unsigned i = 0; i < 257; ++i)
            CHECK(create(g, "BaseClass", {{"Name", std::string("target")}}));
        k = key(g, 0, {}, 0);
        h = path(g);
        f = framer(g, h);
        CHECK(g.step(.01) && f->error.find("256") != std::string::npos && g.objects.get(k));
        CHECK(g.unload_world() && !g.server.string_count() && !g.server.list_count());
        return true;
    }
    class TouchRemoval : public BaseClass
    {
      protected:
        DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT reason) override
        {
            if (message == MID_TOUCHNOTIFY)
                g_pServerDE->RemoveObject(m_hObject);
            return BaseClass::EngineMessageFn(message, data, reason);
        }
    };
    bool collision(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = create(g, "BaseClass", {{"Pos", DVector{0, 10, 0}}, {"Flags", std::int32_t(FLAG_SOLID)}});
        DVector dims{1, 1, 1};
        g.server.SetObjectDims(h, &dims);
        const auto rot = rotation_from_euler({0, .5f, 0});
        CHECK(g.server.MoveKeyframedObject(h, {0, -10, 0}, &rot) == MoverResult::Blocked);
        CHECK(std::abs(g.objects.get(h)->state.m_Pos.y - 1.001f) < .01f);
        CHECK(close(rotate_vector({0, 0, 1}, g.objects.get(h)->state.m_Rotation), rotate_vector({0, 0, 1}, rot)));
        static ClassInfo probe{"TouchRemoval",
                               nullptr,
                               []() -> std::unique_ptr<BaseClass> { return std::make_unique<TouchRemoval>(); },
                               {}};
        CHECK(g.objects.register_class(probe));
        ObjectCreateStruct desc;
        desc.m_Pos = {0, 100, 0};
        desc.m_Flags = FLAG_TOUCH_NOTIFY;
        auto removed = g.objects.create(&probe, desc);
        g.server.SetObjectDims(removed, &dims);
        auto target = create(g, "BaseClass", {{"Pos", DVector{10, 100, 0}}, {"Flags", std::int32_t(FLAG_SOLID)}});
        g.server.SetObjectDims(target, &dims);
        CHECK(g.server.MoveKeyframedObject(removed, {20, 100, 0}, &rot) == MoverResult::Invalid);
        CHECK(g.objects.get(removed)->life == Life::Pending && g.objects.get(removed)->state.m_Rotation.m_Spin == 1);
        CHECK(g.objects.finish_frame() && !g.objects.get(removed));
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        unsigned worlds = 0, controllers = 0, key_count = 0, copied = 0, empty = 0, missing = 0, failed = 0;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World world;
            CHECK(g.resources.read(path, bytes, error));
            if (!read_world(bytes, world, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            for (const auto& o : world.objects)
                if (o.class_name == "Key" || o.class_name == "KeyFramer" || o.class_name == "CameraObj")
                {
                    Properties props;
                    for (const auto& p : o.properties)
                        props.emplace(p.name, p.value);
                    auto h = create(g, o.class_name.c_str(), props);
                    CHECK(h);
                    if (o.class_name == "KeyFramer")
                        ++controllers;
                    else if (o.class_name == "Key")
                        ++key_count;
                }
            CHECK(g.step(.01));
            for (auto id : g.objects.snapshot())
                if (auto* f = dynamic_cast<KeyFramer*>(g.objects.get(id)->game.get()))
                {
                    CHECK(f->initialized);
                    copied += f->keys.size();
                    if (f->keys.empty())
                        ++empty;
                    if (f->targets.empty())
                        ++missing;
                    if (!f->error.empty() && f->error.find("No loaded targets") == std::string::npos &&
                        f->error.find("No contiguous keys") == std::string::npos)
                    {
                        ++failed;
                        std::cout << path << ": " << f->error << '\n';
                    }
                }
            CHECK(g.objects.clear() && !g.server.string_count() && !g.server.list_count());
        }
        std::cout << "worlds=" << worlds << " controllers=" << controllers << " source_keys=" << key_count
                  << " copied=" << copied << " empty_paths=" << empty << " missing_targets=" << missing
                  << " malformed=" << failed << '\n';
        CHECK(worlds == 57 && controllers == 94 && key_count == 383 && copied == 375 && !empty && missing == 44 &&
              !failed);
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            const float entered = g.server.GetTime();
            CHECK(g.objects.snapshot().size() == 174);
            auto* list = g.server.FindNamedObjects("tracking1");
            CHECK(list && list->m_pFirstLink);
            auto h = list->m_pFirstLink->m_hObject;
            g.server.RelinquishList(list);
            auto* f = framer(g, h);
            CHECK(g.step(1.0 / 60) && f->keys.size() == 2 && f->targets.size() == 1 && f->active && f->time == 0);
            CHECK(g.objects.snapshot().size() == 165);
            auto camera = f->targets[0].object;
            auto start = g.objects.get(camera)->state.m_Pos;
            CHECK(g.step(1.0 / 60) && f->targets[0].moves == 1 && f->processed == 1 && f->unresolved_messages == 1);
            CHECK(!close(start, g.objects.get(camera)->state.m_Pos));
            auto expected =
                vector_add(f->keys[0].position, vector_scale(vector_subtract(f->keys[1].position, f->keys[0].position),
                                                             f->time / (f->keys[1].time - f->keys[0].time)));
            expected = vector_add(expected, f->targets[0].offset);
            CHECK(close(expected, g.objects.get(camera)->state.m_Pos));
            CHECK(g.client_shell.view.object == camera && close(expected, g.client_shell.view.position));
            bool second = false;
            for (int i = 0; i < 1800 && g.client_shell.view.object; ++i)
            {
                const auto* view = g.objects.get(g.client_shell.view.object);
                second |= view && std::string_view(view->state.m_Name) == "camera2";
                CHECK(g.step(1.0 / 60));
            }
            CHECK(second && !g.client_shell.view.object && g.server.GetTime() - entered > 12 &&
                  g.server.GetTime() - entered < 12.1f);
            auto* trains = g.server.FindNamedObjects("htrain1");
            CHECK(trains && trains->m_pFirstLink);
            auto train = trains->m_pFirstLink->m_hObject;
            g.server.RelinquishList(trains);
            auto* tf = framer(g, train);
            CHECK(tf && tf->targets.size() == 1);
            auto original = g.objects.get(tf->targets[0].object)->state.m_Pos;
            CHECK(command(g, train, "ON") && g.step(.1) && tf->targets[0].attempts &&
                  tf->targets[0].result == MoverResult::Moved);
            CHECK(!close(original, g.objects.get(tf->targets[0].object)->state.m_Pos));
            CHECK(g.unload_world() && !g.server.string_count() && !g.server.list_count() && !g.server.message_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"defaults", defaults},   {"timeline", timeline},   {"looping", looping},       {"rotation", rotation},
        {"events", events},       {"reentrant", reentrant}, {"validation", validation}, {"limits", limits},
        {"collision", collision}, {"catalog", catalog}};
    auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
