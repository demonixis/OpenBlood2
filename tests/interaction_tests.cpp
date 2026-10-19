#include "app/movement_controls.h"
#include "brush_fixture.h"
#include "engine/world_model.h"
#include "game/original/RotatingDoor.h"
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
            std::cerr << __func__ << ":" << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    bool near(float a, float b, float e = .01f)
    {
        return std::abs(a - b) < e;
    }
    HOBJECT door(GameSession& g, Properties props = {}, DVector lo = {0, 20, -2}, DVector hi = {40, 80, 2})
    {
        const auto name = "hinge" + std::to_string(g.world.models.size());
        auto model = test_brush::box(name, lo, hi);
        model.translation = {0, 50, 0};
        g.world.models.push_back(model);
        props["Name"] = name;
        props.try_emplace("Speed", 90.0f);
        props.try_emplace("RotationPoint", DVector{0, 50, 0});
        props.try_emplace("RotationAngles", DVector{0, 90, 0});
        props.try_emplace("TriggerClosed", true);
        return g.objects.create(g.objects.find_class("RotatingDoor"), {}, Creation::World, props);
    }
    HOBJECT body(GameSession& g, DVector position, DVector dims, DDWORD flags = FLAG_SOLID)
    {
        auto h = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                  {{"Pos", position}, {"Flags", std::int32_t(flags)}});
        if (h)
            g.server.SetObjectDims(h, &dims);
        return h;
    }
    bool command(GameSession& g, HOBJECT h, const char* text = "TRIGGER")
    {
        auto str = g.server.CreateString(text);
        auto msg = g.server.StartMessageToObject(nullptr, h, 1000);
        CHECK(msg && g.server.WriteToMessageHString(msg, str) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        g.server.FreeString(str);
        return true;
    }
    bool steps(GameSession& g, unsigned n, double dt = .02)
    {
        for (unsigned i = 0; i < n; ++i)
            CHECK(g.step(dt));
        return true;
    }
    bool use(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        DVector pos{0, 50, 0};
        CHECK(g.server.TeleportObject(p->m_hObject, &pos) == DE_OK);
        auto t = g.objects.create(g.objects.find_class("Trigger"), {}, Creation::World,
                                  {{"Pos", DVector{0, 86, 60}}, {"Dims", DVector{5, 5, 5}}, {"ResetTime", 0.0f}});
        CHECK(t);
        auto* trigger = dynamic_cast<Trigger*>(g.objects.get(t)->game.get());
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerUse{{0, 0}}));
        CHECK(p->use_hit.object == t && trigger->activations == 1 && near(p->use_hit.fraction * 82, 55));
        CHECK(near(p->use_to.z, 82) && near(p->use_from.y, 86));
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerUse{{0, pi}}));
        CHECK(p->use_result == SegmentResult::Miss);
        g.objects.get(t)->state.m_Pos.z = 88;
        p->m_fYaw = 0;
        CHECK(p->OnCommandUse());
        CHECK(p->use_result == SegmentResult::Miss);
        g.objects.get(t)->state.m_Pos.z = 60;
        auto enclosing = g.objects.create(g.objects.find_class("Trigger"), {}, Creation::World,
                                          {{"Pos", DVector{0, 86, 0}}, {"Dims", DVector{10, 10, 10}}});
        CHECK(enclosing);
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerUse{}));
        CHECK(p->use_hit.object == t);
        auto wall = body(g, {0, 86, 30}, {10, 10, 1});
        CHECK(wall);
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerUse{{0, 0}}));
        CHECK(p->use_hit.object == wall);
        CHECK(g.objects.remove(wall));
        CHECK(p->OnCommandUse() && p->use_hit.object == t);
        p->eye_level = 12;
        CHECK(p->OnCommandUse() && p->use_result == SegmentResult::Miss && near(p->use_from.y, 62));
        g.objects.get(t)->state.m_Pos.y = 62;
        CHECK(p->OnCommandUse() && p->use_hit.object == t);
        p->movement_blocked = true;
        auto calls = p->use_commands;
        CHECK(!p->OnCommandUse() && p->use_commands == calls);
        p->movement_blocked = false;
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerUse{{NAN, 0}}));
        CHECK(g.objects.remove(p->m_hObject));
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerUse{}));
        CHECK(g.step(0) && !g.local_client);
        return true;
    }
    bool input(const std::filesystem::path& root)
    {
        MovementControls controls;
        controls.focused = controls.enabled = true;
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = 1;
        e.key.scancode = SDL_SCANCODE_E;
        CHECK(controls.consumes(e, false));
        controls.event(e, 1);
        CHECK(controls.sample_use(false));
        CHECK(!controls.sample_use(true));
        CHECK(!controls.sample_use(false));
        e.type = SDL_EVENT_KEY_UP;
        controls.event(e, 1);
        CHECK(!controls.sample_use(false));
        e.type = SDL_EVENT_KEY_DOWN;
        controls.event(e, 1);
        CHECK(controls.sample_use(false));
        e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        e.window.windowID = 1;
        controls.event(e, 1);
        CHECK(!controls.sample_use(false));
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto& c = g.client_shell;
        c.use_down = true;
        CHECK(c.UpdateUse(g.server_shell));
        auto n = p->use_commands;
        CHECK(n == 1);
        CHECK(c.UpdateUse(g.server_shell) && p->use_commands == n);
        c.use_down = false;
        CHECK(c.UpdateUse(g.server_shell));
        c.use_down = true;
        CHECK(c.UpdateUse(g.server_shell) && p->use_commands == n + 1);
        CHECK(g.leave_player() && !c.use_down && !c.use_was_down);
        return true;
    }
    bool keys(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        DVector pos{20, 50, -60};
        CHECK(g.server.TeleportObject(p->m_hObject, &pos) == DE_OK);
        p->eye_level = 12;
        auto h = door(g);
        CHECK(h);
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerUse{}));
        CHECK(p->use_hit.object == h && p->use_door_refused);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        h = door(g, {{"SelfTrigger", true},
                     {"Locked", true},
                     {"UnlockKeyName", std::string("LabKey")},
                     {"TriggerDims", DVector{45, 40, 4}}});
        CHECK(h);
        auto* d = dynamic_cast<RotatingDoor*>(g.objects.get(h)->game.get());
        auto* t = dynamic_cast<Trigger*>(g.objects.get(d->trigger)->game.get());
        CHECK(p->OnCommandUse() && p->use_hit.object == d->trigger && d->state == Door::Closed && t->locked);
        p->m_InventoryMgr.AddKey({"labkey", "lab", "key.pcx", "key_h.pcx", 1});
        CHECK(p->OnCommandUse() && d->state == Door::Opening && !t->locked && p->m_InventoryMgr.keys.empty());
        CHECK(steps(g, 30));
        CHECK(d->state == Door::Open);
        CHECK(command(g, h, "TRIGGERCLOSE") && steps(g, 30) && d->state == Door::Closed && t->active);
        return true;
    }
    bool state(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g, {{"SelfTrigger", true}});
        CHECK(h);
        auto* d = dynamic_cast<RotatingDoor*>(g.objects.get(h)->game.get());
        CHECK(near(d->speed, pi / 2) && !d->box_physics && d->push_player_back);
        CHECK(command(g, h) && steps(g, 1));
        CHECK(near(d->angles.y, .02f * pi, .0001f));
        CHECK(steps(g, 26) && d->state == Door::Open && near(d->angles.y, pi / 2));
        CHECK(command(g, h, "TRIGGERCLOSE") && steps(g, 27));
        CHECK(d->state == Door::Closed && near(d->angles.y, 0));
        CHECK(g.objects.clear());
        h = door(g, {{"StartOpen", true}, {"SelfTrigger", true}});
        CHECK(h);
        d = dynamic_cast<RotatingDoor*>(g.objects.get(h)->game.get());
        CHECK(d->state == Door::Closed &&
              d->angles.y == 0); // Source parent StartOpen changes only translational endpoints.
        CHECK(command(g, h) && steps(g, 27) && d->state == Door::Open);
        float a = 0;
        CHECK(!RotatingDoor::CalcAngle(a, 0, 1, 1, 1, 0, .6f) && near(a, 1.2f));
        CHECK(RotatingDoor::CalcAngle(a, 0, 1, 1, 1, 0, .6f) && a == 1);
        a = 0;
        CHECK(RotatingDoor::CalcAngle(a, 0, 0, 1, 1, 1, .1f) && a == 0);
        CHECK(g.unload_world());
        CHECK(g.server.string_count() == 0);
        return true;
    }
    bool geometry(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g);
        CHECK(h);
        auto* r = g.objects.get(h);
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({20, 50, -60}, {20, 50, 60}, nullptr, hit) == SegmentResult::Hit &&
              hit.object == h);
        CHECK(g.server.RotateWorldModel(h, {{}, {0, pi / 2, 0}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Moved);
        CHECK(near(r->dimensions.x, 2) && near(r->dimensions.z, 20));
        CHECK(g.server.IntersectSegment({20, 50, -60}, {20, 50, 60}, nullptr, hit) == SegmentResult::Miss);
        CHECK(g.server.IntersectSegment({-60, 50, -20}, {60, 50, -20}, nullptr, hit) == SegmentResult::Hit &&
              hit.object == h);
        CHECK(near(hit.point.x, -2));
        CHECK(g.server.RotateWorldModel(h, {{0, pi / 2, 0}, {}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Moved);
        CHECK(near(r->world_model->min.x, 0) && near(r->world_model->max.x, 40));
        CHECK(g.server.RotateWorldModel(h, {{}, {0, 1000, 0}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Unavailable);
        CHECK(g.server.RotateWorldModel(h, {{NAN, 0, 0}, {}, {}, {}}) == MoverResult::Invalid);
        CHECK(g.objects.clear());
        h = door(g, {{"Rotation", EulerAngles{{0, .3f, 0}}}, {"RotationAngles", DVector{90, 0, 45}}});
        CHECK(h);
        r = g.objects.get(h);
        const auto basis = r->world_model_basis;
        CHECK(g.server.RotateWorldModel(h, {{0, .3f, 0}, {.4f, .3f, .2f}, {0, 50, 0}, {-10, 50, 0}}) ==
              MoverResult::Moved);
        auto expected =
            vector_add(DVector{-10, 50, 0}, rotate_vector({10, 0, 0}, rotation_from_euler({.4f, .3f, .2f})));
        CHECK(near(r->state.m_Pos.x, expected.x) && near(r->state.m_Pos.z, expected.z));
        auto vertex = r->world_model_base->triangles[0][0];
        vertex = vector_add(expected, rotate_vector(vector_subtract(vertex, r->world_model_base->origin),
                                                    multiply_rotation(r->state.m_Rotation, inverse_rotation(basis))));
        CHECK(near(vertex.x, r->world_model->triangles[0][0].x) && near(vertex.z, r->world_model->triangles[0][0].z));
        return true;
    }
    bool transport(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g);
        CHECK(h);
        auto rider = body(g, {20, 82.001f, 0}, {2, 2, 2});
        CHECK(rider);
        CHECK(g.server.RotateWorldModel(h, {{}, {0, .5f, 0}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Moved);
        auto* r = g.objects.get(rider);
        CHECK(near(r->state.m_Pos.x, 20 * std::cos(.5f), .03f));
        CHECK(near(r->state.m_Pos.z, -20 * std::sin(.5f), .03f) && g.objects.get(h)->mover_riders == 1);
        CHECK(g.objects.clear());
        h = door(g);
        CHECK(h);
        auto detached = body(g, {20, 82.001f, 0}, {2, 2, 2}, FLAG_SOLID | FLAG_DONTFOLLOWSTANDING);
        CHECK(detached);
        CHECK(g.server.RotateWorldModel(h, {{}, {0, .5f, 0}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Moved);
        CHECK(g.objects.get(detached)->state.m_Pos.x == 20 && g.objects.get(detached)->state.m_Pos.z == 0);
        CHECK(g.objects.clear());
        h = door(g);
        CHECK(h);
        auto b = body(g, {20, 50, -6}, {2, 2, 2});
        CHECK(b);
        CHECK(g.server.RotateWorldModel(h, {{}, {0, .3f, 0}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Moved);
        CHECK(g.objects.get(b)->state.m_Pos.z < -6 && g.objects.get(h)->mover_pushes == 1);
        return true;
    }
    bool blocking(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g);
        CHECK(h);
        auto b = body(g, {20, 50, -6}, {2, 2, 2});
        CHECK(b);
        auto wall = test_brush::box("obstruction", {0, 10, -12}, {40, 90, -11});
        std::string error;
        for (auto& surface : wall.surfaces)
            surface.flags = 1;
        CHECK(g.collision.build(wall, error));
        auto* r = g.objects.get(h);
        auto initial = r->world_model;
        CHECK(g.server.RotateWorldModel(h, {{}, {0, pi / 2, 0}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Blocked);
        CHECK(r->world_model == initial && near(r->state.m_Rotation.m_Spin, 1) &&
              near(g.objects.get(b)->state.m_Pos.z, -6));
        CHECK(r->mover_pushes == 0 && r->mover_blocks == 1);
        CHECK(g.objects.remove(h));
        CHECK(g.server.RotateWorldModel(h, {}) == MoverResult::Invalid);
        CHECK(g.objects.finish_frame());
        CHECK(g.objects.snapshot().size() == 2); // start + body
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = door(g, {{"SelfTrigger", true}});
        CHECK(h);
        auto* d = dynamic_cast<RotatingDoor*>(g.objects.get(h)->game.get());
        auto child = d->trigger;
        auto b = body(g, {20, 50, -6}, {2, 2, 2});
        CHECK(b);
        auto sensor = g.objects.create(g.objects.find_class("Trigger"), {}, Creation::World,
                                       {{"Name", std::string("remover")},
                                        {"Pos", DVector{20, 50, -8}},
                                        {"Dims", DVector{5, 5, 1}},
                                        {"TouchActivate", true},
                                        {"ObjectActivate", true},
                                        {"TargetName1", std::string(g.objects.get(h)->state.m_Name)},
                                        {"MessageName1", std::string("REMOVE")}});
        CHECK(sensor);
        CHECK(g.server.RotateWorldModel(h, {{}, {0, .3f, 0}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Moved);
        CHECK(g.objects.get(h)->life == Life::Pending);
        CHECK(g.objects.finish_frame() && !g.objects.get(h) && !g.objects.get(child));
        CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool player_push(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto* r = g.objects.get(p->m_hObject);
        auto h = door(g);
        CHECK(h);
        CHECK(command(g, h));
        DVector position{20, 80 + r->dimensions.y + .001f, 0};
        CHECK(g.server.TeleportObject(p->m_hObject, &position) == DE_OK);
        CHECK(g.server.RotateWorldModel(h, {{}, {0, .2f, 0}, {0, 50, 0}, {0, 50, 0}}) == MoverResult::Moved);
        CHECK(p->door_push_pending && p->door_pushes == 1);
        CHECK(near(std::sqrt(vector_dot(p->door_push_velocity, p->door_push_velocity)), 500, .01f));
        CHECK(p->ApplyDoorPush() && !p->door_push_pending);
        auto velocity = r->velocity;
        CHECK(p->ApplyDoorPush() && r->velocity.x == velocity.x && r->velocity.z == velocity.z);
        auto* d = dynamic_cast<RotatingDoor*>(g.objects.get(h)->game.get());
        d->push_player_back = false;
        CHECK(g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, h));
        CHECK(!p->door_push_pending);
        CHECK(g.unload_world());
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        unsigned created = 0, rejected = 0, filtered = 0, skipped = 0, children = 0, moved = 0;
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
                if (source.class_name != "RotatingDoor")
                    continue;
                Properties props;
                for (const auto& prop : source.properties)
                    props.emplace(prop.name, prop.value);
                auto h = g.objects.create(g.objects.find_class("RotatingDoor"), {}, Creation::World, props);
                auto* r = g.objects.get(h);
                if (!r || r->life != Life::Alive)
                {
                    if (r && dynamic_cast<B2BaseClass*>(r->game.get())->filtered)
                        ++filtered;
                    else
                    {
                        ++rejected;
                        const auto diagnostic = g.objects.diagnostics().back();
                        if (diagnostic == "RotatingDoor: invalid property TypeAction")
                            ++failures["schema"];
                        else if (diagnostic.ends_with(": Missing named world model"))
                            ++failures["missing"];
                        else if (diagnostic.ends_with(": Ambiguous named world model"))
                            ++failures["ambiguous"];
                        else
                            CHECK(false);
                        std::cout << path << " " << property_text(props.at("Name"))
                                  << " rejected: " << g.objects.diagnostics().back() << "\n";
                    }
                }
                else
                {
                    auto* d = dynamic_cast<RotatingDoor*>(r->game.get());
                    CHECK(d && r->world_model);
                    ++created;
                    children += bool(d->trigger);
                    CHECK(command(g, h) && steps(g, 4));
                    if (r->mover_moves)
                        ++moved;
                    CHECK(g.objects.remove(h));
                }
                CHECK(g.objects.finish_frame() && g.objects.snapshot().empty());
            }
        }
        std::cout << "RotatingDoor catalog created=" << created << " rejected=" << rejected << " filtered=" << filtered
                  << " unreadable=" << skipped << " children=" << children << " moved=" << moved << "\n";
        for (const auto& [kind, count] : failures)
            std::cout << kind << "=" << count << "\n";
        CHECK(created == 243 && rejected == 8 && filtered == 0 && skipped == 0 && children == 163 && moved == 235);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"use", use},           {"input", input},
        {"keys", keys},         {"state", state},
        {"geometry", geometry}, {"transport", transport},
        {"blocking", blocking}, {"catalog", catalog},
        {"lifetime", lifetime}, {"player_push", player_push}};
    auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
