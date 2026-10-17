#include "app/model_scene.h"
#include "audio_fixture.h"
#include "game/original/DestructableModel.h"
#include <algorithm>
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
    bool near(float a, float b)
    {
        return std::abs(a - b) < .02f;
    }
    HOBJECT model(GameSession& g, Properties props = {}, Creation reason = Creation::World)
    {
        props.try_emplace("InitFilename", std::string("models/characters/caleb.abc"));
        props.try_emplace("InitSkin", std::string("skins/characters/caleb1.dtx"));
        props.try_emplace("Pos", DVector{0, 100, 0});
        props.try_emplace("Gravity", false);
        return g.objects.create(g.objects.find_class("CDestructableModel"), {}, reason, props);
    }
    CDestructableModel* get(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<CDestructableModel*>(g.server.HandleToObject(h));
    }
    bool hit(GameSession& g, HOBJECT h, float amount, DamageType type = DamageType::Normal)
    {
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(h), h, damage_message);
        CHECK(m && g.server.WriteDamage(m, {{1, 0, 0}, {}, amount, type, h}) == DE_OK);
        CHECK(g.server.EndMessage(m) == DE_OK);
        return true;
    }
    HOBJECT box(GameSession& g, DVector position, DVector dims)
    {
        ObjectCreateStruct state;
        state.m_Pos = position;
        state.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
        if (h)
            g.server.SetObjectDims(h, &dims);
        return h;
    }
    bool unnamed()
    {
        Model m;
        std::string error;
        auto bytes = test_model::abc(false, false, 2, 0, -1, 0, true);
        CHECK(read_model(bytes.b, m, error) && m.animations[0].name.empty());
        CHECK(animation_index(m, "") == 0 && animation_index(m, "walk") == no_animation);
        CHECK(m.animations[0].keys.size() == 3 && m.animations[0].dimensions.y == 2);
        std::vector<DVector> vertices;
        CHECK(sample_vertices(m, 0, 50, vertices) && vertices.size() == 3);
        // An empty name is valid; a zero key count remains malformed.
        for (std::size_t at = 0; at < bytes.b.size();)
        {
            const unsigned len = bytes.b[at] | (unsigned(bytes.b[at + 1]) << 8);
            const std::string name(bytes.b.begin() + at + 2, bytes.b.begin() + at + 2 + len);
            const auto next = at + 2 + len;
            if (name == "Animation")
            {
                bytes.patch(next + 4 + 4 + 2 + 4 + 24, 0);
                break;
            }
            at = bytes.b[next] | (unsigned(bytes.b[next + 1]) << 8) | (unsigned(bytes.b[next + 2]) << 16) |
                 (unsigned(bytes.b[next + 3]) << 24);
        }
        CHECK(!read_model(bytes.b, m, error) && m.animations.empty());
        return true;
    }
    bool properties(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = model(g, {{"Pushable", true},
                           {"ObjectMass", 750.f},
                           {"ModelScale", 2.f},
                           {"Rotation", EulerAngles{{0, .78539816f, 0}}},
                           {"Metal", true},
                           {"SurfaceType", std::int32_t(20)}});
        auto* r = g.objects.get(h);
        auto* m = get(g, h);
        CHECK(r && m && m->initialized && r->model_body && !r->character_body && !r->world_body);
        CHECK(near(r->dimensions.x, 33.94112f) && near(r->dimensions.z, 33.94112f) && near(r->dimensions.y, 82));
        CHECK(near(r->state.m_Pos.y, 82) && m->ground_result == DE_OK);
        CHECK(r->blocking_priority == 40 && r->body_properties.friction == 20 && m->m_damage.apply_damage_physics);
        CHECK(r->user_flags == ((20u << 24) | (1u << 3) | (1u << 4) | (1u << 5)));
        CHECK(m->sliding_path == "sounds/events/pushmetal.wav" && m->damage_hit_points == 0 &&
              m->dimensions_publications == 1);
        auto fixed = model(g, {{"Pos", DVector{300, 100, 0}}, {"InitDims", DVector{1, 2, 3}}, {"ModelScale", 5.f}});
        CHECK(fixed && g.objects.get(fixed)->dimensions.y == 2 && !(g.objects.get(fixed)->state.m_Flags & FLAG_SHADOW));
        CHECK(g.objects.get(fixed)->blocking_priority == 100 && !get(g, fixed)->m_damage.apply_damage_physics);
        auto string = model(g, {{"Pos", DVector{500, 100, 0}}, {"Visible", false}, {"Solid", false}}, Creation::String);
        CHECK(string &&
              (g.objects.get(string)->state.m_Flags & (FLAG_SOLID | FLAG_GRAVITY | FLAG_VISIBLE)) ==
                  (FLAG_SOLID | FLAG_GRAVITY | FLAG_VISIBLE) &&
              near(g.objects.get(string)->state.m_Pos.y, 100));
        CHECK(!g.server.ConfigureCharacterBody(h, 100, 7) && !g.server.ConfigureModelBody(h, {100, 7, true}));
        auto flags = r->state.m_Flags;
        g.server.SetObjectFlags(h, flags | FLAG_GOTHRUWORLD);
        CHECK(r->state.m_Flags == flags);
        return true;
    }
    bool rejects(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        for (const auto& p : std::vector<Properties>{{{"Chrome", true}},
                                                     {{"InitDims", DVector{-1, 0, 0}}},
                                                     {{"ObjectMass", -1.f}},
                                                     {{"ModelScale", 0.f}},
                                                     {{"Alpha", 2.f}},
                                                     {{"SurfaceType", std::int32_t(256)}},
                                                     {{"SpawnObject", std::string("Missing")}},
                                                     {{"DestroyFilename", std::string("missing.abc")}}})
            CHECK(!model(g, p));
        auto missing = model(g, {{"InitFilename", std::string("missing.abc")}});
        CHECK(!missing || !g.server.IsObjectAlive(missing));
        CHECK(!model(g, {}, Creation::Restore));
        CHECK(g.objects.finish_frame() && CDestructable::m_dwNumDestructables == 0);
        CHECK(!g.server.string_count() && !g.server.message_count());
        auto replacement =
            model(g, {{"DamageFilename", std::string("missing.abc")}, {"DamageSkin", std::string("missing.dtx")}});
        auto* m = get(g, replacement);
        CHECK(m && hit(g, replacement, 60) && !m->error.empty());
        CHECK(g.server.IsObjectAlive(replacement) && g.objects.get(replacement)->activity == OBJSTATE_INACTIVE);
        CHECK(std::string(g.objects.get(replacement)->state.m_Filename) == "models/characters/caleb.abc");
        CHECK(!g.objects.get(replacement)->scheduled);
        return true;
    }
    bool damage(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = model(g, {{"DamageFilename", std::string("models/weapons/beretta_pv.abc")},
                           {"DamageSkin", std::string("skins/weapons/c_beretta_pv_t.dtx")},
                           {"DamageDims", DVector{3, 4, 5}},
                           {"DestroyFilename", std::string("models/weapons/c_knife_pv.abc")},
                           {"DestroySkin", std::string("skins/weapons/c_knife_pv_t.dtx")},
                           {"DestroyGravity", false},
                           {"Wood", true}});
        auto* m = get(g, h);
        CHECK(m && hit(g, h, 55) && near(m->m_damage.m_fHitPoints, 45));
        CHECK(m->damage_hit_points == 0 && m->dimensions_publications == 2 && g.objects.get(h)->dimensions.y == 4);
        CHECK(std::string(g.objects.get(h)->state.m_Filename) == "models/weapons/beretta_pv.abc");
        CHECK(hit(g, h, 50) && !m->dead_state && g.objects.get(h)->scheduled);
        CHECK(g.step(.01) && g.server.IsObjectAlive(h) && m->dead_state && m->destructions == 1);
        CHECK(m->dimensions_publications == 3 && g.objects.get(h)->dimensions.y == 2 &&
              g.server.debris_effects.size() == 1);
        CHECK(g.objects.get(h)->state.m_Flags == (FLAG_VISIBLE | FLAG_SOLID));
        // The source revisits destruction on a later scheduled update of a surviving replacement.
        CHECK(m->HandleTriggerMessage("Alpha .5") && g.step(.02) && m->destructions == 2);
        auto inert = model(g, {{"Pos", DVector{200, 100, 0}}, {"Destructable", false}});
        CHECK(inert && hit(g, inert, 150) && g.step(.02) && g.server.IsObjectAlive(inert) &&
              !get(g, inert)->dead_state);
        auto immune = model(g, {{"Pos", DVector{300, 100, 0}}, {"CanDamage", false}});
        CHECK(immune && hit(g, immune, 150) && get(g, immune)->m_damage.m_fHitPoints == 100);
        return true;
    }
    bool cascade(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false, true, true));
        auto h = model(g, {{"Exploding", true}, {"Metal", true}, {"SpawnObject", std::string("HealthPU")}});
        auto neighbor = model(g, {{"Pos", DVector{60, 100, 0}}, {"HitPoints", 10.f}, {"Wood", true}});
        CHECK(h && neighbor && hit(g, h, 120));
        CHECK(g.server.spawn_reports.size() == 1 && g.server.IsObjectAlive(h));
        CHECK(g.step(.1) && g.server.IsObjectAlive(h) && g.server.explosion_effects.empty());
        CHECK(g.step(.1) && g.step(.1) && g.server.IsObjectAlive(h));
        CHECK(g.step(.01) && !g.server.IsObjectAlive(h) && g.server.explosion_effects.size() == 1);
        CHECK(get(g, neighbor)->m_damage.m_fHitPoints <= 0 && g.step(.01) && !g.server.IsObjectAlive(neighbor));
        CHECK(g.server.debris_effects.size() == 2 && !g.server.message_count());
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0 && !g.server.string_count());
        return true;
    }
    bool fade(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = model(g, {{"TintColor", DVector{255, 128, 0}}});
        auto* m = get(g, h);
        CHECK(m && m->HandleTriggerMessage("Alpha .2 1"));
        CHECK(g.step(.25) && near(m->alpha, .8) && near(g.objects.get(h)->alpha, .8));
        CHECK(near(g.objects.get(h)->color.y, 128.f / 255));
        CHECK(!m->HandleTriggerMessage("Alpha .3; Alpha nan") && near(m->target_alpha, .2));
        CHECK(!m->HandleTriggerMessage("Alpha 2") && !m->trigger_error.empty());
        CHECK(m->HandleTriggerMessage("Alpha .9; aLPHa .4") && g.step(.02) && near(m->alpha, .4));
        CHECK(m->HandleTriggerMessage("Alpha .1 1") && g.step(.5) && near(m->alpha, .25));
        auto id = g.objects.get(h)->id;
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && !g.objects.get(id));
        CHECK(!g.server.string_count());
        return true;
    }
    bool physics(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = model(g, {{"Pushable", true}, {"InitDims", DVector{10, 10, 10}}}, Creation::String);
        auto* r = g.objects.get(h);
        CHECK(r && g.step(.1) && near(r->state.m_Pos.y, 90) && near(r->velocity.y, -200));
        for (int i = 0; i < 10; ++i)
            CHECK(g.step(.05));
        CHECK(near(r->state.m_Pos.y, 10) && near(r->velocity.y, 0) && r->body_steps > 0);
        auto actor = box(g, {-15, 10, 0}, {2, 8, 2});
        CHECK(actor && g.server.SetBlockingPriority(actor, 50));
        DVector to{0, 10, 0};
        CHECK(g.server.MoveObject(actor, &to) == DE_OK && g.objects.get(actor)->state.m_Pos.x > -.1f);
        CHECK(r->state.m_Pos.x > 11 && r->body_pushes == 1);
        const float before = r->state.m_Pos.x;
        CHECK(box(g, {before + 14, 10, 0}, {2, 10, 10}));
        to.x = 30;
        CHECK(g.server.MoveObject(actor, &to) == DE_OK && near(r->state.m_Pos.x, before));
        CHECK(hit(g, h, 1) && get(g, h)->m_damage.damage_impulses == 1 && near(r->velocity.x, 25));
        auto carried = model(g, {{"Pos", DVector{300, 100, 0}}, {"Pushable", true}, {"InitDims", DVector{10, 10, 10}}});
        auto rider = box(g, {300, g.objects.get(carried)->state.m_Pos.y + 11.001f, 0}, {1, 1, 1});
        CHECK(carried && rider);
        DVector carry_to{320, g.objects.get(carried)->state.m_Pos.y, 0};
        auto moved = g.server.MoveObject(carried, &carry_to);
        CHECK(moved == DE_OK && near(g.objects.get(rider)->state.m_Pos.x, 320));
        CHECK(g.objects.get(carried)->mover_riders == 1);
        r->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(g.step(.05) && r->body_steps > 0);
        return true;
    }
    bool filters(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = model(g, {{"InitDims", DVector{10, 10, 10}}});
        CHECK(h);
        SegmentHit out;
        CHECK(g.server.IntersectSegment({-30, 100, 0}, {30, 100, 0}, nullptr, out, false, SegmentObjects::SolidOrRayHit,
                                        SegmentFilter::AIVisibility) == SegmentResult::Hit &&
              out.object == h);
        CHECK(g.server.IntersectSegment({-30, 100, 0}, {30, 100, 0}, nullptr, out, false, SegmentObjects::SolidOrRayHit,
                                        SegmentFilter::AIShot) == SegmentResult::Miss);
        get(g, h)->destructable = false;
        CHECK(g.server.IntersectSegment({-30, 100, 0}, {30, 100, 0}, nullptr, out, false, SegmentObjects::SolidOrRayHit,
                                        SegmentFilter::AIShot) == SegmentResult::Hit);
        CHECK(g.objects.remove(h));
        CHECK(g.server.IntersectSegment({-30, 100, 0}, {30, 100, 0}, nullptr, out) == SegmentResult::Miss);
        return true;
    }
    bool audio(const char* root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = model(g, {{"Pushable", true}, {"SlidingSound", std::string("cue.wav")}});
        ObjectCreateStruct state;
        state.m_Pos = {200, 100, 0};
        auto actor = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
        auto* m = get(g, h);
        CHECK(m && actor && g.objects.engine_message(h, MID_TOUCHNOTIFY, actor));
        CHECK(m->sliding && !m->standing && g.step(.01) && !m->sliding_sound);
        // Explicit nonzero force probe: the production solver does not manufacture contact force.
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, actor, 1));
        DVector to{5, 10, 0};
        CHECK(g.server.MoveObject(h, &to) == DE_OK && g.step(.01));
        CHECK(m->sliding_sound && g.server.audio.get(m->sliding_sound));
        auto sound = m->sliding_sound;
        CHECK(g.server.audio.get(sound)->request.loop);
        CHECK(g.step(.01) && g.step(.01) && !m->sliding_sound && !g.server.audio.get(sound));
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, actor, 1));
        to.x = 10;
        CHECK(g.server.MoveObject(h, &to) == DE_OK && g.step(.01) && m->sliding_sound);
        sound = m->sliding_sound;
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && !g.server.audio.get(sound));
        CHECK(!g.server.string_count());
        return true;
    }
    bool city(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_CityHub", error));
            unsigned count = 0, pushable = 0, gravity = 0, failed = 0, unknown = 0, created = 0, ground_misses = 0;
            std::vector<ObjectId> ids;
            ModelView scene;
            for (std::size_t i = 0; i < g.entries.size(); ++i)
            {
                auto& e = g.entries[i];
                created += e.state == EntryState::Created;
                unknown += e.state == EntryState::Unsupported;
                failed += e.state == EntryState::Failed;
                if (g.world.objects[i].class_name != "CDestructableModel")
                    continue;
                auto* r = g.objects.get(e.object);
                auto* m = r ? get(g, r->handle) : nullptr;
                if (!m)
                    std::cerr << "Model failed " << i << ": " << e.detail << " "
                              << std::get<std::string>(*g.world.objects[i].find("InitFilename")) << '\n';
                CHECK(e.state == EntryState::Created && r && r->model && m && m->initialized && m->error.empty());
                ++count;
                pushable += m->pushable;
                gravity += bool(r->state.m_Flags & FLAG_GRAVITY);
                ground_misses += (r->state.m_Flags & FLAG_GRAVITY) && m->ground_result == DE_NOTFOUND;
                CHECK(r->model_body && r->model_error.empty() && m->damage_hit_points == 0 && m->destroy_model.empty());
                ids.push_back(r->id);
            }
            std::cout << "CityHub model initialization: " << count << " models / " << pushable << " pushable / "
                      << gravity << " gravity / " << ground_misses << " ground misses\n";
            CHECK(count == 122 && pushable == 51 && gravity == 98 && ground_misses == 18);
            CHECK(created == 443 && unknown == 10 && failed == 1);
            CHECK(update_model_scene(g, scene, error));
            unsigned skin_failures = 0, triangles = 0;
            for (auto id : ids)
            {
                auto it =
                    std::find_if(scene.entries.begin(), scene.entries.end(), [&](auto& e) { return e.object == id; });
                CHECK(it != scene.entries.end() && it->visible && it->triangles > 0);
                skin_failures += !it->error.empty();
                triangles += it->triangles;
            }
            std::cout << "CityHub cycle " << cycle << ": created " << created << ", unsupported " << unknown
                      << ", failed " << failed << "; models " << count << ", pushable " << pushable << ", gravity "
                      << gravity << ", ground no-hit " << ground_misses << ", skin failures " << skin_failures
                      << ", triangles " << triangles << '\n';
            CHECK(skin_failures == 0 && triangles == 7450);
            for (auto id : ids)
            {
                auto* r = g.objects.get(id);
                if (std::string_view(r->state.m_Filename).ends_with("ceilingfan2.abc"))
                {
                    CHECK(r->model->animations.size() == 1 && r->model->animations[0].name.empty());
                    CHECK(r->model->animations[0].keys.size() == 19 && r->model->animations[0].duration == 599);
                    std::vector<DVector> a, b;
                    CHECK(sample_vertices(*r->model, 0, 0, a) && sample_vertices(*r->model, 0, 100, b));
                    bool changed = false;
                    for (std::size_t i = 0; i < a.size(); ++i)
                        changed |= !near(a[i].x, b[i].x) || !near(a[i].y, b[i].y) || !near(a[i].z, b[i].z);
                    CHECK(changed);
                }
            }
            CHECK(g.server.advance_physics(.05));
            unsigned blocked = 0;
            for (auto id : ids)
            {
                auto* r = g.objects.get(id);
                CHECK(r && get(g, r->handle)->error.empty());
                blocked += r->mover_blocks > 0;
            }
            std::cout << "Initial bounded physics: " << blocked
                      << " bodies reporting a collision block (includes floor contacts; no depenetration)\n";
            CHECK(g.unload_world() && !g.objects.get(ids.front()) && CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string test = argv[1];
    const bool ok = test == "unnamed"      ? unnamed()
                    : test == "properties" ? properties(argv[2])
                    : test == "rejects"    ? rejects(argv[2])
                    : test == "damage"     ? damage(argv[2])
                    : test == "cascade"    ? cascade(argv[2])
                    : test == "fade"       ? fade(argv[2])
                    : test == "physics"    ? physics(argv[2])
                    : test == "filters"    ? filters(argv[2])
                    : test == "audio"      ? audio(argv[2])
                    : test == "city"       ? city(argv[2])
                                           : false;
    return ok ? 0 : 1;
}
