#include "brush_fixture.h"
#include "engine/container.h"
#include "game/original/RotatingBrush.h"
#include "game/original/spawn_object.h"
#include "game/original/Trigger.h"
#include "player_fixture.h"
#include <functional>
#include <numbers>
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
    constexpr float pi = std::numbers::pi_v<float>;
    bool near(float a, float b)
    {
        return std::abs(a - b) < .005f;
    }
    HOBJECT brush(GameSession& g, Properties props = {}, bool rotating = true, DVector lo = {-20, 40, -2},
                  DVector hi = {20, 60, 2})
    {
        const auto name = "brush" + std::to_string(g.world.models.size());
        auto model = test_brush::box(name, lo, hi);
        model.translation = {0, 50, 0};
        g.world.models.push_back(model);
        props["Name"] = name;
        if (rotating)
            props.try_emplace("YAxisRevTime", 4.0f);
        props.try_emplace("HitPoints", 10.0f);
        return g.objects.create(g.objects.find_class(rotating ? "RotatingBrush" : "CDestructableBrush"), {},
                                Creation::World, props);
    }
    Rotating& rotation(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<RotatingBrush*>(g.objects.get(h)->game.get())->rotating;
    }
    CDestructableBrush& destruct(GameSession& g, HOBJECT h)
    {
        return *dynamic_cast<CDestructableBrush*>(g.objects.get(h)->game.get());
    }
    bool command(GameSession& g, HOBJECT h, const char* text)
    {
        auto str = g.server.CreateString(text);
        auto msg = g.server.StartMessageToObject(g.objects.get(h)->game.get(), h, 1000);
        CHECK(msg && g.server.WriteToMessageHString(msg, str) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        g.server.FreeString(str);
        return true;
    }
    bool damage(GameSession& g, HOBJECT h, float amount, DamageType type = DamageType::Normal)
    {
        auto msg = g.server.StartMessageToObject(g.objects.get(h)->game.get(), h, damage_message);
        CHECK(msg && g.server.WriteDamage(msg, {{0, 0, 1}, {}, amount, type, h}) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        return true;
    }
    HOBJECT body(GameSession& g, DVector pos, DVector dims, DDWORD flags = FLAG_SOLID)
    {
        auto h = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                  {{"Pos", pos}, {"Flags", std::int32_t(flags)}});
        if (h)
            g.server.SetObjectDims(h, &dims);
        return h;
    }
    bool state(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        CHECK(h);
        auto& r = rotation(g, h);
        CHECK(r.state == Rotating::Normal && near(r.velocity.y, pi / 2) && !destruct(g, h).allow_marks);
        CHECK(!(g.objects.get(h)->state.m_Flags & FLAG_GOTHRUWORLD));
        CHECK(g.step(.1) && near(r.angles.y, pi * .05f) && r.last_move == MoverResult::Moved);
        CHECK(command(g, h, "oFf") && r.state == Rotating::SpinDown);
        CHECK(g.step(.1) && r.state == Rotating::Off);
        CHECK(g.step(.1) && !g.objects.get(h)->scheduled);
        auto a = r.angles.y;
        CHECK(g.step(.1) && r.angles.y == a);
        CHECK(command(g, h, "TRIGGER") && r.state == Rotating::Off); // USE is not ON.
        CHECK(command(g, h, "toggle") && g.step(.1) && r.state == Rotating::Normal);
        CHECK(near(r.angles.y - a, pi * .15f)); // Off duration remains in source last_time.
        CHECK(command(g, h, "ON") && r.commands == 2);
        CHECK(command(g, h, "OFF") && command(g, h, "TOGGLE") && r.state == Rotating::SpinDown);
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool spin(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {{"StartOn", false},
                           {"XAxisRevTime", 2.0f},
                           {"ZAxisRevTime", -1.0f},
                           {"XAxisSpinUpTime", .2f},
                           {"YAxisSpinUpTime", .4f},
                           {"YAxisSpinDownTime", .2f},
                           {"YRotateForward", false},
                           {"SpinUpSound", std::string("up.wav")},
                           {"BusySound", std::string("busy.wav")}});
        CHECK(h && command(g, h, "ON"));
        auto& r = rotation(g, h);
        CHECK(g.step(.1) && near(r.velocity.x, pi * .5f) && near(r.velocity.y, pi * .125f) && r.velocity.z == 0);
        CHECK(near(r.angles.y, -pi * .0125f) && r.state == Rotating::SpinUp && r.last_sound == "up.wav");
        CHECK(g.step(.1) && r.state == Rotating::SpinUp);
        CHECK(g.step(.21) && r.state == Rotating::Normal && r.sound_loop && r.last_sound == "busy.wav");
        CHECK(command(g, h, "OFF") && g.step(.1) && near(r.velocity.y, pi * .25f));
        CHECK(command(g, h, "OFF") && g.step(.1) && near(r.velocity.y, pi * .25f)); // Restart the ramp.
        CHECK(g.step(.11) && r.state == Rotating::Off && !r.sound_loop);
        return true;
    }
    bool geometry(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {{"Rotation", EulerAngles{{0, .4f, 0}}}, {"BoxPhysics", false}});
        CHECK(h);
        auto* record = g.objects.get(h);
        auto& r = rotation(g, h);
        CHECK(near(r.pose_angles.y, .4f) && r.angles.y == 0);
        CHECK(g.step(.1) && near(r.angles.y, pi * .05f) && near(record->state.m_Pos.y, 50));
        CHECK(record->world_model != record->world_model_base && record->rotation_steps > 0);
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({0, 50, -50}, {0, 50, 50}, nullptr, hit) == SegmentResult::Hit &&
              hit.object == h);
        record->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(g.step(.1) && near(r.angles.y, pi * .1f));
        g.server.SetObjectState(h, OBJSTATE_INACTIVE);
        auto angle = r.angles.y;
        CHECK(g.step(.1) && r.angles.y == angle);
        g.server.SetObjectState(h, OBJSTATE_ACTIVE);
        CHECK(g.step(.1) && r.angles.y > angle);
        return true;
    }
    bool limits(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        CHECK(!brush(g, {{"Pushable", true}}));
        CHECK(!brush(g, {{"Gravity", true}}));
        CHECK(!brush(g, {{"SpawnObject", std::string("Enemy")}}));
        CHECK(!brush(g, {{"YAxisRevTime", 1e-40f}}));
        CHECK(!brush(g, {{"Rotation", rotation_from_euler({0, .4f, 0})}}));
        CHECK(g.objects.finish_frame() && g.objects.snapshot().empty());
        auto h = brush(g, {{"StartOn", false}});
        CHECK(h);
        CHECK(g.step(20) && command(g, h, "ON") && g.step(.02));
        auto& r = rotation(g, h);
        CHECK(r.last_move == MoverResult::Unavailable && r.angles.y == 0);
        CHECK(g.step(.02) && r.last_move == MoverResult::Moved && near(r.angles.y, pi * .01f));
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        auto name = g.world.models.back().name;
        auto hs = g.objects.create(g.objects.find_class("RotatingBrush"), {}, Creation::String,
                                   {{"Name", name}, {"YAxisRevTime", 4.0f}});
        CHECK(hs && rotation(g, hs).state == Rotating::Off && rotation(g, hs).velocity.y == 0);
        return true;
    }
    bool transport(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {{"BoxPhysics", false}}, true, {-30, 40, -30}, {30, 50, 30});
        CHECK(h);
        auto b = body(g, {15, 52.001f, 0}, {1, 2, 1});
        CHECK(b);
        auto detached = body(g, {-15, 52.001f, 0}, {1, 2, 1}, FLAG_SOLID | FLAG_DONTFOLLOWSTANDING);
        CHECK(detached);
        CHECK(g.step(.05));
        CHECK(g.objects.get(h)->mover_riders > 0 && g.objects.get(b)->state.m_Pos.z < -.1f);
        CHECK(g.objects.get(detached)->state.m_Pos.z == 0);
        CHECK(g.objects.remove(b) && g.objects.remove(detached) && g.objects.finish_frame());
        auto obstacle = brush(g, {{"StartOn", false}, {"BoxPhysics", false}});
        CHECK(obstacle);
        auto old = rotation(g, h).angles;
        CHECK(g.step(.05) && rotation(g, h).last_move == MoverResult::Blocked && rotation(g, h).angles.y == old.y);
        CHECK(g.objects.get(h)->mover_blocker == obstacle);
        CHECK(g.objects.remove(obstacle) && g.step(.05) && rotation(g, h).last_move == MoverResult::Moved);
        return true;
    }
    bool destruction(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {{"FireThrough", true}}, false);
        CHECK(h);
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({0, 50, -50}, {0, 50, 50}, nullptr, hit) == SegmentResult::Hit &&
              hit.object == h);
        CHECK(g.server.IntersectSegment({0, 50, -50}, {0, 50, 50}, nullptr, hit, true) == SegmentResult::Miss);
        CHECK(damage(g, h, 3) && destruct(g, h).m_damage.m_fHitPoints == 7);
        CHECK(damage(g, h, 7) && destruct(g, h).m_damage.IsDead() && g.server.IsObjectAlive(h));
        CHECK(g.step(.002) && !g.objects.get(h) && CDestructable::m_dwNumDestructables == 0);
        CHECK(g.server.brush_destructions.size() == 1 && g.server.brush_destructions[0].damage == 10);
        auto immortal = brush(g, {{"CanDamage", false}}, false);
        CHECK(immortal && command(g, immortal, "KILL"));
        CHECK(!destruct(g, immortal).m_damage.IsDead());
        auto remain = brush(g, {{"Destructable", false}}, false);
        CHECK(remain && command(g, remain, "KILL"));
        CHECK(destruct(g, remain).m_damage.IsDead() && g.step(.01) && g.server.IsObjectAlive(remain));
        auto triggeronly = brush(g, {{"TriggerDestroyOnly", true}}, false);
        CHECK(triggeronly && damage(g, triggeronly, 100) && !destruct(g, triggeronly).m_damage.IsDead());
        CHECK(command(g, triggeronly, "kill") && g.step(.01) && !g.objects.get(triggeronly));
        CHECK(g.unload_world() && g.server.brush_destructions.empty() && CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto victim = brush(g, {{"StartOn", false}});
        CHECK(victim);
        const std::string victim_name = g.objects.get(victim)->state.m_Name;
        auto h = brush(g, {{"DamageTriggerTarget", victim_name},
                           {"DamageTriggerMessage", std::string("ON")},
                           {"DeathTriggerTarget", victim_name},
                           {"DeathTriggerMessage", std::string("REMOVE")}});
        CHECK(h);
        CHECK(damage(g, h, 1) && rotation(g, victim).state == Rotating::SpinUp);
        CHECK(command(g, h, "KILL") && !g.server.IsObjectAlive(victim));
        CHECK(g.step(.002) && g.objects.snapshot().empty());
        auto self = brush(g);
        CHECK(self);
        auto& d = destruct(g, self).m_damage;
        d.death_target = g.objects.get(self)->state.m_Name;
        d.death_message = "KILL";
        CHECK(command(g, self, "KILL") && g.step(.01) && g.objects.snapshot().empty());
        auto removed = brush(g);
        CHECK(removed);
        auto& remove_damage = destruct(g, removed).m_damage;
        remove_damage.damage_target = g.objects.get(removed)->state.m_Name;
        remove_damage.damage_message = "REMOVE";
        CHECK(damage(g, removed, 1) && !g.server.IsObjectAlive(removed) && g.step(.01) && !g.objects.get(removed));
        CHECK(g.server.message_count() == 0 && g.server.string_count() == 0 &&
              CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        struct Count
        {
            unsigned created = 0, rejected = 0, filtered = 0, moved = 0, explosive = 0, detonated = 0, dynamic = 0,
                     blocked = 0, spawned = 0, compound = 0, absent_spawns = 0;
        };
        std::map<std::string, Count> counts;
        std::map<std::string, unsigned> failures, spawns;
        unsigned unreadable = 0;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            CHECK(g.resources.read(path, bytes, error));
            if (!read_world(bytes, g.world, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                ++unreadable;
                continue;
            }
            CHECK(g.collision.build(g.world.models.front(), error));
            g.server.collision_ready = true;
            for (const auto& source : g.world.objects)
            {
                if (source.class_name != "RotatingBrush" && source.class_name != "CDestructableBrush")
                    continue;
                Properties props;
                for (const auto& p : source.properties)
                    props.emplace(p.name, p.value);
                if (props.contains("SpawnObject") && !std::get<std::string>(props.at("SpawnObject")).empty())
                    ++spawns[std::get<std::string>(props.at("SpawnObject"))];
                auto h = g.objects.create(g.objects.find_class(source.class_name), {}, Creation::World, props);
                auto* r = g.objects.get(h);
                auto& count = counts[source.class_name];
                if (!r || r->life != Life::Alive)
                {
                    if (r && dynamic_cast<B2BaseClass*>(r->game.get())->filtered)
                        ++count.filtered;
                    else
                    {
                        const auto& why = g.objects.diagnostics().back();
                        std::string kind;
                        if (why.find("invalid property TypeAction") != std::string::npos)
                            kind = "schema";
                        else if (why.ends_with("Missing named world model"))
                            kind = "missing";
                        else if (why.ends_with("Ambiguous named world model"))
                            kind = "ambiguous";
                        else if (why.find("container shell") != std::string::npos ||
                                 why.find("face is not on its plane") != std::string::npos)
                            kind = "shell";
                        else if (why.starts_with("Rejected precreate: CDestructableBrush"))
                        {
                            auto enabled = [&](const char* key)
                            {
                                auto it = props.find(key);
                                return it != props.end() && std::get<bool>(it->second);
                            };
                            if (props.contains("SpawnObject") &&
                                !std::get<std::string>(props.at("SpawnObject")).empty())
                                kind = "spawn";
                            else if (enabled("Pushable") || enabled("Gravity"))
                            {
                                CHECK(!enabled("BoxPhysics"));
                                kind = "polygon dynamics";
                            }
                            else
                                CHECK(false);
                        }
                        else
                            CHECK(false);
                        ++failures[source.class_name + "/" + kind];
                        ++count.rejected;
                        std::cout << path << ' ' << source.class_name << ' ' << property_text(props.at("Name")) << ": "
                                  << g.objects.diagnostics().back() << '\n';
                    }
                }
                else
                {
                    ++count.created;
                    CHECK(r->world_model && dynamic_cast<CDestructableBrush*>(r->game.get()));
                    if (r->world_model->shared_edges)
                    {
                        ++count.compound;
                        CHECK(r->world_model->closed_components > 1);
                        std::cout << path << " " << r->state.m_Name << ": " << r->world_model->closed_components
                                  << " closed components / " << r->world_model->shared_edges << " shared edges\n";
                    }
                    CHECK(command(g, h, "ON"));
                    for (int i = 0; i < 4; ++i)
                        CHECK(g.step(.02));
                    count.moved += r->mover_moves > 0;
                    count.dynamic += r->world_body;
                    count.blocked += r->world_body && r->mover_blocks > 0;
                    if (r->world_body)
                        CHECK(r->body_steps > 0 && (r->state.m_Flags & FLAG_BOXPHYSICS));
                    auto& brush = destruct(g, h);
                    if (brush.debris.exploding)
                    {
                        ++count.explosive;
                        CHECK(brush.debris.explosion_damage == 100 && brush.debris.explosion_radius == 200);
                        const std::string name = r->state.m_Name;
                        const bool removes = brush.destructable && brush.m_damage.can_damage;
                        CHECK(command(g, h, "KILL") && g.step(.002));
                        if (removes)
                        {
                            ++count.detonated;
                            CHECK(!g.objects.get(h) && !g.server.explosion_effects.empty());
                            const auto& effect = g.server.explosion_effects.back();
                            CHECK(effect.source == name && effect.targets.delivered == 1 && !effect.targets.cancelled);
                        }
                        else
                            CHECK(g.objects.remove(h));
                    }
                    else if (!brush.m_damage.spawn_object.empty())
                    {
                        CHECK(command(g, h, "KILL"));
                        if (brush.m_damage.can_damage)
                        {
                            CHECK(!g.server.spawn_reports.empty());
                            auto child = g.server.spawn_reports.back().child;
                            if (SpawnClassAbsentFromOriginal(brush.m_damage.spawn_object))
                            {
                                // No original class: nothing spawns, as GetClass returns null.
                                CHECK(!child);
                                ++count.absent_spawns;
                            }
                            else
                            {
                                CHECK(child && g.objects.get(child)->model);
                                CHECK(g.objects.get(child)->velocity.x == brush.m_damage.spawn_velocity.x &&
                                      g.objects.get(child)->velocity.y == brush.m_damage.spawn_velocity.y &&
                                      g.objects.get(child)->velocity.z == brush.m_damage.spawn_velocity.z);
                                ++count.spawned;
                                CHECK(g.objects.remove(child));
                            }
                        }
                        CHECK(g.objects.remove(h));
                    }
                    else
                        CHECK(g.objects.remove(h));
                }
                CHECK(g.objects.finish_frame() && g.objects.snapshot().empty() &&
                      CDestructable::m_dwNumDestructables == 0);
            }
        }
        for (const auto& [name, c] : counts)
            std::cout << name << " created=" << c.created << " rejected=" << c.rejected << " filtered=" << c.filtered
                      << " moved=" << c.moved << " explosive=" << c.explosive << " detonated=" << c.detonated
                      << " dynamic=" << c.dynamic << " blocked=" << c.blocked << " spawned=" << c.spawned << '\n';
        for (const auto& [name, n] : spawns)
            std::cout << "SpawnObject " << name << "=" << n << '\n';
        for (const auto& [name, n] : failures)
            std::cout << name << "=" << n << '\n';
        CHECK(unreadable == 1 && counts["RotatingBrush"].created == 136 && counts["RotatingBrush"].rejected == 0 &&
              counts["RotatingBrush"].moved == 136 && counts["RotatingBrush"].filtered == 0 &&
              counts["CDestructableBrush"].created == 2390 && counts["CDestructableBrush"].rejected == 65 &&
              counts["CDestructableBrush"].filtered == 0 && counts["CDestructableBrush"].explosive == 50 &&
              counts["CDestructableBrush"].detonated == 50 && counts["CDestructableBrush"].spawned == 67 &&
              counts["CDestructableBrush"].compound == 3);
        CHECK(failures["CDestructableBrush/shell"] == 2 && !failures.contains("CDestructableBrush/schema") &&
              !failures.contains("CDestructableBrush/ambiguous") && failures["CDestructableBrush/missing"] == 32 &&
              failures["CDestructableBrush/polygon dynamics"] == 28 && failures["CDestructableBrush/spawn"] == 3 && counts["CDestructableBrush"].absent_spawns == 1 && !failures.contains("RotatingBrush/ambiguous"));
        CHECK(g.request_world("01_SubwayA", error));
        std::map<EntryState, unsigned> entries;
        for (const auto& e : g.entries)
            ++entries[e.state];
        CHECK(entries[EntryState::Created] == 204 && entries[EntryState::EditorOnly] == 372 &&
              entries[EntryState::Unsupported] == 0 && entries[EntryState::Failed] == 0 &&
              g.objects.snapshot().size() == 227);
        std::cout << "Subway instances=" << g.objects.snapshot().size() << "\n";
        for (auto [state, n] : entries)
            std::cout << "state=" << int(state) << " count=" << n << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"state", state},       {"spin", spin},           {"geometry", geometry},
        {"limits", limits},     {"transport", transport}, {"destruction", destruction},
        {"lifetime", lifetime}, {"catalog", catalog}};
    auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
