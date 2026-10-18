#include "brush_fixture.h"
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
    using test_brush::box;
    using test_brush::face;
    using test_brush::prism;
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
    // Engine built-in Container (03_Museum, 05_CDM): code only, zero gravity/current, in list order.
    bool builtin(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto pool = volume(game, "Container", "officepool", {-200, 0, -200}, {200, 100, 200});
        CHECK(pool && game.objects.get(pool)->state.m_ObjectType == OT_CONTAINER);
        CHECK(volume(game, "Water", "water", {-200, 0, -200}, {200, 100, 200}));
        CHECK(place(game, {0, 50, 0}));
        auto* player = game.server_shell.Player(game.local_client);
        CHECK(player->UpdateContainerState());
        CHECK(player->containers.count == 2 && player->containers.body[0].object == pool);
        CHECK(player->containers.body[0].code == CC_NOTHING && player->containers.body[0].gravity == 0 &&
              !player->containers.body[0].hidden && player->containers.body[1].code == CC_WATER);
        // The first point container answers: the authored code-0 volume shadows the water behind it.
        ContainerCode code = CC_WATER;
        CHECK(game.server.GetPointContainerCode({0, 50, 0}, code) && code == CC_NOTHING);
        auto coded = volume(game, "Container", "coded", {300, 0, 300}, {400, 100, 400},
                            {{"ContainerCode", std::int32_t(CC_WATER)}});
        CHECK(coded && game.server.GetPointContainerCode({350, 50, 350}, code) && code == CC_WATER);
        CHECK(!volume(game, "Container", "bad", {500, 0, 500}, {600, 100, 600}, {{"ContainerCode", std::int32_t(-1)}}));
        CHECK(game.objects.diagnostics().back().find("D_WORD") != std::string::npos);
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
        auto shape = game.objects.get(h)->container();
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
    struct IndexHooks
    {
        bool reject = false;
        std::function<void(HOBJECT)> initial, destroy, physics;
    };
    IndexHooks* index_hooks = nullptr;
    struct IndexHookScope
    {
        explicit IndexHookScope(IndexHooks& hooks)
        {
            index_hooks = &hooks;
        }
        ~IndexHookScope()
        {
            index_hooks = nullptr;
        }
    };
    class IndexVolume : public VolumeBrush
    {
      public:
        ~IndexVolume() override
        {
            if (index_hooks && index_hooks->destroy)
                index_hooks->destroy(m_hObject);
        }
        void UpdatePhysics(ContainerPhysics& physics) override
        {
            VolumeBrush::UpdatePhysics(physics);
            if (index_hooks && index_hooks->physics)
                index_hooks->physics(m_hObject);
        }

      protected:
        DDWORD EngineMessageFn(DDWORD id, void* data, DFLOAT reason) override
        {
            const auto result = VolumeBrush::EngineMessageFn(id, data, reason);
            if (id == MID_PRECREATE && index_hooks && index_hooks->reject)
                return 0;
            if (id == MID_INITIALUPDATE && index_hooks && index_hooks->initial)
                index_hooks->initial(m_hObject);
            return result;
        }
    };
    bool index_matches(const Objects& objects)
    {
        std::vector<ObjectId> slow;
        for (auto id : objects.snapshot())
            if (const auto* r = objects.get(id); r && r->container())
                slow.push_back(id);
        return objects.container_snapshot() == slow && objects.bound_container_count() == slow.size();
    }
    bool register_index_volume(GameSession& game)
    {
        static ClassInfo type{
            "IndexVolume", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<IndexVolume>(); }, {}};
        type.parent = game.objects.find_class("VolumeBrush");
        return game.objects.register_class(type);
    }
    bool index_lifetime(const std::filesystem::path& root)
    {
        IndexHooks hooks;
        GameSession game;
        IndexHookScope scope(hooks); // Disable captured callbacks before implicit teardown on CHECK failure.
        CHECK(test_player::setup(game, root, false) && register_index_volume(game));
        auto ordinary = game.objects.create(game.objects.find_class("BaseClass"));
        auto a = volume(game, "Water", "a", {-10, -10, -10}, {10, 10, 10});
        auto b = volume(game, "Water", "b", {-10, -10, -10}, {10, 10, 10});
        CHECK(ordinary && a && b && index_matches(game.objects));
        const auto old = game.objects.get(a)->id;
        const auto held = game.objects.container_snapshot();
        CHECK(game.objects.remove(a) && index_matches(game.objects));
        CHECK(game.objects.container_snapshot() == held); // Pending records remain indexed, queries exclude them.
        CHECK(game.objects.finish_frame() && index_matches(game.objects));
        auto reused = volume(game, "Water", "reused", {-10, -10, -10}, {10, 10, 10});
        CHECK(reused && game.objects.get(reused)->id.slot == old.slot && !game.objects.get(old));
        CHECK(game.objects.container_snapshot().front() == game.objects.get(reused)->id && index_matches(game.objects));
        CHECK(held.front() == old); // Already returned snapshots remain owned and unchanged.
        hooks.reject = true;
        CHECK(!volume(game, "IndexVolume", "rejected", {-10, -10, -10}, {10, 10, 10}));
        hooks.reject = false;
        CHECK(!game.objects.create(game.objects.find_class("Water"), {}, Creation::World,
                                   {{"Name", std::string("missing")}}));
        game.world.models.push_back(box("invalid", {-10, -10, -10}, {10, 10, 10}));
        game.world.models.back().polygons.pop_back();
        CHECK(!game.objects.create(game.objects.find_class("Water"), {}, Creation::World,
                                   {{"Name", std::string("invalid")}}));
        CHECK(index_matches(game.objects) && game.objects.bound_container_count() == 2);
        CHECK(game.objects.finish_frame());
        HOBJECT nested = nullptr, from_destructor = nullptr;
        bool initial_ok = false, destructor_ok = false;
        hooks.initial = [&](HOBJECT h)
        {
            initial_ok = game.objects.get(h)->container() && index_matches(game.objects);
            nested = volume(game, "Water", "nested", {-10, -10, -10}, {10, 10, 10});
            initial_ok = initial_ok && nested && game.objects.remove(h) && index_matches(game.objects);
        };
        auto pending = volume(game, "IndexVolume", "pending", {-10, -10, -10}, {10, 10, 10});
        CHECK(pending && initial_ok && game.objects.bound_container_count() == 4);
        hooks.destroy = [&](HOBJECT h)
        {
            destructor_ok = game.objects.get(h)->life == Life::Destroying && index_matches(game.objects);
            from_destructor = volume(game, "Water", "destructor_child", {-10, -10, -10}, {10, 10, 10});
            destructor_ok =
                destructor_ok && from_destructor && game.objects.remove(nested) && index_matches(game.objects);
        };
        CHECK(game.objects.finish_frame() && destructor_ok && index_matches(game.objects));
        CHECK(!game.objects.get(pending) && !game.objects.get(nested) && game.objects.get(from_destructor));
        CHECK(game.objects.bound_container_count() == 3);
        hooks = {};
        CHECK(game.unload_world() && index_matches(game.objects) && !game.objects.bound_container_count());
        for (unsigned i = 0; i < 3; ++i)
        {
            game.objects.world = &game.world;
            CHECK(volume(game, "Water", "reload" + std::to_string(i), {-10, -10, -10}, {10, 10, 10}));
            CHECK(index_matches(game.objects) && game.objects.bound_container_count() == 1);
            CHECK(game.unload_world() && index_matches(game.objects) && !game.objects.bound_container_count());
        }
        return true;
    }
    bool index_callbacks(const std::filesystem::path& root)
    {
        IndexHooks hooks;
        GameSession game;
        IndexHookScope scope(hooks);
        CHECK(test_player::setup(game, root, false) && register_index_volume(game));
        auto first = volume(game, "IndexVolume", "first", {-10, -10, -10}, {10, 10, 10});
        auto removed = volume(game, "Water", "removed", {-10, -10, -10}, {10, 10, 10});
        auto body = game.objects.create(game.objects.find_class("BaseClass"));
        CHECK(first && removed && body);
        HOBJECT spawned = nullptr;
        bool callback_ok = false;
        hooks.physics = [&](HOBJECT)
        {
            if (spawned)
                return;
            spawned = volume(game, "Water", "spawned", {-10, -10, -10}, {10, 10, 10});
            callback_ok = spawned && game.objects.remove(removed) && index_matches(game.objects);
        };
        CHECK(game.server.damage_containers() && callback_ok);
        auto* skipped = dynamic_cast<VolumeBrush*>(game.objects.get(removed)->game.get());
        auto* fresh = dynamic_cast<VolumeBrush*>(game.objects.get(spawned)->game.get());
        CHECK(skipped && fresh && !skipped->physics_calls && !fresh->physics_calls);
        CHECK(game.server.damage_containers() && fresh->physics_calls == 1);
        CHECK(game.objects.finish_frame() && !game.objects.get(removed) && index_matches(game.objects));
        hooks = {};
        CHECK(game.unload_world() && !game.objects.bound_container_count());
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
        CHECK(game.objects.advance(2.11));
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
    bool specialized_defaults(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto belt = volume(game, "Conveyor", "belt", {-100, 0, -100}, {100, 200, 100});
        auto damage = volume(game, "Damage", "damage", {200, 0, -100}, {400, 200, 100});
        auto fall = volume(game, "FreeFall", "fall", {500, 0, -100}, {700, 200, 100});
        CHECK(belt && damage && fall);
        auto* c = dynamic_cast<Conveyor*>(game.objects.get(belt)->game.get());
        auto* d = dynamic_cast<::Damage*>(game.objects.get(damage)->game.get());
        auto* f = dynamic_cast<FreeFall*>(game.objects.get(fall)->game.get());
        CHECK(c && d && f);
        CHECK(c->info.code == CC_CONVEYOR && c->damage == 0 && !c->show_surface && c->surface_height == 0);
        CHECK(d->info.code == CC_DAMAGE && d->damage == 100 && d->damage_type == DamageType::Normal);
        CHECK(!d->show_surface && d->info.viscosity == 0);
        CHECK(f->info.code == CC_FREEFALL && f->info.gravity == 0 && f->damage == 9999999);
        CHECK(f->damage_type == DamageType::FreeFall && !f->show_surface && f->surface_height == 5);
        CHECK(game.objects.get(fall)->state.m_Flags & FLAG_RAYHIT);
        CHECK(!(game.objects.get(belt)->state.m_Flags & FLAG_RAYHIT));
        CHECK(!volume(game, "Damage", "invalid", {800, 0, -100}, {900, 200, 100}, {{"Damage", 10000001.0f}}));
        CHECK(game.objects.finish_frame());
        CHECK(!game.objects.find_class("CraneControl") && !game.objects.find_class("Minefield"));
        CHECK(!volume(game, "VolumeBrush", "unsupported", {800, 0, -100}, {900, 200, 100},
                      {{"Code", std::int32_t(CC_CRANECONTROL)}}));
        CHECK(game.objects.finish_frame());
        CHECK(game.unload_world());
        CHECK(!game.objects.get(belt) && !game.objects.get(damage) && !game.objects.get(fall));
        CHECK(game.server.message_count() == 0 && game.server.string_count() == 0);
        return true;
    }
    bool conveyor_motion(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto belt =
            volume(game, "Conveyor", "belt", {-500, 0, -500}, {500, 900, 500}, {{"Current", DVector{2400, 0, 0}}});
        CHECK(belt && place(game, {0, 41.001f, 0}));
        auto& client = game.client_shell;
        CHECK(steps(game, 30));
        CHECK(client.move_mgr.m_bBodyOnConveyor && !client.move_mgr.m_bBodyOnLadder);
        CHECK(client.move_mgr.m_bUseGravity && client.move_mgr.m_bOnGround);
        CHECK(client.position.x > 70 && client.position.x < 100);
        CHECK(std::abs(client.move_mgr.body.velocity.x - 200) < .01f); // Source ladder-speed cap.
        client.movement_input.jump = true;
        CHECK(game.step(1.0 / 120));
        CHECK(client.move_mgr.jumps == 1 && client.move_mgr.body.velocity.y > 500);
        CHECK(!client.move_mgr.m_bOnGround && client.move_mgr.m_bBodyOnConveyor);
        client.movement_input = {};
        CHECK(game.step(1.0 / 120));
        CHECK(std::hypot(client.move_mgr.body.velocity.x, client.move_mgr.body.velocity.y) < 201);
        // Hidden first conveyor consumes the category, but later active currents
        // still add. No conveyor braking/cap is applied by the second record.
        CHECK(place(game, {0, 400, 0}));
        auto* brush = dynamic_cast<Conveyor*>(game.objects.get(belt)->game.get());
        brush->info.hidden = true;
        CHECK(volume(game, "Conveyor", "second", {-500, 0, -500}, {500, 900, 500}, {{"Current", DVector{120, 0, 0}}}));
        CHECK(game.step(1.0 / 120));
        CHECK(!client.move_mgr.m_bBodyOnConveyor && client.move_mgr.m_bUseGravity);
        CHECK(client.move_mgr.body.velocity.x > .8f && client.move_mgr.body.velocity.x < 1.01f);
        CHECK(game.objects.remove(belt));
        CHECK(game.step(1.0 / 120));
        CHECK(client.move_mgr.m_bBodyOnConveyor); // Pending removal cannot mask the next volume.
        CHECK(place(game, {800, 400, 0}));
        CHECK(!client.move_mgr.m_bBodyOnConveyor); // Force-position resets immediately.
        CHECK(game.step(.1));
        CHECK(!client.move_mgr.m_bBodyOnConveyor && client.move_mgr.body.velocity.y < -150);
        return true;
    }
    bool conveyor_bodies(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        auto belt = volume(game, "Conveyor", "belt", {-500, 0, -500}, {500, 500, 500},
                           {{"Current", DVector{120, 0, 0}}, {"Damage", 100.0f}});
        CHECK(belt);
        ObjectCreateStruct create;
        create.m_Pos = {0, 1.001f, 0};
        auto character = game.objects.create(game.objects.find_class("CBaseCharacter"), create);
        create.m_Pos.x = 100;
        auto ordinary = game.objects.create(game.objects.find_class("BaseClass"), create);
        CHECK(character && ordinary);
        DVector dims{1, 1, 1};
        game.server.SetObjectDims(character, &dims);
        game.server.SetObjectDims(ordinary, &dims);
        CHECK(game.server.advance_physics(1.0 / 120));
        CHECK(std::abs(game.objects.get(character)->velocity.x - 1) < .001f);
        CHECK(game.objects.get(ordinary)->velocity.x == 0);
        auto* brush = dynamic_cast<Conveyor*>(game.objects.get(belt)->game.get());
        CHECK(brush->physics_calls == 1);
        brush->info.hidden = true;
        CHECK(game.server.advance_physics(1.0 / 120));
        CHECK(std::abs(game.objects.get(character)->velocity.x - 2) < .001f);
        CHECK(brush->physics_calls == 2); // Source server override does not check Hidden.
        DVector air{0, 100, 0}, zero{};
        CHECK(game.server.TeleportObject(character, &air) == DE_OK);
        CHECK(game.server.SetVelocity(character, &zero) == DE_OK);
        CHECK(game.server.advance_physics(1.0 / 120));
        CHECK(game.objects.get(character)->velocity.x == 0 && brush->physics_calls == 2);
        CHECK(game.server.damage_containers());
        CHECK(brush->damage_events == 0); // Conveyor override never invokes base damage.
        return true;
    }
    bool specialized_damage(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        CHECK(volume(game, "Damage", "damage", {-100, 0, -100}, {100, 200, 100}, {{"DamageType", std::int32_t(9)}}));
        CHECK(place(game, {0, 80, 0}));
        auto* player = game.server_shell.Player(game.local_client);
        CHECK(game.objects.advance(.11));
        CHECK(game.server.damage_containers());
        CHECK(player->m_damage.m_fHitPoints == 100 && player->protected_volume_hits == 1);
        CHECK(game.objects.advance(1.9));
        CHECK(game.server.damage_containers());
        CHECK(player->m_damage.last_damage.amount == 10 && player->m_damage.last_damage.type == DamageType::Blind);
        const auto health = player->m_damage.m_fHitPoints;
        CHECK(health < 100);
        CHECK(game.objects.advance(.04));
        CHECK(game.server.damage_containers());
        CHECK(player->m_damage.m_fHitPoints == health);
        auto fall =
            volume(game, "FreeFall", "fall", {200, 0, -100}, {400, 500, 100}, {{"DamageType", std::int32_t(10)}});
        CHECK(fall && place(game, {300, 200, 0}));
        player->m_damage.m_bGodMode = DTRUE;
        CHECK(game.objects.advance(.11));
        CHECK(game.server.damage_containers());
        CHECK(player->m_damage.m_fHitPoints == health);
        player->m_damage.m_bGodMode = DFALSE;
        player->m_damage.SetArmorPoints(100);
        CHECK(game.objects.advance(.11));
        CHECK(game.server.damage_containers());
        CHECK(player->m_damage.IsDead() && player->m_damage.m_fArmorPoints == 0);
        CHECK(player->m_damage.last_damage.type == DamageType::Slow);
        CHECK(std::abs(player->m_damage.last_damage.amount - 999999.9f) < .1f);
        CHECK(game.server.message_count() == 0);
        // Death suppresses commands; it does not corrupt the session or leak views on exit.
        game.client_shell.movement_input.forward = true;
        CHECK(game.step(.01));
        CHECK(game.client_shell.move_mgr.m_dwControlFlags == 0);
        CHECK(game.leave_player() && game.unload_world());
        CHECK(game.server.message_count() == 0 && game.server.string_count() == 0);
        return true;
    }
    bool freefall_motion(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto fall = volume(game, "FreeFall", "fall", {-500, 0, -500}, {500, 1000, 500}, {{"Damage", 0.0f}});
        CHECK(fall && place(game, {0, 400, 0}));
        auto& client = game.client_shell;
        auto* player = game.server_shell.Player(game.local_client);
        CHECK(steps(game, 60));
        CHECK(client.move_mgr.m_bBodyInLiquid && !client.move_mgr.m_bUseGravity);
        CHECK(std::abs(client.position.y - 400) < .01f && player->m_fAirLevel == 100);
        client.movement_input.jump = true;
        CHECK(steps(game, 30));
        CHECK(client.position.y > 420 && player->m_fAirLevel == 100);
        client.movement_input = {};
        CHECK(place(game, {800, 400, 0}));
        CHECK(game.step(.1));
        CHECK(client.move_mgr.m_bUseGravity && client.move_mgr.body.velocity.y < -150);
        // Non-player gravity also uses FreeFall's zero, with no permanent flag mutation.
        ObjectCreateStruct create;
        create.m_Pos = {0, 300, 0};
        create.m_Flags = FLAG_GRAVITY;
        auto body = game.objects.create(game.objects.find_class("BaseClass"), create);
        CHECK(body && game.server.advance_physics(.1));
        CHECK(game.objects.get(body)->velocity.y == 0);
        CHECK(game.objects.get(body)->state.m_Flags & FLAG_GRAVITY);
        CHECK(game.objects.remove(fall));
        CHECK(game.server.advance_physics(.1));
        CHECK(game.objects.get(body)->velocity.y < -190);
        return true;
    }
    bool specialized_private(const std::filesystem::path& root)
    {
        GameSession game;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        std::map<std::string, unsigned> counts;
        unsigned skipped = 0, ambiguous = 0, missing = 0;
        for (const auto& [path, resource] : game.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            CHECK(game.resources.read(path, bytes, error));
            if (!read_world(bytes, game.world, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                ++skipped;
                continue;
            }
            for (const auto& source : game.world.objects)
            {
                if (source.class_name != "Conveyor" && source.class_name != "Damage" && source.class_name != "FreeFall")
                    continue;
                Properties properties;
                for (const auto& p : source.properties)
                    properties.emplace(p.name, p.value);
                auto object =
                    game.objects.create(game.objects.find_class(source.class_name), {}, Creation::World, properties);
                auto* r = game.objects.get(object);
                if (!r && path == "worlds/14_subwayc.dat" && game.objects.error() == ObjectError::Unsupported &&
                    !game.objects.diagnostics().empty() &&
                    game.objects.diagnostics().back() == "noname: Ambiguous named container model")
                {
                    std::cout << path << " " << source.class_name << ": noname model is ambiguous\n";
                    ++ambiguous;
                    CHECK(game.objects.finish_frame());
                    continue;
                }
                if (!r && path == "worlds/20_powerstation.dat" && source.class_name == "Damage" &&
                    game.objects.error() == ObjectError::MissingWorldModel && !game.objects.diagnostics().empty() &&
                    game.objects.diagnostics().back() == "kkk: Missing named container model")
                {
                    std::cout << path << " Damage: missing kkk model\n";
                    ++missing;
                    CHECK(game.objects.finish_frame());
                    continue;
                }
                if (!r || r->life != Life::Alive)
                {
                    std::cerr << path << " " << source.class_name << " failed\n";
                    for (const auto& p : source.properties)
                        if (const auto* number = std::get_if<float>(&p.value))
                            std::cerr << p.name << "=" << *number << "\n";
                        else if (const auto* number = std::get_if<std::int32_t>(&p.value))
                            std::cerr << p.name << "=" << *number << "\n";
                    for (const auto& diagnostic : game.objects.diagnostics())
                        std::cerr << diagnostic << "\n";
                }
                CHECK(r && r->life == Life::Alive && r->container());
                const auto* brush = dynamic_cast<VolumeBrush*>(r->game.get());
                CHECK(brush);
                const auto& s = *r->container();
                std::cout << path << " " << source.class_name << " " << r->state.m_Name << " bounds " << s.min.x << ","
                          << s.min.y << "," << s.min.z << " / " << s.max.x << "," << s.max.y << "," << s.max.z
                          << " current " << brush->info.current.x << "," << brush->info.current.y << ","
                          << brush->info.current.z << " damage " << brush->damage << " type "
                          << unsigned(brush->damage_type) << "\n";
                ++counts[source.class_name];
                CHECK(game.objects.remove(object) && game.objects.finish_frame());
            }
        }
        std::cout << "Skipped DATs " << skipped << "; ambiguous models " << ambiguous << "; missing models " << missing
                  << "\n";
        // Duplicate names bind the first model in file order: four former ambiguities are created.
        CHECK(skipped == 1 && ambiguous == 0 && missing == 1);
        CHECK(counts["Conveyor"] == 40 && counts["Damage"] == 49 && counts["FreeFall"] == 18);
        for (const auto& [name, count] : counts)
            std::cout << name << ": " << count << " original instances created and removed\n";
        // Enter a real level after time has already elapsed, then reload it.
        // The original volume-damage grace belongs to the level, not the player.
        CHECK(game.objects.advance(3));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(game.request_world("01_SubwayA", error));
            CHECK(game.server.GetWorldTime() == 0);
            if (cycle == 0)
                CHECK(game.objects.advance(2.1));
            CHECK(game.enter_player(error));
            auto* player = game.server_shell.Player(game.local_client);
            HOBJECT belt = nullptr, hazard = nullptr;
            for (auto id : game.objects.snapshot())
            {
                auto* record = game.objects.get(id);
                if (std::string_view(record->state.m_Name) == "conveyor1")
                    belt = record->handle;
                if (std::string_view(record->state.m_Name) == "pain1")
                    hazard = record->handle;
            }
            CHECK(belt && hazard);
            ContainerPhysics dose;
            dose.object = player->m_hObject;
            dose.player = dose.damage_only = true;
            CHECK(game.objects.engine_message(hazard, MID_AFFECTPHYSICS, &dose));
            if (cycle == 0)
                CHECK(player->m_damage.m_fHitPoints < 100 && player->protected_volume_hits == 0);
            else
                CHECK(player->m_damage.m_fHitPoints == 100 && player->protected_volume_hits == 1);
            DVector dims;
            CHECK(game.server.GetObjectDims(player->m_hObject, &dims) == DE_OK);
            bool placed = false;
            DVector start;
            for (float y = dims.y; y < 200 && !placed; y += 1)
            {
                DVector candidate{-32, y + .001f, 0};
                CollisionHit hit;
                if (!game.objects.get(belt)->container()->overlaps(candidate, dims) ||
                    game.collision.sweep(candidate, candidate, dims, hit) != CollisionQuery::Miss)
                    continue;
                CHECK(place(game, candidate));
                MovementState state;
                CHECK(game.server.GetMovementState(player->m_hObject, state));
                start = candidate;
                placed = true;
            }
            CHECK(placed);
            CHECK(steps(game, 4));
            CHECK(game.client_shell.move_mgr.m_bBodyOnConveyor && game.client_shell.move_mgr.m_bUseGravity);
            CHECK(game.client_shell.position.z < start.z - .1f);
            std::cout << "Subway conveyor: start " << start.x << "," << start.y << "," << start.z << "; travel "
                      << game.client_shell.position.z - start.z << "; level clock " << game.server.GetWorldTime()
                      << "; protected hits " << player->protected_volume_hits << "\n";
            CHECK(game.unload_world());
        }
        CHECK(game.objects.snapshot().empty());
        CHECK(game.server.message_count() == 0 && game.server.string_count() == 0);
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
            if (game.objects.get(id)->container())
                ++created;
        if (created != 11)
        {
            for (std::size_t i = 0; i < game.entries.size(); ++i)
                if (game.world.objects[i].class_name == "Water" || game.world.objects[i].class_name == "Ladder")
                    std::cerr << game.world.objects[i].class_name << ": " << entry_state_name(game.entries[i].state)
                              << " / " << game.entries[i].detail << "\n";
            for (auto& d : game.objects.diagnostics())
                std::cerr << d << "\n";
        }
        CHECK(created == 11 && index_matches(game.objects));
        CHECK(game.enter_player(error));
        CHECK(place(game, {0, 80, 0}));
        CHECK(game.server_shell.Player(game.local_client)->containers.head.code == CC_WATER);
        CHECK(game.step(1.0 / 60));
        CHECK(game.client_shell.move_mgr.m_bBodyInLiquid);
        const ObjectRecord* ladder = nullptr;
        for (auto id : game.objects.snapshot())
        {
            const auto* record = game.objects.get(id);
            if (record->container() && std::string_view(record->state.m_Name) == "Ladder1")
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
            if (ladder->container()->overlaps(candidate, dimensions) &&
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
        std::cout << "CityHub: 11 original volumes; canal swimming and ladder ascent "
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
        {"geometry", geometry},
        {"concave", concave},
        {"membership", membership},
        {"builtin", builtin},
        {"lifetime", lifetime},
        {"index_lifetime", index_lifetime},
        {"index_callbacks", index_callbacks},
        {"physics", physics},
        {"swimming", swimming},
        {"surface", surface},
        {"ladders", ladders},
        {"air_damage", air_damage},
        {"specialized_defaults", specialized_defaults},
        {"conveyor_motion", conveyor_motion},
        {"conveyor_bodies", conveyor_bodies},
        {"specialized_damage", specialized_damage},
        {"freefall_motion", freefall_motion},
        {"specialized_private", specialized_private},
        {"private", private_data}};
    auto it = cases.find(argv[1]);
    if (it == cases.end())
        return 2;
    return it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
