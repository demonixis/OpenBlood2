#include "engine/container.h"
#include "game/original/VolumeBrush.h"
#include "game/original/trigger_helpers.h"
#include "player_fixture.h"
#include <algorithm>
#include <cmath>
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
    void face(WorldModel& model, std::vector<DVector> points, DVector normal)
    {
        const auto surface = static_cast<unsigned>(model.surfaces.size());
        model.planes.push_back({normal, vector_dot(normal, points[0])});
        model.surfaces.push_back({});
        model.surfaces.back().plane = surface;
        Polygon polygon;
        polygon.surface = surface;
        for (auto p : points)
        {
            polygon.points.push_back(static_cast<std::uint16_t>(model.points.size()));
            model.points.push_back(p);
        }
        model.polygons.push_back(std::move(polygon));
    }
    WorldModel prism(std::string name, const std::vector<DVector>& outline, float bottom, float top)
    {
        WorldModel m;
        m.name = std::move(name);
        for (std::size_t i = 0; i < outline.size(); ++i)
        {
            const auto a = outline[i], b = outline[(i + 1) % outline.size()];
            const auto length = std::hypot(b.x - a.x, b.z - a.z);
            face(m, {{a.x, bottom, a.z}, {b.x, bottom, b.z}, {b.x, top, b.z}, {a.x, top, a.z}},
                 {(b.z - a.z) / length, 0, (a.x - b.x) / length});
        }
        std::vector<DVector> lo, hi;
        for (auto p : outline)
        {
            lo.push_back({p.x, bottom, p.z});
            hi.push_back({p.x, top, p.z});
        }
        face(m, lo, {0, -1, 0});
        face(m, hi, {0, 1, 0});
        return m;
    }
    WorldModel box(std::string name, DVector min, DVector max)
    {
        return prism(std::move(name), {{min.x, 0, min.z}, {max.x, 0, min.z}, {max.x, 0, max.z}, {min.x, 0, max.z}},
                     min.y, max.y);
    }
    HOBJECT volume(GameSession& game, std::string type, std::string name, DVector min, DVector max,
                   Properties extra = {})
    {
        game.world.models.push_back(box(name, min, max));
        extra["Name"] = name;
        return game.objects.create(game.objects.find_class(type), {}, Creation::World, extra);
    }
    bool enter(GameSession& game, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(game, root));
        CHECK(game.enter_player(error));
        game.client_shell.rotation = {};
        return true;
    }
    bool place(GameSession& game, DVector position)
    {
        std::string error;
        auto* player = game.server_shell.Player(game.local_client);
        CHECK(player);
        CHECK(game.server.TeleportObject(player->m_hObject, &position) == DE_OK);
        CHECK(player->TeleportClientToServerPos());
        CHECK(game.pump_player_messages(error));
        CHECK(game.client_shell.Update());
        CHECK(player->UpdateContainerState());
        return true;
    }
    bool steps(GameSession& game, unsigned count, double dt = 1.0 / 60)
    {
        for (unsigned i = 0; i < count; ++i)
        {
            if (!game.step(dt))
            {
                for (const auto& d : game.objects.diagnostics())
                    std::cerr << d << "\n";
                for (const auto& t : game.trace)
                    std::cerr << t << "\n";
                return false;
            }
        }
        return true;
    }
    bool geometry(const std::filesystem::path&)
    {
        auto model = box("test", {-10, -10, -10}, {10, 10, 10});
        model.translation = {100, 200, 300};
        ContainerShape shape;
        std::string error;
        CHECK(shape.build(model, {}, error));
        CHECK(shape.convex);
        auto doubled = model;
        for (auto& polygon : doubled.polygons)
        {
            polygon.base_points = static_cast<std::uint16_t>(polygon.points.size());
            auto extra = polygon.points;
            polygon.points.insert(polygon.points.end(), extra.begin(), extra.end());
        }
        CHECK(shape.build(doubled, {}, error));
        CHECK(shape.contains({0, 0, 0}));
        CHECK(shape.contains({10, 0, 0}));
        CHECK(!shape.contains({10.01f, 0, 0}));
        CHECK(shape.overlaps({12, 0, 0}, {3, 1, 1}));
        CHECK(!shape.overlaps({12, 0, 0}, {1, 1, 1}));
        CHECK(!shape.contains({100, 200, 300})); // Never add the DAT translation twice.
        model.polygons.pop_back();
        CHECK(!shape.build(model, {}, error));
        CHECK(shape.triangles.empty());
        model = box("test", {-10, -10, -10}, {10, 10, 10});
        model.polygons[0].points[0] = 65535;
        CHECK(!shape.build(model, {}, error));
        model = box("rotated", {-10, -1, -1}, {10, 1, 1});
        CHECK(shape.build(model, rotation_from_euler({0, .785398163f, 0}), error));
        CHECK(!shape.contains({6, 0, 6}));
        CHECK(!shape.overlaps({6, 0, 6}, {.1f, .1f, .1f}));
        CHECK(shape.contains({6, 0, -6}));
        return true;
    }
    bool concave(const std::filesystem::path&)
    {
        // L-shaped extrusion; top/bottom split into convex faces with T junctions.
        auto model = prism("L", {{0, 0, 0}, {4, 0, 0}, {4, 0, 1}, {1, 0, 1}, {1, 0, 4}, {0, 0, 4}}, 0, 4);
        model.polygons.resize(6);
        model.surfaces.resize(6);
        model.planes.resize(6);
        for (float y : {0.0f, 4.0f})
        {
            DVector n{0, y == 0 ? -1.0f : 1.0f, 0};
            face(model, {{0, y, 0}, {4, y, 0}, {4, y, 1}, {0, y, 1}}, n);
            face(model, {{0, y, 1}, {1, y, 1}, {1, y, 4}, {0, y, 4}}, n);
        }
        ContainerShape shape;
        std::string error;
        CHECK(shape.build(model, {}, error));
        CHECK(!shape.convex);
        CHECK(shape.contains({.5f, 2, 3}));
        CHECK(shape.contains({3, 2, .5f}));
        CHECK(!shape.contains({3, 2, 3}));
        CHECK(!shape.overlaps({3, 2, 3}, {.2f, .2f, .2f}));
        CHECK(shape.overlaps({1.1f, 2, 3}, {.2f, .2f, .2f}));
        CHECK(shape.overlaps({2, 2, 2}, {5, 5, 5}));
        return true;
    }
    bool membership(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto water = volume(game, "Water", "water", {-200, 0, -200}, {200, 100, 200});
        CHECK(water);
        auto* brush = dynamic_cast<VolumeBrush*>(game.objects.get(water)->game.get());
        CHECK(brush && brush->info.viscosity == .5f);
        CHECK(place(game, {0, 80, 0}));
        auto* player = game.server_shell.Player(game.local_client);
        CHECK(player->containers.count == 1 && player->containers.head.code == CC_NOTHING);
        CHECK(place(game, {0, 50, 0}));
        CHECK(player->containers.head.code == CC_WATER);
        auto name = game.server.CreateString("water"), command = game.server.CreateString("OFF");
        SendTriggerMsgToObjects(player, name, command);
        game.server.FreeString(command);
        CHECK(player->UpdateContainerState());
        CHECK(player->containers.count == 1 && player->containers.body[0].hidden);
        CHECK(player->containers.head.code == CC_NOTHING);
        command = game.server.CreateString("TOGGLE");
        SendTriggerMsgToObjects(player, name, command);
        game.server.FreeString(command);
        game.server.FreeString(name);
        CHECK(player->UpdateContainerState());
        CHECK(player->containers.head.code == CC_WATER);
        for (unsigned i = 0; i < 5; ++i)
            CHECK(volume(game, "Ladder", "ladder" + std::to_string(i), {-20, 0, -20}, {20, 200, 20}));
        CHECK(player->UpdateContainerState());
        CHECK(player->containers.count == 4 && player->containers.total == 6);
        CHECK(game.objects.remove(water));
        CHECK(player->UpdateContainerState());
        CHECK(player->containers.head.code == CC_LADDER);
        CHECK(game.objects.finish_frame());
        ContainerState state;
        CHECK(!game.server.GetContainers(water, {}, state));
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        CHECK(!game.objects.create(game.objects.find_class("Water"), {}, Creation::World,
                                   {{"Name", std::string("missing")}}));
        CHECK(game.objects.finish_frame());
        auto h = volume(game, "Ladder", "ladder", {0, 0, 0}, {10, 200, 10});
        CHECK(h);
        auto shape = game.objects.get(h)->container;
        DVector pos{100, 100, 100};
        DRotation rot;
        CHECK(game.server.ResizeObject(h, {10, 10, 10}) == ResizeResult::Invalid);
        CHECK(game.server.TeleportObject(h, &pos) != DE_OK);
        CHECK(game.server.SetObjectRotation(h, &rot) != DE_OK);
        CHECK(game.server.SetVelocity(h, &pos) != DE_OK && game.server.SetAcceleration(h, &pos) != DE_OK);
        game.server.SetObjectFlags(h, FLAG_SOLID);
        CHECK(!(game.objects.get(h)->state.m_Flags & FLAG_SOLID));
        game.server.SetObjectDims(h, &pos);
        CHECK(game.objects.get(h)->dimensions.x == 5);
        CHECK(game.objects.remove(h));
        CHECK(game.objects.finish_frame());
        CHECK(shape->contains({5, 50, 5}));
        CHECK(game.unload_world());
        CHECK(!game.objects.get(h));
        CHECK(game.server.string_count() == 0);
        CHECK(game.server.message_count() == 0);
        return true;
    }
    bool physics(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        auto h = volume(game, "Water", "water", {-200, 0, -200}, {200, 500, 200}, {{"Current", DVector{120, 0, 0}}});
        CHECK(h);
        ObjectCreateStruct create;
        create.m_Pos = {0, 200, 0};
        create.m_Flags = FLAG_GRAVITY;
        auto object = game.objects.create(game.objects.find_class("BaseClass"), create);
        CHECK(object);
        DVector dims{1, 1, 1}, v{100, 0, 0};
        game.server.SetObjectDims(object, &dims);
        CHECK(game.server.SetVelocity(object, &v) == DE_OK);
        CHECK(game.server.advance_physics(1.0 / 120));
        auto* record = game.objects.get(object);
        CHECK(std::abs(record->velocity.x - 51) < .001 && std::abs(record->velocity.y + 200.0 / 120) < .001);
        CHECK(record->state.m_Flags & FLAG_GRAVITY); // Per-step suppression never destroys the original flag.
        auto* brush = dynamic_cast<VolumeBrush*>(game.objects.get(h)->game.get());
        brush->info.hidden = true;
        CHECK(game.server.advance_physics(1.0 / 120));
        CHECK(record->velocity.y < -18);
        brush->info.hidden = false;
        brush->info.viscosity = 1;
        v = {100, 0, 0};
        game.server.SetVelocity(object, &v);
        CHECK(game.server.advance_physics(1.0 / 120));
        CHECK(std::abs(record->velocity.x - 101) < .001); // Source endpoint quirk.
        return true;
    }
    bool swimming(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        CHECK(volume(game, "Water", "water", {-500, 0, -500}, {500, 1000, 500}));
        CHECK(place(game, {0, 400, 0}));
        auto& client = game.client_shell;
        auto* player = game.server_shell.Player(game.local_client);
        client.movement_input.forward = true;
        client.movement_input.run = true;
        CHECK(steps(game, 30));
        CHECK(client.move_mgr.m_bBodyInLiquid && !client.move_mgr.m_bUseGravity);
        CHECK(client.position.z > 50 && client.position.z < 110);
        CHECK(client.move_mgr.m_fSwimVel == 175);
        const auto y = client.position.y;
        client.movement_input = {};
        client.movement_input.jump = true;
        CHECK(steps(game, 30));
        CHECK(client.position.y > y + 10 && !player->crouched);
        client.movement_input = {};
        client.movement_input.crouch = true;
        CHECK(steps(game, 30));
        CHECK(client.move_mgr.body.velocity.y < 0 && player->crouched);
        CHECK(player->m_fAirLevel < 91 && player->m_fAirLevel > 89);
        client.movement_input = {};
        client.movement_input.forward = true;
        client.rotation.pitch = -1.2f;
        CHECK(steps(game, 30));
        CHECK(client.move_mgr.body.velocity.y > 0);
        return true;
    }
    bool surface(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        CHECK(volume(game, "Water", "water", {-500, 0, -500}, {500, 300, 500}));
        CHECK(place(game, {0, 250, 0}));
        CHECK(game.step(1.0 / 60));
        CHECK(game.client_shell.move_mgr.m_bCanSwimJump);
        // Cross the water surface without force-position resetting the source transition state.
        DVector p{0, 280, 0}, v{};
        CHECK(game.server.TeleportObject(game.client_shell.player, &p) == DE_OK);
        CHECK(game.server.SetVelocity(game.client_shell.player, &v) == DE_OK);
        CHECK(game.step(1.0 / 60));
        CHECK(game.client_shell.move_mgr.m_bSwimmingOnSurface);
        game.client_shell.movement_input.jump = true;
        CHECK(game.step(1.0 / 60));
        CHECK(game.client_shell.move_mgr.m_bSwimmingJump && game.client_shell.move_mgr.body.velocity.y > 400);
        game.client_shell.movement_input = {};
        CHECK(place(game, {0, 41.001f, 0}));
        // Head submerged keeps free movement even when supported.
        CHECK(game.step(1.0 / 60));
        CHECK(game.client_shell.move_mgr.m_bBodyInLiquid);
        return true;
    }
    bool ladders(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto ladder = volume(game, "Ladder", "ladder", {-30, 0, -30}, {30, 1500, 30});
        CHECK(ladder);
        CHECK(place(game, {0, 300, 0}));
        auto& client = game.client_shell;
        client.movement_input.jump = true;
        CHECK(steps(game, 60));
        CHECK(client.position.y > 430 && client.position.y < 550);
        CHECK(client.move_mgr.m_bBodyOnLadder && !client.move_mgr.m_bUseGravity);
        client.movement_input = {};
        CHECK(steps(game, 30));
        CHECK(std::abs(client.move_mgr.body.velocity.y) < 1);
        const auto y = client.position.y;
        client.movement_input.crouch = true;
        CHECK(steps(game, 30));
        CHECK(client.position.y < y - 50 && game.server_shell.Player(game.local_client)->crouched);
        client.movement_input = {};
        client.rotation.pitch = -1.57079632679f;
        client.movement_input.forward = true;
        CHECK(steps(game, 30));
        CHECK(client.move_mgr.body.velocity.y > 100);
        CHECK(place(game, {200, 400, 0}));
        client.movement_input = {};
        CHECK(game.step(.1));
        CHECK(!client.move_mgr.m_bBodyOnLadder && client.move_mgr.m_bUseGravity &&
              client.move_mgr.body.velocity.y < -150);
        return true;
    }
    bool air_damage(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        CHECK(volume(game, "Blood", "blood", {-500, 0, -500}, {500, 1000, 500}));
        CHECK(place(game, {0, 400, 0}));
        auto* player = game.server_shell.Player(game.local_client);
        player->m_damage.SetArmorPoints(100);
        const auto health = player->m_damage.m_fHitPoints;
        for (unsigned i = 0; i < 150; ++i)
            CHECK(player->UpdateAirLevel(.1f));
        CHECK(player->m_fAirLevel < .001f);
        CHECK(player->UpdateAirLevel(.1f));
        CHECK(player->m_damage.m_fHitPoints < health && player->m_damage.m_fArmorPoints == 100);
        CHECK(player->m_damage.last_damage.type == DamageType::Suffocate);
        CHECK(place(game, {700, 400, 0}));
        for (unsigned i = 0; i < 26; ++i)
            CHECK(player->UpdateAirLevel(.1f));
        CHECK(player->m_fAirLevel == 100);
        auto acid = volume(game, "Acid", "acid", {600, 0, -100}, {800, 600, 100},
                           {{"Damage", 10.0f}, {"DamageType", std::int32_t(14)}});
        CHECK(acid);
        CHECK(game.objects.advance(.11));
        CHECK(game.server.damage_containers());
        CHECK(player->m_damage.last_damage.type == DamageType::Acid && player->m_damage.m_fArmorPoints < 100);
        CHECK(game.server.message_count() == 0);
        auto death = volume(game, "VolumeBrush", "death", {600, 0, -100}, {800, 600, 100},
                            {{"Code", std::int32_t(CC_DAMAGE)}, {"Damage", 1.0f}, {"DamageType", std::int32_t(7)}});
        CHECK(death);
        CHECK(game.objects.advance(.11));
        CHECK(game.server.damage_containers());
        CHECK(player->m_damage.IsDead() && player->m_damage.m_fHitPoints == 0 && player->m_damage.m_fArmorPoints == 0);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession game;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game.initialize());
        CHECK(game.resources.mount_game(config, error));
        unsigned volumes = 0, nonconvex = 0, failed = 0;
        for (const auto& [path, resource] : game.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World world;
            CHECK(game.resources.read(path, bytes, error));
            if (!read_world(bytes, world, error))
            {
                std::cout << "Skipped unsupported DAT " << path << ": " << error << "\n";
                continue;
            }
            for (const auto& object : world.objects)
            {
                if (object.class_name != "VolumeBrush" && object.class_name != "Water" &&
                    object.class_name != "Blood" && object.class_name != "Acid" && object.class_name != "Ladder")
                    continue;
                const auto* name = object.find("Name");
                CHECK(name && std::holds_alternative<std::string>(*name));
                auto model = std::find_if(world.models.begin(), world.models.end(),
                                          [&](const auto& m) { return m.name == std::get<std::string>(*name); });
                if (model == world.models.end())
                {
                    std::cout << "Missing named volume model " << path << " " << std::get<std::string>(*name) << "\n";
                    continue;
                }
                ContainerShape shape;
                DRotation rotation;
                if (auto* property = object.find("Rotation"))
                {
                    GenericProp value;
                    CHECK(generic_property(*property, value));
                    rotation = value.m_Rotation;
                }
                ++volumes;
                if (!shape.build(*model, rotation, error))
                {
                    ++failed;
                    std::cerr << path << " " << model->name << ": " << error << "\n";
                }
                else
                    nonconvex += !shape.convex;
            }
        }
        std::cout << "Private volume shells: " << volumes << ", nonconvex=" << nonconvex << ", rejected=" << failed
                  << "\n";
        CHECK(failed == 0 && volumes >= 200 && nonconvex >= 7);
        CHECK(game.request_world("02_CityHub", error));
        unsigned created = 0;
        for (auto id : game.objects.snapshot())
            if (game.objects.get(id)->container)
                ++created;
        if (created != 5)
        {
            for (std::size_t i = 0; i < game.entries.size(); ++i)
                if (game.world.objects[i].class_name == "Water" || game.world.objects[i].class_name == "Ladder")
                    std::cerr << game.world.objects[i].class_name << ": " << entry_state_name(game.entries[i].state)
                              << " / " << game.entries[i].detail << "\n";
            for (auto& d : game.objects.diagnostics())
                std::cerr << d << "\n";
        }
        CHECK(created == 5);
        CHECK(game.enter_player(error));
        CHECK(place(game, {0, 80, 0}));
        CHECK(game.server_shell.Player(game.local_client)->containers.head.code == CC_WATER);
        CHECK(game.step(1.0 / 60));
        CHECK(game.client_shell.move_mgr.m_bBodyInLiquid);
        const ObjectRecord* ladder = nullptr;
        for (auto id : game.objects.snapshot())
        {
            const auto* record = game.objects.get(id);
            if (record->container && std::string_view(record->state.m_Name) == "Ladder1")
                ladder = record;
        }
        CHECK(ladder);
        DVector dimensions;
        CHECK(game.server.GetObjectDims(game.client_shell.player, &dimensions) == DE_OK);
        bool found = false;
        DVector climb_start;
        for (float x = 232; x <= 276 && !found; x += 4)
        {
            DVector candidate{x, 700, -1536};
            CollisionHit hit;
            if (ladder->container->overlaps(candidate, dimensions) &&
                game.collision.sweep(candidate, candidate, dimensions, hit) == CollisionQuery::Miss)
            {
                found = true;
                climb_start = candidate;
            }
        }
        CHECK(found);
        CHECK(place(game, climb_start));
        game.client_shell.movement_input.jump = true;
        CHECK(steps(game, 20));
        CHECK(game.client_shell.move_mgr.m_bBodyOnLadder && !game.client_shell.move_mgr.m_bUseGravity);
        CHECK(game.client_shell.position.y > climb_start.y + 20);
        std::cout << "CityHub: 5 original volumes; canal swimming and ladder ascent "
                  << game.client_shell.position.y - climb_start.y << " units\n";
        CHECK(game.leave_player());
        CHECK(game.unload_world());
        CHECK(game.server.message_count() == 0 && game.server.string_count() == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"geometry", geometry},     {"concave", concave},     {"membership", membership}, {"lifetime", lifetime},
        {"physics", physics},       {"swimming", swimming},   {"surface", surface},       {"ladders", ladders},
        {"air_damage", air_damage}, {"private", private_data}};
    auto it = cases.find(argv[1]);
    if (it == cases.end())
        return 2;
    return it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
