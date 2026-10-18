#include "../src/app/sprite_scene.h"
#include "game/original/BuiltinSprite.h"
#include "game/world_preparation.h"
#include "sprite_view_fixture.h"
#include <cmath>
#include <iostream>
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
    bool close(float a, float b)
    {
        return std::abs(a - b) < .0001f;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        if (!test_player::setup(g, root, false) || !test_sprite_view::mount(g.resources, root, error))
            return false;
        test_model::Writer spr;
        for (auto v : {1u, 10u, 1u, 0u, 0u}) // one frame, 10 fps, transparent, opaque, key 0
            spr.u32(v);
        const std::string frame = "alpha.dtx";
        spr.u16(frame.size());
        for (char c : frame)
            spr.u8(c);
        test_player::Directory dir;
        dir.children["sprites"].files["lamp.spr"] = spr.b;
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream out(root / "lamp.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return g.resources.mount(root / "lamp.rez", error);
    }
    HOBJECT make(GameSession& g, Properties props = {}, Creation reason = Creation::World)
    {
        props.try_emplace("Filename", std::string("sprites\\lamp.spr"));
        return g.objects.create(g.objects.find_class("Sprite"), {}, reason, props);
    }
    BuiltinSprite* get(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<BuiltinSprite*>(r->game.get()) : nullptr;
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        // Registration defaults: Flags FLAG_VISIBLE, Scale 1, white, Alpha 1.
        auto h = make(g);
        auto* r = g.objects.get(h);
        CHECK(r && r->life == Life::Alive && r->state.m_ObjectType == OT_SPRITE && r->sprite);
        CHECK(r->state.m_Flags == FLAG_VISIBLE && r->state.m_Scale.x == 1 && r->state.m_Scale.z == 1);
        CHECK(get(g, h)->color_applied && get(g, h)->error.empty());
        CHECK(r->color.x == 1 && r->color.y == 1 && r->color.z == 1 && r->alpha == 1);
        CHECK(!r->scheduled); // Neither callback schedules updates.
        // Source quirk: blue uses the green byte; float colors truncate to bytes.
        auto tinted = make(g, {{"Color", DVector{200.9f, 51, 255}},
                               {"Alpha", .5f},
                               {"Scale", DVector{.25f, .5f, 2}},
                               {"Flags", std::int32_t(FLAG_VISIBLE | FLAG_ROTATEABLESPRITE)}});
        auto* s = get(g, tinted);
        r = g.objects.get(tinted);
        CHECK(s && s->m_Color[0] == 200 && s->m_Color[1] == 51 && s->m_Color[2] == 255 && s->m_Color[3] == 127);
        CHECK(close(r->color.x, 200 / 255.f) && close(r->color.y, .2f) && close(r->color.z, .2f));
        CHECK(close(r->alpha, 127 / 255.f));
        CHECK(r->state.m_Scale.x == .25f && r->state.m_Scale.y == .5f && r->state.m_Scale.z == 2);
        CHECK(r->state.m_Flags == (FLAG_VISIBLE | FLAG_ROTATEABLESPRITE));
        // Inherited BaseClass boolean overrides still run after Flags.
        auto hidden = make(g, {{"Visible", false}, {"Solid", true}});
        CHECK(hidden && !(g.objects.get(hidden)->state.m_Flags & FLAG_VISIBLE) &&
              (g.objects.get(hidden)->state.m_Flags & FLAG_SOLID));
        // Runtime creation also reads the class defaults (Scale overrides the structure).
        ObjectCreateStruct state;
        SAFE_STRCPY(state.m_Filename, "sprites\\lamp.spr");
        state.m_Scale = {3, 3, 3};
        auto runtime = g.objects.create(g.objects.find_class("Sprite"), state);
        CHECK(runtime && g.objects.get(runtime)->state.m_Scale.x == 1 && get(g, runtime)->m_Color[3] == 255);
        // Precreate type is forced even when the structure requests another type.
        state.m_ObjectType = OT_MODEL;
        auto forced = g.objects.create(g.objects.find_class("Sprite"), state);
        CHECK(forced && g.objects.get(forced)->state.m_ObjectType == OT_SPRITE);
        return true;
    }
    bool rejects(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        const float nan = std::numeric_limits<float>::quiet_NaN();
        CHECK(!make(g, {{"Color", DVector{256, 0, 0}}}));
        CHECK(!make(g, {{"Color", DVector{-1, 0, 0}}}));
        CHECK(!make(g, {{"Color", DVector{0, nan, 0}}}));
        CHECK(!make(g, {{"Alpha", 1.01f}}));
        CHECK(!make(g, {{"Alpha", -.1f}}));
        CHECK(!make(g, {{"Scale", DVector{nan, 1, 1}}}));
        CHECK(!make(g, {{"Filename", std::string("sprites\\missing.spr")}}));
        CHECK(!make(g, {{"Unknown", 1.0f}}));
        CHECK(make(g, {{"Alpha", 0.0f}, {"Color", DVector{0, 0, 0}}})); // Boundaries are valid.
        return true;
    }
    bool scene(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto lamp = make(g, {{"Pos", DVector{0, 50, 100}}, {"Alpha", .5f}, {"Color", DVector{255, 0, 255}}});
        auto hidden = make(g, {{"Pos", DVector{0, 50, 120}}, {"Visible", false}});
        CHECK(lamp && hidden);
        SpriteView view;
        std::string e;
        CHECK(view.begin_frame({}, {}, e));
        CHECK(update_sprite_scene(g, view, e));
        unsigned found = 0;
        for (const auto& entry : view.entries)
        {
            if (entry.object == g.objects.get(lamp)->id)
            {
                ++found;
                CHECK(entry.visible && entry.error.empty() && close(entry.alpha, 127 / 255.f) && !entry.rotatable);
                CHECK(entry.width == 4 && entry.height == 2 && entry.range != UINT32_MAX);
            }
            CHECK(entry.object != g.objects.get(hidden)->id || entry.range == UINT32_MAX);
        }
        CHECK(found == 1);
        // Pending removal stops presentation immediately; drawing never advances clocks.
        CHECK(g.objects.get(lamp)->sprite_track.milliseconds == 0);
        CHECK(g.objects.remove(lamp));
        CHECK(view.begin_frame({}, {}, e) && update_sprite_scene(g, view, e));
        for (const auto& entry : view.entries)
            CHECK(!g.objects.get(entry.object) || g.objects.get(entry.object)->life == Life::Alive);
        g.objects.finish_frame();
        CHECK(!g.objects.get(lamp));
        CHECK(g.unload_world());
        return true;
    }
    bool editor_only()
    {
        // CF_NORUNTIME in engineobjects.c: preprocessor input, never a runtime instance.
        CHECK(editor_only_world_class("FastApproxArea") && editor_only_world_class("OutsideDef"));
        CHECK(!editor_only_world_class("Sprite") && !editor_only_world_class("Spawner"));
        GameSession g;
        CHECK(g.initialize() && !g.objects.find_class("FastApproxArea"));
        return true;
    }
    bool private_world(const char* path)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = path;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("05_cdmb", e));
            unsigned sprites = 0, approx = 0;
            for (std::size_t i = 0; i < g.entries.size(); ++i)
            {
                const auto& name = g.world.objects[i].class_name;
                if (name == "FastApproxArea")
                    approx += g.entries[i].state == EntryState::EditorOnly;
                if (name != "Sprite")
                    continue;
                CHECK(g.entries[i].state == EntryState::Created);
                const auto* r = g.objects.get(g.entries[i].object);
                CHECK(r && r->sprite && r->state.m_Flags == FLAG_VISIBLE && r->alpha == 1 && r->color.z == 1);
                const auto* s = dynamic_cast<const BuiltinSprite*>(r->game.get());
                CHECK(s && s->color_applied && s->m_Color[0] == 255 && s->m_Color[3] == 255);
                const auto scale = r->state.m_Scale.x;
                CHECK(scale == .15f || scale == .25f || scale == 1);
                for (const auto& frame : r->sprite->frames)
                    CHECK(g.resources.find(frame));
                ++sprites;
            }
            CHECK(sprites == 28 && approx == 1);
            SpriteView view;
            CHECK(view.begin_frame({}, {}, e) && update_sprite_scene(g, view, e));
            unsigned prepared = 0;
            for (const auto& entry : view.entries)
                if (const auto* r = g.objects.get(entry.object); r && dynamic_cast<const BuiltinSprite*>(r->game.get()))
                {
                    CHECK(entry.error.empty());
                    prepared += entry.range != UINT32_MAX;
                }
            std::cout << "05_CDMB built-in sprites: " << sprites << " created, " << prepared << " quads prepared\n";
            CHECK(prepared == 28);
            CHECK(g.unload_world());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string mode = argv[1];
    if (mode == "editor_only")
        return editor_only() ? 0 : 1;
    if (argc != 3)
        return 2;
    return (mode == "rules"     ? rules(argv[2])
            : mode == "rejects" ? rejects(argv[2])
            : mode == "scene"   ? scene(argv[2])
                                : mode == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
