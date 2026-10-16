#include "brush_fixture.h"
#include "engine/model_commands.h"
#include "game/original/Sparam.h"
#include "player_fixture.h"
#include <cmath>
#include <functional>
#include <set>
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
    bool near(float a, float b)
    {
        return std::abs(a - b) < .002f;
    }
    CBaseCharacter* character(GameSession& g, DVector pos = {})
    {
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = FLAG_MODELKEYS | FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), s);
        return h ? dynamic_cast<CBaseCharacter*>(g.objects.get(h)->game.get()) : nullptr;
    }
    bool key(GameSession& g, HOBJECT h, std::string text)
    {
        std::vector<std::vector<std::string>> commands;
        if (!model_commands(text, commands))
            return false;
        for (auto& command : commands)
        {
            std::vector<char*> args;
            for (auto& a : command)
                args.push_back(a.data());
            ArgList list{args.data(), int(args.size())};
            if (!g.objects.engine_message(h, MID_MODELSTRINGKEY, &list))
                return false;
        }
        return true;
    }
    bool world(GameSession& g)
    {
        auto m = test_brush::box("wall", {10, -100, -100}, {20, 100, 100});
        for (auto& s : m.surfaces)
            s.flags = 1;
        std::string error;
        CHECK(g.collision.build(m, error));
        g.server.collision_ready = true;
        return true;
    }
    bool lookup(const std::filesystem::path&)
    {
        std::string_view v = "unchanged";
        CHECK(Sparam_Get("x[RotY:2][RotY:9]", "RotY", v) == SparamStatus::Found && v == "2");
        CHECK(Sparam_Get("[roty:2]", "RotY", v) == SparamStatus::Missing && v == "2");
        CHECK(Sparam_Get("[x: leading ; spaces ]", "x", v) == SparamStatus::Found && v == " leading ; spaces ");
        const auto old = v;
        for (auto s : {"[x:]", "[x:2", "[x:"})
            CHECK(Sparam_Get(s, "x", v) == SparamStatus::Invalid && v == old);
        CHECK(Sparam_Get(std::string(4097, 'x'), "x", v) == SparamStatus::Invalid);
        CHECK(Sparam_Get(std::string("[x:2]\0[x:3]", 11), "x", v) == SparamStatus::Invalid);
        CHECK(Sparam_Get("[x:" + std::string(256, 'a') + "]", "x", v) == SparamStatus::Invalid);
        CHECK(Sparam_Get("[x:2]", "x:", v) == SparamStatus::Invalid);
        return true;
    }
    bool motion(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* c = character(g);
        CHECK(c);
        auto* r = g.objects.get(c->m_hObject);
        CHECK(key(g, c->m_hObject, "[RotY:1.57079632679][ForwVel:120]"));
        CHECK(near(r->velocity.x, 120) && near(r->velocity.z, 0) && c->motion_applied == 3);
        CHECK(key(g, c->m_hObject, "[UpVel:3][RightVel:20][ForwVel:100]"));
        CHECK(near(r->velocity.x, 0) && near(r->velocity.y, 3) && near(r->velocity.z, 0) && c->motion_applied == 14);
        CHECK(key(g, c->m_hObject, "[RightVel:-20][RightVel:90]"));
        CHECK(near(r->velocity.z, 20));
        auto tilted = rotation_from_euler({0, 0, 1.57079632679f});
        CHECK(g.server.SetObjectRotation(c->m_hObject, &tilted) == DE_OK);
        CHECK(key(g, c->m_hObject, "[RotY:1.57079632679][ForwVel:10]"));
        CHECK(near(r->velocity.x, 0) && near(r->velocity.y, 10) &&
              near(r->velocity.z, 0)); // local up axis, not world yaw
        CHECK(key(g, c->m_hObject, "[UpVel:+1e2]"));
        CHECK(near(r->velocity.x, -100));
        CHECK(c->motion_failures == 0 && c->motion_keys == 5 && c->motion_actions == 9);
        CHECK(key(g, c->m_hObject, "unknown [ForwVel:8]")); // Source only sees argv[0].
        CHECK(near(r->velocity.x, -100) && c->pending_model_commands.size() == 1);
        return true;
    }
    bool rejection(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* c = character(g);
        CHECK(c);
        auto* r = g.objects.get(c->m_hObject);
        for (const auto* value : {"nan", "inf", "1e100", "1bad", "", "--1", "++1", "+-1", "10000001"})
        {
            CHECK(key(g, c->m_hObject, "[RotY:1][ForwVel:" + std::string(value) + "]"));
            CHECK(c->motion_applied == 0 && r->state.m_Rotation.m_Spin == 1 && !c->motion_error.empty());
        }
        for (const auto* text : {"[RotY:1][AddX:3]", "[DimsX:2][DimsY:3]", "[DimsX:2][DimsY:-1][DimsZ:2]"})
        {
            CHECK(key(g, c->m_hObject, text));
            CHECK(c->motion_applied == 0 && r->state.m_Rotation.m_Spin == 1);
        }
        CHECK(c->motion_failures == 12);
        CHECK(key(g, c->m_hObject, "[ForwVel:4][AddX:5][AddY:0][AddZ:0][DimsX:3][DimsY:4][DimsZ:5]"));
        CHECK(c->motion_applied == 2 && r->velocity.z == 4 && r->dimensions.x == 0 && r->state.m_Pos.x == 0);
        CHECK(!c->motion_error.empty()); // Missing world fails actual displacement, not successful stub.
        CHECK(g.objects.remove(c->m_hObject));
        DVector dims{1, 2, 3};
        CHECK(g.server.SetObjectDims(c->m_hObject, &dims) != DE_OK && r->dimensions.x == 0);
        CHECK(!key(g, c->m_hObject, "[DimsX:1][DimsY:2][DimsZ:3]"));
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool collision(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize() && world(g));
        auto* c = character(g, {0, 5, 0});
        CHECK(c);
        auto* r = g.objects.get(c->m_hObject);
        DVector dims{1, 1, 1};
        CHECK(g.server.SetObjectDims(c->m_hObject, &dims) == DE_OK);
        CHECK(key(g, c->m_hObject, "[AddX:30][AddY:0][AddZ:0][DimsX:2][DimsY:3][DimsZ:4]"));
        CHECK(r->state.m_Pos.x > 8.9f && r->state.m_Pos.x <= 9.001f && r->contact_polygon != UINT32_MAX);
        CHECK(r->state.m_Pos.y == 5 && r->dimensions.x == 2 && r->dimensions.y == 3 && c->motion_applied == 48);
        // Raw SetObjectDims preserves center and permits overlap, unlike player ResizeObject.
        CHECK(c->motion_failures == 0);
        CHECK(key(g, c->m_hObject, "[AddX:1][AddY:0][AddZ:0][DimsX:1][DimsY:1][DimsZ:1]"));
        CHECK(c->motion_applied == 0 && c->motion_failures == 1 && r->dimensions.x == 2);
        return true;
    }
    unsigned removed_keys = 0, removed_actions = 0, removed_failures = 0;
    class RemovingCharacter : public CBaseCharacter
    {
      public:
        ~RemovingCharacter() override
        {
            removed_keys = motion_keys;
            removed_actions = motion_actions;
            removed_failures = motion_failures;
        }

      private:
        DDWORD EngineMessageFn(DDWORD m, void* p, DFLOAT f) override
        {
            if (m == MID_TOUCHNOTIFY)
            {
                g_pServerDE->RemoveObject(m_hObject);
            }
            return CBaseCharacter::EngineMessageFn(m, p, f);
        }
    };
    bool callbacks(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize() && world(g));
        ClassInfo type{"RemovingCharacter",
                       g.objects.find_class("CBaseCharacter"),
                       []() -> std::unique_ptr<BaseClass> { return std::make_unique<RemovingCharacter>(); },
                       {}};
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Flags = FLAG_MODELKEYS | FLAG_TOUCH_NOTIFY;
        const auto h = g.objects.create(&type, state);
        auto* r = g.objects.get(h);
        CHECK(r);
        auto* c = dynamic_cast<CBaseCharacter*>(r->game.get());
        CHECK(c);
        auto m = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *m, error));
        m->animations[0].keys[0].text = "[ForwVel:5];[AddX:4][AddY:0][AddZ:0][DimsX:2][DimsY:3][DimsZ:4];[UpVel:99]";
        for (size_t i = 1; i < m->animations[0].keys.size(); ++i)
            m->animations[0].keys[i].text.clear();
        r->model = m;
        ObjectCreateStruct sensor;
        sensor.m_Pos = {2, 0, 0};
        sensor.m_Flags = FLAG_TOUCH_NOTIFY;
        auto other = g.objects.create(g.objects.find_class("BaseClass"), sensor);
        CHECK(other);
        DVector dims{.5f, .5f, .5f};
        CHECK(g.server.SetObjectDims(other, &dims) == DE_OK);
        CHECK(g.server.advance_models(0));
        CHECK(!g.objects.get(h)); // Touch removed owner; later action/key may not write pending/stale state.
        CHECK(removed_keys == 2 && removed_actions == 2 && removed_failures == 1);
        CHECK(g.objects.get(other));
        auto* live = character(g, {0, 10, 0});
        CHECK(live);
        r = g.objects.get(live->m_hObject);
        r->model = m;
        r->activity = OBJSTATE_INACTIVE;
        CHECK(g.server.advance_models(0) && live->motion_keys == 0);
        r->activity = OBJSTATE_ACTIVE;
        CHECK(g.server.advance_models(0));
        CHECK(live->motion_keys == 3 && live->motion_actions == 4 && r->velocity.y == 99 && r->dimensions.y == 3);
        CHECK(m->animations[0].keys[0].text.find("[UpVel:99]") != std::string::npos); // Immutable resource.
        const auto old = live->m_hObject;
        CHECK(g.unload_world() && !g.objects.get(old));
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool player(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("start")}, {"Pos", DVector{32, 100, 64}}}));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        const auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
        CHECK(key(g, p->m_hObject, "[RotY:1][ForwVel:99][DimsX:9][DimsY:9][DimsZ:9]"));
        CHECK(p->motion_keys == 0 && p->pending_model_commands.size() == 1);
        CHECK(g.objects.get(p->m_hObject)->state.m_Pos.x == pos.x);
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
        const char* paths[]{"models/characters/caleb.abc",    "models/enemies/m_civilian1.abc",
                            "models/enemies/f_civilian1.abc", "models/enemies/labtech.abc",
                            "models/enemies/m_cultist1.abc",  "models/enemies/f_cultist1.abc",
                            "models/enemies/gideon.abc"};
        size_t total_clips = 0, total_keys = 0, total_brackets = 0;
        for (auto path : paths)
        {
            std::shared_ptr<const Model> m;
            CHECK(g.models.load(path, m, error));
            std::map<std::string, unsigned> keys;
            size_t count = 0;
            for (const auto& a : m->animations)
                for (const auto& k : a.keys)
                    if (!k.text.empty())
                    {
                        ++keys[k.text];
                        ++count;
                    }
            std::cout << path << " clips=" << m->animations.size() << " keys=" << count << " unique=" << keys.size()
                      << "\n";
            for (const auto& [k, n] : keys)
                std::cout << n << " " << k << "\n";
            auto* c = character(g);
            CHECK(c);
            auto* r = g.objects.get(c->m_hObject);
            r->model = m;
            size_t delivered = 0;
            for (unsigned i = 0; i < m->animations.size(); ++i)
            {
                r->animation.animation = i;
                r->animation.reset();
                r->animation.looping = false;
                c->pending_model_commands.clear();
                CHECK(g.server.advance_models(m->animations[i].duration / 1000.0));
                size_t expected = 0;
                for (const auto& k : m->animations[i].keys)
                    if (!k.text.empty())
                    {
                        ++expected;
                        total_brackets += k.text.front() == '[';
                        std::string_view v;
                        for (const auto* name : {"RotY", "ForwVel", "RightVel", "UpVel", "AddX", "DimsX"})
                            CHECK(Sparam_Get(k.text, name, v) == SparamStatus::Missing);
                    }
                CHECK(expected <= 32 && c->pending_model_commands.size() == expected);
                delivered += expected;
            }
            CHECK(c->motion_keys == 0 && c->motion_failures == 0 && delivered == count);
            total_clips += m->animations.size();
            total_keys += delivered;
            const auto old = c->m_hObject;
            CHECK(g.objects.remove(old) && g.objects.finish_frame() && !g.objects.get(old));
        }
        CHECK(total_clips == 326 && total_keys == 193 && total_brackets == 147);
        std::cout << "Actual callbacks: clips=" << total_clips << " keys=" << total_keys
                  << " bracket keys=" << total_brackets
                  << " motion keys=0 (AI sound/fire/extra keys remain unavailable)\n";
        CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"lookup", lookup},       {"motion", motion}, {"rejection", rejection}, {"collision", collision},
        {"callbacks", callbacks}, {"player", player}, {"catalog", catalog}};
    auto i = cases.find(argv[1]);
    return i != cases.end() && i->second(argv[2]) ? 0 : 1;
}
