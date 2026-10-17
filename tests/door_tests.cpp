#include "brush_fixture.h"
#include "engine/world_model.h"
#include "game/original/Door.h"
#include "player_fixture.h"
#include <functional>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ":" << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    HOBJECT door(GameSession& g, Properties props = {}, DVector lo = {-10, 10, -10}, DVector hi = {10, 30, 10})
    {
        const auto name = "door" + std::to_string(g.world.models.size());
        auto model = test_brush::box(name, lo, hi);
        model.translation = {5, 20, 0}; // Pivot deliberately differs from the bounds center.
        g.world.models.push_back(model);
        props["Name"] = name;
        props.try_emplace("Speed", 100.0f);
        props.try_emplace("MoveDist", 50.0f);
        props.try_emplace("MoveDir", DVector{2, 0, 0});
        props.try_emplace("TriggerClosed", true);
        return g.objects.create(g.objects.find_class("Door"), {}, Creation::World, props);
    }
    HOBJECT body(GameSession& g, DVector pos, DVector dims, DDWORD flags = FLAG_SOLID)
    {
        auto h = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                  {{"Pos", pos}, {"Flags", std::int32_t(flags)}});
        if (h)
            g.server.SetObjectDims(h, &dims);
        return h;
    }
    bool trigger(GameSession& g, HOBJECT h, const char* command)
    {
        auto text = g.server.CreateString(command);
        auto msg = g.server.StartMessageToObject(nullptr, h, 1000);
        CHECK(text && msg);
        CHECK(g.server.WriteToMessageHString(msg, text) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        g.server.FreeString(text);
        return true;
    }
    bool steps(GameSession& g, unsigned n, double dt = .02)
    {
        for (unsigned i = 0; i < n; ++i)
            CHECK(g.step(dt));
        return true;
    }
    bool state(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g, {{"OpenDelay", .1f},
                          {"OpenBusySound", std::string("open.wav")},
                          {"CloseStopSound", std::string("stop.wav")},
                          {"PortalName", std::string("portal")}});
        CHECK(h);
        auto* d = dynamic_cast<Door*>(g.objects.get(h)->game.get());
        CHECK(d && d->state == Door::Closed && d->closing_speed == 100 && d->move_dir.x == 1);
        CHECK(trigger(g, h, "trigger"));
        CHECK(d->state == Door::Opening);
        CHECK(steps(g, 4));
        CHECK(g.objects.get(h)->state.m_Pos.x == 5 && !d->started_moving);
        CHECK(steps(g, 40));
        CHECK(d->state == Door::Open && g.objects.get(h)->state.m_Pos.x == 55 && !g.objects.get(h)->scheduled);
        CHECK(d->portal_open && d->sound_requests == 1);
        CHECK(trigger(g, h, "TRIGGER"));
        CHECK(d->state == Door::Closing);
        CHECK(steps(g, 30));
        CHECK(d->state == Door::Closed && g.objects.get(h)->state.m_Pos.x == 5 && !d->portal_open);
        CHECK(d->last_sound == "stop.wav" && !d->sound_loop);
        CHECK(trigger(g, h, "TRIGGER"));
        CHECK(steps(g, 10));
        CHECK(trigger(g, h, "TRIGGERCLOSE"));
        CHECK(d->state == Door::Closing);
        CHECK(trigger(g, h, "TRIGGER"));
        CHECK(d->state == Door::Opening);
        CHECK(trigger(g, h, "LOCK") && !d->trigger); // No child: source command is a no-op.
        CHECK(g.server.string_count() == 1 && g.server.message_count() == 0);
        CHECK(std::abs(Door::GetWaveValue(100, .5f, 1) - 110) < .001f);
        CHECK(std::abs(Door::GetWaveValue(100, 0, 2) - 110) < .001f);
        CHECK(std::abs(Door::GetWaveValue(100, 0, 3) - 10) < .001f);
        CHECK(g.unload_world());
        return true;
    }
    bool options(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g, {{"StartOpen", true}, {"TriggerClosed", false}, {"WaitTime", .1f}});
        CHECK(h);
        auto* d = dynamic_cast<Door*>(g.objects.get(h)->game.get());
        CHECK(d->closed_pos.x == 55 && d->open_pos.x == 5 && d->move_dir.x == -1);
        CHECK(g.objects.get(h)->state.m_Pos.x == 55 && d->state == Door::Closed);
        CHECK(trigger(g, h, "TRIGGER"));
        CHECK(steps(g, 60));
        CHECK(d->state == Door::Closed && g.objects.get(h)->state.m_Pos.x == 55);
        CHECK(g.objects.clear());
        h = door(g, {{"MoveDist", 0.0f},
                     {"MoveDir", DVector{}},
                     {"Speed", 0.0f},
                     {"TriggerClosed", false},
                     {"RemainsOpen", true},
                     {"SelfTrigger", true}});
        CHECK(h);
        CHECK(trigger(g, h, "TRIGGER") && steps(g, 3));
        d = dynamic_cast<Door*>(g.objects.get(h)->game.get());
        CHECK(d->state == Door::Open && !g.objects.get(h)->scheduled && d->self_trigger);
        CHECK(!door(g, {{"Waveform", std::int32_t(4)}}));
        CHECK(!door(g, {{"Speed", -1000001.0f}}));
        CHECK(!door(g, {{"MoveDir", DVector{}}}));
        CHECK(g.objects.finish_frame());
        CHECK(!g.objects.create(g.objects.find_class("Door"), {}, Creation::World, {{"Name", std::string("missing")}}));
        CHECK(g.objects.finish_frame());
        CHECK(g.objects.snapshot().size() == 2); // Door and its owned SelfTrigger.
        CHECK(g.objects.clear());
        h = door(g, {{"WaitTime", -1.0f}, {"TriggerClosed", false}});
        CHECK(h && trigger(g, h, "TRIGGER") && steps(g, 60));
        d = dynamic_cast<Door*>(g.objects.get(h)->game.get());
        CHECK(d->wait_time == -1 && d->state == Door::Closed);
        CHECK(g.objects.clear());
        h = door(g, {{"ClosingSpeed", -1.0f}});
        CHECK(h && trigger(g, h, "TRIGGER") && steps(g, 30));
        CHECK(trigger(g, h, "TRIGGERCLOSE") && steps(g, 2));
        CHECK(g.objects.get(h)->state.m_Pos.x > 55); // Signed source arithmetic, not a guessed sentinel.
        return true;
    }
    bool geometry(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g, {{"BoxPhysics", false}});
        CHECK(h);
        auto* r = g.objects.get(h);
        CHECK(r->state.m_ObjectType == OT_WORLDMODEL && r->world_model && !r->container);
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({-100, 20, 0}, {100, 20, 0}, nullptr, hit) == SegmentResult::Hit);
        CHECK(hit.object == h && std::abs(hit.point.x + 10) < .001f);
        CHECK(g.server.MoveWorldModel(h, {105, 20, 0}) == MoverResult::Moved);
        CHECK(g.server.IntersectSegment({-100, 20, 0}, {200, 20, 0}, nullptr, hit) == SegmentResult::Hit);
        CHECK(hit.object == h && std::abs(hit.point.x - 90) < .001f);
        DVector v{1, 1, 1};
        DRotation rotation{};
        CHECK(g.server.TeleportObject(h, &v) != DE_OK);
        CHECK(g.server.SetObjectRotation(h, &rotation) != DE_OK);
        CHECK(g.server.SetVelocity(h, &v) != DE_OK);
        CHECK(g.server.ResizeObject(h, v) == ResizeResult::Invalid);
        g.server.SetObjectDims(h, &v);
        CHECK(r->dimensions.x == 10);
        auto* d = dynamic_cast<Door*>(r->game.get());
        d->fire_through = true;
        CHECK(g.server.IntersectSegment({-100, 20, 0}, {200, 20, 0}, nullptr, hit, true) == SegmentResult::Miss);
        CHECK(g.objects.remove(h));
        CHECK(g.server.IntersectSegment({-100, 20, 0}, {200, 20, 0}, nullptr, hit) == SegmentResult::Miss);
        CHECK(g.server.MoveWorldModel(h, {}) == MoverResult::Invalid);
        CHECK(g.objects.finish_frame());
        // Authored rotation, polygon rather than bounds collision.
        auto model = test_brush::box("rotated", {-10, 10, -1}, {10, 30, 1});
        g.world.models.push_back(model);
        h = g.objects.create(
            g.objects.find_class("Door"), {}, Creation::World,
            {{"Name", std::string("rotated")}, {"BoxPhysics", false}, {"Rotation", EulerAngles{{0, .785398163f, 0}}}});
        CHECK(h);
        CHECK(g.server.IntersectSegment({6, 15, 6}, {6, 25, 6}, nullptr, hit) == SegmentResult::Miss);
        CHECK(g.server.IntersectSegment({6, 0, -6}, {6, 40, -6}, nullptr, hit) == SegmentResult::Hit);
        return true;
    }
    bool pushing(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g);
        auto b = body(g, {30, 20, 0}, {5, 5, 5});
        CHECK(h && b);
        CHECK(g.server.MoveWorldModel(h, {45, 20, 0}) == MoverResult::Moved);
        CHECK(std::abs(g.objects.get(b)->state.m_Pos.x - 55.002f) < .001f);
        CHECK(g.objects.get(h)->mover_pushes == 1);
        // A wall behind the pushed body blocks the entire transaction.
        auto wall = body(g, {80, 20, 0}, {5, 10, 20}, FLAG_SOLID);
        CHECK(wall);
        const auto before = g.objects.get(b)->state.m_Pos;
        CHECK(g.server.MoveWorldModel(h, {85, 20, 0}) == MoverResult::Blocked);
        CHECK(g.objects.get(h)->state.m_Pos.x == 45 && g.objects.get(b)->state.m_Pos.x == before.x);
        CHECK(g.objects.get(h)->mover_blocks == 1);
        CHECK(g.objects.remove(wall));
        CHECK(g.server.MoveWorldModel(h, {85, 20, 0}) == MoverResult::Moved);
        CHECK(g.objects.finish_frame());
        // A large single move still sweeps across bodies.
        CHECK(g.server.MoveWorldModel(h, {505, 20, 0}) == MoverResult::Moved);
        CHECK(g.objects.get(b)->state.m_Pos.x > 510);
        return true;
    }
    bool riders(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto h = door(g, {{"BoxPhysics", false}}, {-100, 10, -100}, {100, 30, 100});
        CHECK(h);
        auto* player = g.objects.get(g.client_shell.player);
        DVector pos{0, 30 + player->dimensions.y + .001f, 0};
        CHECK(g.server.TeleportObject(player->handle, &pos) == DE_OK);
        MovementState support;
        CHECK(g.server.GetMovementState(player->handle, support) && support.standing && support.ground_object == h);
        CHECK(trigger(g, h, "TRIGGER"));
        CHECK(steps(g, 15));
        CHECK(g.client_shell.position.x > 25 && g.client_shell.position.x < 32);
        CHECK(g.objects.get(h)->mover_riders >= 14);
        CHECK(std::abs(player->state.m_Pos.x - (g.objects.get(h)->state.m_Pos.x - 5)) < .01f);
        // No double integration; the shared body is observed by MoveMgr.
        CHECK(std::abs(g.client_shell.position.x - player->state.m_Pos.x) < .001f);
        CHECK(g.server.MoveWorldModel(h, vector_add(g.objects.get(h)->state.m_Pos, {0, -5, 0})) == MoverResult::Moved);
        CHECK(g.server.GetMovementState(player->handle, support) && support.standing && support.ground_object == h);
        // Jumping away must detach immediately.
        DVector velocity{0, 300, 0};
        CHECK(g.server.SetVelocity(player->handle, &velocity) == DE_OK);
        const auto before = player->state.m_Pos.x;
        CHECK(g.server.MoveWorldModel(h, vector_add(g.objects.get(h)->state.m_Pos, {5, 0, 0})) == MoverResult::Moved);
        CHECK(player->state.m_Pos.x == before);
        CHECK(g.objects.remove(h));
        CHECK(g.server.GetMovementState(player->handle, support) && !support.standing);
        CHECK(g.leave_player() && g.unload_world());
        return true;
    }
    std::function<void(BaseClass&, DDWORD, void*)> on_message;
    unsigned destroyed = 0;
    class Probe : public BaseClass
    {
      public:
        ~Probe() override
        {
            ++destroyed;
        }
        DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT value) override
        {
            if (on_message)
                on_message(*this, message, data);
            return BaseClass::EngineMessageFn(message, data, value);
        }
    };
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        static const ClassInfo probe{"MoverProbe",
                                     g.objects.find_class("BaseClass"),
                                     []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); },
                                     {}};
        CHECK(g.objects.register_class(probe));
        auto h = door(g);
        auto target =
            g.objects.create(&probe, {}, Creation::World,
                             {{"Pos", DVector{30, 20, 0}}, {"Flags", std::int32_t(FLAG_SOLID | FLAG_TOUCH_NOTIFY)}});
        CHECK(h && target);
        DVector dims{5, 5, 5};
        g.server.SetObjectDims(target, &dims);
        bool notified = false, recursive_rejected = false, pending = false;
        on_message = [&](BaseClass& object, DDWORD message, void* data)
        {
            if (message != MID_TOUCHNOTIFY)
                return;
            notified = object.m_hObject == target && data == h;
            recursive_rejected = g.server.MoveWorldModel(h, {100, 20, 0}) == MoverResult::Invalid;
            g.objects.remove(h);
            g.objects.remove(target);
            pending = destroyed == 0 && g.objects.get(h)->life == Life::Pending &&
                      g.objects.get(target)->life == Life::Pending && !g.unload_world();
        };
        CHECK(g.server.MoveWorldModel(h, {45, 20, 0}) == MoverResult::Moved);
        on_message = {};
        CHECK(notified && recursive_rejected && pending);
        CHECK(g.objects.finish_frame() && destroyed == 1);
        CHECK(!g.objects.get(h) && !g.objects.get(target));
        CHECK(g.server.MoveWorldModel(h, {}) == MoverResult::Invalid);
        // A ceiling must block a rising platform and its rider atomically.
        h = door(g, {}, {-20, 10, -20}, {20, 30, 20});
        target = body(g, {0, 32.001f, 0}, {2, 2, 2});
        WorldModel ceiling;
        ceiling.points = {{-100, 40, -100}, {100, 40, -100}, {100, 40, 100}, {-100, 40, 100}};
        ceiling.surfaces.resize(1);
        ceiling.surfaces[0].flags = 1;
        ceiling.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(ceiling, error));
        CHECK(g.server.MoveWorldModel(h, {5, 40, 0}) == MoverResult::Blocked);
        CHECK(g.objects.get(h)->state.m_Pos.y == 20 && g.objects.get(target)->state.m_Pos.y == 32.001f);
        CHECK(g.unload_world());
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        unsigned created = 0, rejected = 0, skipped = 0, self_triggers = 0, polygon = 0, compound = 0;
        std::map<std::string, unsigned> failures;
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
                ++skipped;
                continue;
            }
            CHECK(g.collision.build(g.world.models.front(), error));
            g.server.collision_ready = true;
            for (const auto& source : g.world.objects)
            {
                if (source.class_name != "Door")
                    continue;
                Properties props;
                for (const auto& property : source.properties)
                    props.emplace(property.name, property.value);
                auto h = g.objects.create(g.objects.find_class("Door"), {}, Creation::World, props);
                const auto* r = g.objects.get(h);
                if (!r || r->life != Life::Alive)
                {
                    ++rejected;
                    CHECK(!g.objects.diagnostics().empty());
                    const auto& diagnostic = g.objects.diagnostics().back();
                    if (diagnostic == "Door: invalid property TypeAction" && path.starts_with("worlds/multi/"))
                        ++failures["schema"];
                    else if (diagnostic.ends_with(": Ambiguous named world model"))
                        ++failures["ambiguous"];
                    else if (diagnostic.ends_with(": Missing named world model"))
                        ++failures["missing"];
                    else if (diagnostic.find("Open or non-manifold container shell") != std::string::npos)
                    {
                        ++failures["shell"];
                        CHECK(path == "worlds/multi/bb_condemned.dat" &&
                              property_text(props.at("Name")) == "Buildings");
                    }
                    else
                        CHECK(false); // A new failure must be investigated, not silently counted.
                    std::cout << path << " " << property_text(props.at("Name")) << " rejected: "
                              << (g.objects.diagnostics().empty() ? "initialization" : g.objects.diagnostics().back())
                              << "\n";
                }
                else
                {
                    const auto* d = dynamic_cast<const Door*>(r->game.get());
                    CHECK(d && r->world_model);
                    ++created;
                    self_triggers += d->self_trigger;
                    polygon += !d->box_physics;
                    if (r->world_model->shared_edges)
                    {
                        ++compound;
                        CHECK(r->world_model->closed_components > 1);
                        std::cout << path << " " << r->state.m_Name << ": " << r->world_model->closed_components
                                  << " closed components / " << r->world_model->shared_edges << " shared edges\n";
                    }
                    CHECK(g.objects.remove(h));
                }
                CHECK(g.objects.finish_frame());
            }
        }
        std::cout << "Door catalog: created=" << created << ", rejected=" << rejected << ", unreadable=" << skipped
                  << ", SelfTrigger=" << self_triggers << ", polygon=" << polygon << "\n";
        CHECK(created == 619 && rejected == 79 && skipped == 1 && self_triggers == 149 && polygon == 114 &&
              compound == 14);
        CHECK(failures["schema"] == 47 && failures["ambiguous"] == 19 && failures["missing"] == 12 &&
              failures["shell"] == 1);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        CHECK(g.request_world("01_SubwayA", error));
        unsigned count = 0, failures = 0;
        for (std::size_t i = 0; i < g.entries.size(); ++i)
            if (g.world.objects[i].class_name == "Door")
            {
                if (g.entries[i].state == EntryState::Created)
                    ++count;
                else
                {
                    ++failures;
                    std::cout << "Door rejected: " << g.entries[i].detail << "\n";
                }
            }
        std::cout << "SubwayA doors: " << count << " created, " << failures << " rejected\n";
        CHECK(count > 0 && failures == 0);
        // Freeze only the tested doors. Normal updates consume authored Key
        // objects, so an all-object snapshot cannot be dereferenced across steps.
        std::vector<ObjectId> doors;
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && dynamic_cast<Door*>(r->game.get()))
                doors.push_back(id);
        unsigned moved = 0;
        for (auto id : doors)
        {
            auto* r = g.objects.get(id);
            CHECK(r && r->life == Life::Alive);
            if (auto* d = dynamic_cast<Door*>(r->game.get()); d && d->move_dist > 0 && d->speed > 0)
            {
                const auto pos = r->state.m_Pos;
                CHECK(trigger(g, r->handle, "TRIGGER"));
                CHECK(steps(g, 4));
                if (vector_dot(vector_subtract(r->state.m_Pos, pos), vector_subtract(r->state.m_Pos, pos)) > 0)
                    ++moved;
                CHECK(trigger(g, r->handle, "TRIGGERCLOSE"));
                CHECK(steps(g, 20));
            }
        }
        std::cout << "SubwayA moving doors: " << moved << "\n";
        CHECK(moved > 0);
        CHECK(g.enter_player(error) && steps(g, 2));
        CHECK(g.leave_player() && g.unload_world());
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0 && g.objects.snapshot().empty());
        CHECK(g.request_world("01_SubwayA", error));
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"state", state},   {"options", options},   {"geometry", geometry},    {"pushing", pushing},
        {"riders", riders}, {"lifetime", lifetime}, {"private", private_data}, {"catalog", catalog}};
    auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
