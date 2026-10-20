#include "game/original/DetailSprite.h"
#include "player_fixture.h"
#include <cmath>
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
    std::vector<std::uint8_t> spr(unsigned count = 3, unsigned fps = 10, std::string name = "frame.dtx")
    {
        test_model::Writer w;
        w.u32(count);
        w.u32(fps);
        w.u32(1);
        w.u32(0);
        w.u32(0xff00ff);
        for (unsigned i = 0; i < count; ++i)
        {
            w.u16(name.size());
            for (char c : name)
                w.u8(c);
        }
        return w.b;
    }
    bool setup(GameSession& g, const std::filesystem::path& root, std::string archive = "sprite.rez", unsigned fps = 10)
    {
        if (g.resources.mounts().empty() && !test_player::setup(g, root, false))
            return false;
        test_player::Directory dir;
        dir.files["sprite.spr"] = spr(3, fps);
        dir.files["missing.spr"] = spr(1, 10, "absent.dtx");
        dir.files["bad.spr"] = {0, 1, 2};
        dir.files["frame.dtx"] = {0}; // Presence only, no decoder is claimed.
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream f(root / archive, std::ios::binary);
        f.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        f.close();
        std::string error;
        return g.resources.mount(root / archive, error);
    }
    HOBJECT make(GameSession& g, Properties props = {})
    {
        props.try_emplace("Filename", std::string("sprite.spr"));
        return g.objects.create(g.objects.find_class("DetailSprite"), {}, Creation::World, props);
    }
    DetailSprite* get(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<DetailSprite*>(r->game.get()) : nullptr;
    }
    bool reader(const std::filesystem::path&)
    {
        auto b = spr();
        Sprite s;
        std::string error;
        CHECK(read_sprite(b, s, error));
        CHECK(s.frames.size() == 3 && s.fps == 10 && s.transparent && !s.translucent && s.color_key == 0xff00ff);
        for (size_t n = 0; n < b.size(); ++n)
        {
            CHECK(!read_sprite(std::span(b).first(n), s, error));
            CHECK(s.frames.size() == 3);
        }
        b.push_back(0);
        CHECK(!read_sprite(b, s, error));
        for (auto count : {0u, 4097u})
            CHECK(!read_sprite(spr(count), s, error));
        CHECK(!read_sprite(spr(1, 0), s, error));
        CHECK(read_sprite(spr(5, 65537), s, error) && s.fps == 65537);
        for (auto name : {"", "../bad.dtx", "/abs.dtx", "a/../../bad.dtx", "a.wav"})
            CHECK(!read_sprite(spr(1, 10, name), s, error));
        b = spr();
        b[8] = 2;
        CHECK(!read_sprite(b, s, error));
        b = spr();
        b[12] = 2;
        CHECK(!read_sprite(b, s, error));
        b = spr();
        b[22] = 0;
        CHECK(!read_sprite(b, s, error));
        CHECK(!read_sprite(spr(1, 10, std::string(1001, 'a') + ".dtx"), s, error));
        b.resize(1024 * 1024 + 1);
        CHECK(!read_sprite(b, s, error));
        CHECK(read_sprite(spr(4096, 1000), s, error));
        return true;
    }
    bool clock_test(const std::filesystem::path&)
    {
        Sprite s;
        std::string error;
        CHECK(read_sprite(spr(), s, error));
        SpriteTrack t;
        CHECK(advance_sprite(s, t, .099) && t.frame == 0);
        CHECK(advance_sprite(s, t, .002) && t.frame == 1 && std::abs(t.milliseconds - 101) < 1e-8);
        CHECK(advance_sprite(s, t, .2) && t.frame == 0 && std::abs(t.milliseconds - 1) < 1e-8);
        CHECK(advance_sprite(s, t, 60) && t.frame == 0 && std::abs(t.milliseconds - 1) < 1e-8);
        CHECK(!advance_sprite(s, t, -1) && !advance_sprite(s, t, 61) && t.milliseconds == 1);
        CHECK(!advance_sprite(s, t, std::numeric_limits<double>::quiet_NaN()));
        CHECK(!advance_sprite(s, t, std::numeric_limits<double>::infinity()) && t.milliseconds == 1);
        CHECK(read_sprite(spr(3, 15), s, error));
        t = {};
        for (unsigned i = 0; i < 60; ++i)
            CHECK(advance_sprite(s, t, 1.0 / 60));
        CHECK(t.frame == 0 && std::abs(t.milliseconds - 10) < 1e-7); // 3*66ms, preserving fractional ticks.
        CHECK(read_sprite(spr(5, 65537), s, error));
        t = {};
        CHECK(t.has_frame && t.frame == 0);
        CHECK(advance_sprite(s, t, .01) && !t.has_frame && t.frame == 0 && t.milliseconds == 0);
        CHECK(advance_sprite(s, t, 60) && !t.has_frame);
        return true;
    }
    bool properties(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = make(g);
        auto* r = g.objects.get(h);
        auto* c = get(g, h);
        CHECK(r && c && r->state.m_ObjectType == OT_SPRITE && r->sprite && !r->scheduled && (r->user_flags & 8));
        CHECK(c->rotatable && !c->flush && r->state.m_Scale.x == 1 && (r->state.m_Flags & FLAG_ROTATEABLESPRITE));
        auto other = make(g, {{"ScaleX", -2.0f},
                              {"ScaleY", 0.0f},
                              {"Chromakey", true},
                              {"Rotatable", false},
                              {"Repeat", true},
                              {"RepeatX", std::int32_t(3)}});
        r = g.objects.get(other);
        CHECK(r && r->state.m_Scale.x == -2 && r->state.m_Scale.y == 0 && !(r->state.m_Flags & FLAG_ROTATEABLESPRITE) &&
              (r->state.m_Flags & FLAG_SPRITECHROMAKEY));
        CHECK(get(g, other)->children.empty() && r->sprite == g.objects.get(h)->sprite);
        ObjectCreateStruct state;
        legacy_copy(state.m_Filename, "sprite.spr");
        state.m_Scale = {2, 3, 4};
        auto runtime = g.objects.create(g.objects.find_class("DetailSprite"), state, Creation::Runtime);
        CHECK(runtime && !get(g, runtime)->rotatable && get(g, runtime)->filename.empty() &&
              g.objects.get(runtime)->state.m_Scale.z == 4);
        auto str = g.objects.create_props(g.objects.find_class("DetailSprite"), {},
                                          "Filename sprite.spr; ScaleY 4; Rotatable false");
        CHECK(str && g.objects.get(str)->state.m_Scale.y == 4 && !get(g, str)->rotatable);
        CHECK(g.server.advance_models(.15) && g.objects.get(h)->sprite_track.frame == 1);
        g.objects.get(h)->state.m_Flags = 0;
        CHECK(g.server.advance_models(.1) && g.objects.get(h)->sprite_track.frame == 2);
        g.objects.get(h)->activity = OBJSTATE_INACTIVE;
        CHECK(g.server.advance_models(.1) && g.objects.get(h)->sprite_track.frame == 2);
        CHECK(g.objects.remove(h) && g.server.advance_models(.1) && !g.objects.get(h));
        return true;
    }
    bool placement(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto rotation = EulerAngles{{1.5707963268f, 0, 0}};
        auto h = make(g, {{"Pos", DVector{0, 10, 0}},
                          {"Rotation", rotation},
                          {"FlushWithWorld", true},
                          {"Repeat", true},
                          {"RepeatX", std::int32_t(2)},
                          {"RepeatY", std::int32_t(3)},
                          {"SpacingX", 2.0f},
                          {"SpacingY", 3.0f},
                          {"ScaleX", .5f}});
        auto* c = get(g, h);
        auto* r = g.objects.get(h);
        CHECK(c && c->error.empty());
        CHECK(c->flush_result == SegmentResult::Hit && std::abs(r->state.m_Pos.y - .25f) < .001f &&
              c->children.size() == 5);
        CHECK(r->state.m_Rotation.m_Vec.x == rotation_from_euler(rotation.radians).m_Vec.x);
        auto origin = r->state.m_Pos;
        auto up = rotate_vector({0, 3, 0}, r->state.m_Rotation);
        auto right = rotate_vector({2, 0, 0}, r->state.m_Rotation);
        std::vector<DVector> expected{vector_add(origin, up), vector_add(origin, vector_scale(up, 2)),
                                      vector_add(origin, right), vector_add(vector_add(origin, right), up),
                                      vector_add(vector_add(origin, right), vector_scale(up, 2))};
        auto children = c->children;
        for (size_t i = 0; i < children.size(); ++i)
        {
            auto* cr = g.objects.get(children[i]);
            auto* d = get(g, children[i]);
            CHECK(cr && d && !d->flush && !d->repeat && d->filename.empty() && cr->reason == Creation::Runtime &&
                  cr->state.m_Scale.x == .5f && cr->sprite == r->sprite);
            auto delta = vector_subtract(cr->state.m_Pos, expected[i]);
            CHECK(vector_dot(delta, delta) < .00001f);
        }
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        for (auto child : children)
            CHECK(g.objects.get(child));
        // Source repeats even when the ray misses, and RepeatX=0 retains the first Y column.
        h = make(g, {{"Pos", DVector{2000, 10, 0}},
                     {"FlushWithWorld", true},
                     {"Repeat", true},
                     {"RepeatX", std::int32_t(0)},
                     {"RepeatY", std::int32_t(3)}});
        CHECK(get(g, h)->flush_result == SegmentResult::Miss && get(g, h)->children.size() == 2);
        h = make(
            g,
            {{"FlushWithWorld", true}, {"Repeat", true}, {"RepeatX", std::int32_t(3)}, {"RepeatY", std::int32_t(0)}});
        CHECK(get(g, h)->children.empty());
        // B2 filtering runs AFTER DetailSprite creates independent runtime children.
        h = make(g,
                 {{"FlushWithWorld", true}, {"Repeat", true}, {"RepeatY", std::int32_t(2)}, {"DifficultyMed", false}});
        c = get(g, h);
        CHECK(c && c->filtered && c->children.size() == 1);
        auto child = c->children[0];
        CHECK(g.objects.finish_frame() && !g.objects.get(h) && g.objects.get(child));
        return true;
    }
    bool ray(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        SegmentHit hit;
        ObjectCreateStruct state;
        state.m_Pos = {0, 5, 0};
        state.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
        g.objects.get(h)->dimensions = {2, 2, 2};
        CHECK(g.server.CastWorldRay({0, 10, 0}, {0, -2, 0}, hit) == SegmentResult::Hit && !hit.object &&
              std::abs(hit.point.y) < .001f && hit.normal.y > 0);
        CHECK(g.server.CastWorldRay({0, 10, 0}, {0, 1, 0}, hit) == SegmentResult::Miss);
        CHECK(g.server.CastWorldRay({0, 10, 0}, {}, hit) == SegmentResult::Invalid);
        CHECK(g.server.CastWorldRay({1e8f, 0, 0}, {1, 0, 0}, hit) == SegmentResult::Invalid);
        WorldModel nonsolid;
        nonsolid.points = {{-1000, 0, -1000}, {1000, 0, -1000}, {1000, 0, 1000}, {-1000, 0, 1000}};
        nonsolid.surfaces.resize(1);
        nonsolid.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(nonsolid, error));
        CHECK(g.server.CastWorldRay({0, 10, 0}, {0, -1, 0}, hit) == SegmentResult::Miss);
        g.server.collision_ready = false;
        CHECK(g.server.CastWorldRay({0, 10, 0}, {0, -1, 0}, hit) == SegmentResult::Unavailable);
        h = make(g, {{"FlushWithWorld", true}});
        CHECK(h && !get(g, h)->error.empty() && g.objects.get(h)->life == Life::Pending);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (auto name : {"", "absent.spr", "missing.spr", "bad.spr", "frame.dtx"})
        {
            CHECK(!make(g, {{"Filename", std::string(name)}}));
            CHECK(g.objects.finish_frame());
        }
        for (auto n : {-1, 1025})
            CHECK(!make(g, {{"RepeatY", std::int32_t(n)}}));
        CHECK(!make(g, {{"RepeatX", std::int32_t(33)}, {"RepeatY", std::int32_t(33)}}));
        CHECK(!make(g, {{"Bogus", true}})); // Undeclared names still reject (TypeAction is a retired flag).
        CHECK(!g.objects.create(g.objects.find_class("DetailSprite"), {}, Creation::Restore));
        CHECK(g.objects.finish_frame() && g.objects.snapshot().empty());
        auto failed = make(g, {{"FlushWithWorld", true},
                               {"Repeat", true},
                               {"RepeatX", std::int32_t(3)},
                               {"SpacingX", 1e7f},
                               {"Pos", DVector{0, 10, 0}}});
        auto* partial = get(g, failed);
        CHECK(partial && !partial->error.empty() && partial->children.size() == 1);
        auto survivor = partial->children[0];
        CHECK(g.objects.finish_frame() && !g.objects.get(failed) && g.objects.get(survivor));
        auto a = make(g);
        CHECK(a);
        auto sprite = g.objects.get(a)->sprite;
        CHECK(setup(g, root, "override.rez", 20));
        auto b = make(g);
        CHECK(b && g.objects.get(b)->sprite != sprite && g.objects.get(b)->sprite->fps == 20 && sprite->fps == 10);
        ObjectSelection selected{g.objects.get(a)->id};
        std::weak_ptr<const Sprite> weak = g.objects.get(b)->sprite;
        CHECK(g.unload_world());
        selected.refresh(g.objects);
        CHECK(!selected.id && weak.expired() && !g.objects.get(a));
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0 && g.server.list_count() == 0);
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        unsigned worlds = 0, count = 0, loaded = 0, missing = 0, rejected = 0, flush = 0, repeat = 0, invalid = 0;
        for (const auto& [path, res] : g.resources.index())
        {
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<uint8_t> b;
            World w;
            CHECK(g.resources.read(path, b, error));
            if (!read_world(b, w, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            CHECK(g.collision.build(w.models[0], error));
            g.server.collision_ready = true;
            for (const auto& o : w.objects)
                if (o.class_name == "DetailSprite")
                {
                    ++count;
                    Properties p;
                    for (const auto& v : o.properties)
                        p.emplace(v.name, v.value);
                    auto name = std::get<std::string>(p.at("Filename"));
                    std::shared_ptr<const Sprite> sprite;
                    if (!g.resources.find(name))
                        ++missing;
                    else if (g.sprites.load(name, sprite, error))
                        ++loaded;
                    else
                    {
                        ++invalid;
                        std::cout << path << " resource=" << name << " error=" << error << '\n';
                    }
                    auto h = g.objects.create(g.objects.find_class("DetailSprite"), {}, Creation::World, p);
                    if (!h)
                    {
                        ++rejected;
                        std::cout << path << " rejected=" << g.objects.diagnostics().back() << '\n';
                    }
                    else
                    {
                        auto* d = get(g, h);
                        CHECK(d && d->error.empty());
                        flush += d->flush;
                        repeat += d->repeat;
                        if (d->flush)
                            std::cout << path << " flush=" << int(d->flush_result)
                                      << " y=" << g.objects.get(h)->state.m_Pos.y << '\n';
                        CHECK(g.objects.remove(h));
                    }
                    CHECK(g.objects.finish_frame());
                }
        }
        std::cout << "worlds=" << worlds << " sources=" << count << " manifests=" << loaded << " missing=" << missing
                  << " invalid=" << invalid << " rejected=" << rejected << " flush=" << flush << " repeat=" << repeat
                  << '\n';
        CHECK(worlds == 58 && count == 126 && missing == 4 && invalid == 0 && rejected == 4 && loaded == 122 &&
              flush == 3 && repeat == 0);
        return true;
    }
    bool subway(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error));
            unsigned sprites = 0;
            std::map<EntryState, unsigned> counts;
            std::weak_ptr<const Sprite> weak;
            for (size_t i = 0; i < g.entries.size(); ++i)
            {
                const auto& e = g.entries[i];
                ++counts[e.state];
                if (g.world.objects[i].class_name != "DetailSprite")
                    continue;
                ++sprites;
                CHECK(e.state == EntryState::Created);
                auto* r = g.objects.get(e.object);
                auto* d = dynamic_cast<DetailSprite*>(r->game.get());
                CHECK(d && !d->flush && d->children.empty() && d->chromakey && d->rotatable &&
                      r->state.m_Scale.x == .5f && r->state.m_Pos.y == 129 && r->sprite &&
                      r->sprite->frames.size() == 1 && r->sprite->fps == 15 && r->sprite->transparent &&
                      !r->sprite->translucent);
                weak = r->sprite;
            }
            CHECK(sprites == 12 && counts[EntryState::Created] == 204 && counts[EntryState::WorldSound] == 4 &&
                  counts[EntryState::Unsupported] == 0 && counts[EntryState::EditorOnly] == 372 &&
                  counts[EntryState::Failed] == 0);
            CHECK(g.objects.snapshot().size() == 227 && g.enter_player(error) && g.objects.snapshot().size() == 230 &&
                  g.step(.02));
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                if (r->sprite)
                    CHECK(r->sprite_track.frame == 0 && std::abs(r->sprite_track.milliseconds - 20) < 1e-6);
            }
            CHECK(g.objects.snapshot().size() == 232 && g.objects.attachments().size() == 11 && g.run_world(error) &&
                  error.empty() && g.state == SessionState::Ready);
            CHECK(g.unload_world() && weak.expired() && g.objects.snapshot().empty() && g.server.string_count() == 0 &&
                  g.server.message_count() == 0 && g.server.list_count() == 0);
            std::cout << "cycle=" << cycle
                      << " 12 DetailSprite source instances, exact SPR frame resources, hidden-independent clocks and "
                         "unload verified; full M2 acceptance remains separate\n";
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"reader", reader}, {"clock", clock_test},  {"properties", properties}, {"placement", placement},
        {"ray", ray},       {"failures", failures}, {"catalog", catalog},       {"subway", subway}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
