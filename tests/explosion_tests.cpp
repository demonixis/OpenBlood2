#include "brush_fixture.h"
#include "game/original/DestructableBrush.h"
#include "game/original/radius_damage.h"
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
    bool near(float a, float b)
    {
        return std::abs(a - b) < .01f;
    }
    // Test-only receiver with the real aggregate and callback-removal hooks.
    struct Target : BaseClass
    {
        CDestructable damage;
        HOBJECT remove_target = nullptr;
        unsigned hits = 0;
        bool remove_sender = false;
        std::function<void()> callback;
        Target()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD id, void* data, float reason) override
        {
            if (id == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject);
                damage.SetHitPoints(1000);
            }
            return BaseClass::EngineMessageFn(id, data, reason);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD id, HMESSAGEREAD read) override
        {
            auto result = BaseClass::ObjectMessageFn(sender, id, read);
            if (id == damage_message)
            {
                ++hits;
                if (remove_target)
                    g_pServerDE->RemoveObject(remove_target);
                if (remove_sender)
                    g_pServerDE->RemoveObject(sender);
                if (callback)
                    callback();
            }
            return result;
        }
    };
    Target* target(GameSession& g, DVector pos, DVector dims = {})
    {
        static ClassInfo type{"ExplosionTestTarget",
                              nullptr,
                              []() -> std::unique_ptr<BaseClass> { return std::make_unique<Target>(); },
                              {}};
        if (!g.objects.find_class(type.name))
        {
            type.parent = g.objects.find_class("BaseClass");
            if (!g.objects.register_class(type))
                return nullptr;
        }
        ObjectCreateStruct state;
        state.m_Pos = pos;
        auto h = g.objects.create(&type, state);
        if (!h)
            return nullptr;
        g.server.SetObjectDims(h, &dims);
        return dynamic_cast<Target*>(g.server.HandleToObject(h));
    }
    CDestructableBrush* brush(GameSession& g, DVector pos = {0, 100, 0}, Properties props = {},
                              Creation reason = Creation::World)
    {
        const auto name = "blast" + std::to_string(g.world.models.size());
        auto model = test_brush::box(name, vector_subtract(pos, {2, 2, 2}), vector_add(pos, {2, 2, 2}));
        model.translation = pos;
        g.world.models.push_back(std::move(model));
        props["Name"] = name;
        props.try_emplace("HitPoints", 50.0f);
        auto h = g.objects.create(g.objects.find_class("CDestructableBrush"), {}, reason, props);
        return h ? dynamic_cast<CDestructableBrush*>(g.server.HandleToObject(h)) : nullptr;
    }
    bool kill(GameSession& g, HOBJECT h)
    {
        auto str = g.server.CreateString("KILL");
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(h), h, 1000);
        CHECK(message && g.server.WriteToMessageHString(message, str) == DE_OK);
        CHECK(g.server.EndMessage(message) == DE_OK);
        g.server.FreeString(str);
        return true;
    }
    bool properties(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        for (auto reason : {Creation::World, Creation::String})
        {
            auto* b = brush(g, {},
                            {{"Amount", std::int32_t(258)},
                             {"Stone", true},
                             {"Metal", true},
                             {"Energy", true},
                             {"Liquid", true},
                             {"Custom", true},
                             {"Exploding", true},
                             {"ExplodeDamage", 999.0f},
                             {"DamageRadius", 888.0f},
                             {"Scale", -2.0f}},
                            reason);
            CHECK(b && b->debris.amount == 2 && b->debris.scale == -2 && b->debris.custom && b->debris.exploding);
            CHECK(b->GetSurfaceType() == 10 && b->debris.explosion_damage == 100 && b->debris.explosion_radius == 200);
            CHECK((g.objects.get(b->m_hObject)->user_flags >> 24) == 0); // Fallback is a query, not a flag rewrite.
        }
        auto* explicit_surface = brush(g, {}, {{"SurfaceType", std::int32_t(50)}, {"Stone", true}});
        CHECK(explicit_surface && explicit_surface->GetSurfaceType() == 50);
        auto* runtime = brush(g, {}, {}, Creation::Runtime);
        // Runtime creation does not read property overrides, hence Name cannot bind.
        CHECK(!runtime);
        CHECK(!brush(g, {}, {{"Scale", std::numeric_limits<float>::infinity()}}));
        CHECK(!brush(g, {}, {{"Scale", 1e7f}}));
        auto* polygon = brush(g, {}, {{"Pushable", true}, {"BoxPhysics", false}, {"Exploding", true}});
        CHECK(polygon && polygon->debris.exploding);
        const auto* body = g.objects.get(polygon->m_hObject);
        CHECK(body && body->world_body && body->body_properties.polygon_bounds &&
              !(body->state.m_Flags & FLAG_BOXPHYSICS));
        CHECK(!brush(g, {}, {{"SpawnObject", std::string("Enemy")}, {"Exploding", true}}));
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0 && g.server.string_count() == 0);
        return true;
    }
    bool requests(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto* b = brush(g, {},
                        {{"Stone", true},
                         {"Metal", true},
                         {"Wood", true},
                         {"Amount", std::int32_t(-1)},
                         {"CustomModel", std::string("models\\missing.abc")},
                         {"CustomSound", std::string("sounds\\missing.wav")}});
        CHECK(b);
        b->debris.Create({0, 0, 1}, 50);
        CHECK(g.server.debris_effects.size() == 3 && g.server.explosion_effects.empty());
        const auto& a = g.server.debris_effects;
        CHECK(a[0].amount == 255 && a[0].flags == (1 | 0x200 | 0x4000));
        CHECK(a[1].flags == (2 | 0x200 | 0x4000) && a[2].flags == (3 | 0x200 | 0x4000));
        CHECK(near(a[0].velocity.y, 70.7107f) && near(a[0].velocity.z, 70.7107f));
        CHECK(near(a[1].velocity.y, -70.7107f) && near(a[2].velocity.y, 70.7107f));
        CHECK(!b->debris.custom && a[0].model == "models\\missing.abc" && a[0].sound == "sounds\\missing.wav");
        CHECK(a[0].dimensions.x == 2);
        b->debris.Create({0, -1, 0}, 50);
        CHECK(g.server.debris_effects.back().velocity.y == 0 && g.server.debris_effects.back().velocity.z == 0);
        auto* custom_only = brush(g, {}, {{"Custom", true}});
        CHECK(custom_only);
        custom_only->debris.Create({}, 0);
        CHECK(g.server.debris_effects.size() == 6); // Custom alone selects no material.
        for (int i = 0; i < 30; ++i)
            b->debris.Create({}, 0);
        CHECK(g.server.debris_effects.size() == 64);
        auto saved = g.server.debris_effects.back().source;
        CHECK(g.objects.remove(b->m_hObject) && g.objects.finish_frame());
        CHECK(g.server.debris_effects.back().source == saved);
        CHECK(g.unload_world() && g.server.debris_effects.empty());
        return true;
    }
    bool radius(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto* b = brush(g);
        CHECK(b);
        auto* full = target(g, {100, 100, 0});
        auto* falloff = target(g, {150, 100, 0});
        auto* edge = target(g, {200, 100, 0});
        auto* outside = target(g, {201, 100, 0});
        auto* large = target(g, {210, 100, 0}, {90, 90, 90});
        CHECK(full && falloff && edge && outside && large);
        g.server.SetObjectState(full->m_hObject, OBJSTATE_INACTIVE);
        // A solid occluder is intentionally ignored by the source area rule.
        CHECK(brush(g, {50, 100, 0}, {{"CanDamage", false}}));
        auto report = DamageObjectsInRadius(b->m_hObject, b, {0, 100, 0}, 200, 100, DamageType::Explode);
        CHECK(!report.cancelled && report.candidates == 7 && report.delivered == 6);
        CHECK(near(full->damage.m_fHitPoints, 900) && near(falloff->damage.m_fHitPoints, 912.5f));
        CHECK(edge->hits == 1 && edge->damage.m_fHitPoints == 1000 && outside->hits == 0);
        CHECK(near(large->damage.m_fHitPoints, 980));
        CHECK(full->damage.last_damage.area_effect && full->damage.last_damage.type == DamageType::Explode);
        CHECK(full->damage.last_damage.attacker == b->m_hObject && full->damage.last_damage.direction.x == 100);
        return true;
    }
    bool chain(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto* first = brush(g, {0, 100, 0}, {{"Exploding", true}, {"Metal", true}});
        auto* second = brush(g, {100, 100, 0}, {{"Exploding", true}, {"Stone", true}});
        auto* third = brush(g, {250, 100, 0}, {{"Exploding", true}, {"Glass", true}});
        CHECK(first && second && third);
        const auto h1 = first->m_hObject, h2 = second->m_hObject, h3 = third->m_hObject;
        CHECK(kill(g, h1) && g.server.explosion_effects.empty());
        CHECK(g.step(.002) && !g.objects.get(h1) && second->m_damage.IsDead() && !third->m_damage.IsDead());
        CHECK(g.server.explosion_effects.size() == 1 && g.server.debris_effects.size() == 1);
        CHECK(g.step(.002) && !g.objects.get(h2) && third->m_damage.IsDead());
        CHECK(g.step(.002) && !g.objects.get(h3) && g.objects.snapshot().empty());
        CHECK(g.server.explosion_effects.size() == 3 && g.server.debris_effects.size() == 3);
        CHECK(g.server.brush_destructions.size() == 3 && CDestructable::m_dwNumDestructables == 0);
        const auto& effect = g.server.explosion_effects[0];
        CHECK(effect.type == 3 && effect.normal.y == 1 && effect.radius == 200 && effect.damage == 100);
        CHECK(effect.sound == "Sounds\\Weapons\\c4\\explosion_1.wav" && effect.sound_radius == 1000);
        CHECK(g.step(.1) && g.server.explosion_effects.size() == 3);
        CHECK(g.unload_world() && g.server.explosion_effects.empty());
        return true;
    }
    bool gates(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto* b = brush(g);
        auto* god = target(g, {0, 100, 0});
        auto* locked = target(g, {0, 100, 0});
        auto* trigger = target(g, {0, 100, 0});
        auto* armored = target(g, {0, 100, 0});
        CHECK(b && god && locked && trigger && armored);
        god->damage.m_bGodMode = true;
        locked->damage.can_damage = false;
        trigger->damage.trigger_only = true;
        armored->damage.SetArmorPoints(100);
        auto report = DamageObjectsInRadius(b->m_hObject, b, {0, 100, 0}, 200, 100, DamageType::Explode);
        CHECK(report.delivered == 5 && !report.cancelled);
        CHECK(god->damage.m_fHitPoints == 1000 && locked->damage.m_fHitPoints == 1000 &&
              trigger->damage.m_fHitPoints == 1000);
        CHECK(near(armored->damage.m_fHitPoints, 970) && near(armored->damage.m_fArmorPoints, 30));
        auto* retained = brush(g, {1000, 100, 0}, {{"Destructable", false}, {"Exploding", true}});
        CHECK(retained && kill(g, retained->m_hObject) && g.step(.002));
        CHECK(g.server.IsObjectAlive(retained->m_hObject) && g.server.explosion_effects.empty());
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto* b = brush(g, {0, 100, 0}, {{"Exploding", true}, {"Glass", true}});
        auto* victim = target(g, {0, 100, 0});
        auto* remover = target(g, {0, 100, 0});
        CHECK(b && victim && remover);
        // List inserts at its head: newest receiver removes the later target.
        remover->remove_target = victim->m_hObject;
        auto report = DamageObjectsInRadius(b->m_hObject, b, {0, 100, 0}, 200, 10, DamageType::Explode);
        CHECK(!report.cancelled && report.candidates == 3 && report.delivered == 2 && victim->hits == 0);
        CHECK(g.objects.finish_frame());
        Target* spawned = nullptr;
        remover->callback = [&]()
        {
            if (!spawned)
                spawned = target(g, {0, 100, 0});
        };
        report = DamageObjectsInRadius(b->m_hObject, b, {0, 100, 0}, 200, 1, DamageType::Explode);
        CHECK(!report.cancelled && report.candidates == 2 && spawned && spawned->hits == 0);
        CHECK(g.objects.remove(spawned->m_hObject) && g.objects.finish_frame());
        remover->callback = {};
        remover->remove_sender = true;
        const auto source = b->m_hObject;
        CHECK(kill(g, source) && g.step(.002) && !g.objects.get(source));
        CHECK(g.server.explosion_effects.size() == 1 && g.server.explosion_effects[0].targets.cancelled);
        CHECK(g.server.debris_effects.size() == 1); // Copied FX request survives source removal.
        CHECK(g.server.message_count() == 0 && g.server.string_count() == 0);
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool limits(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto* b = brush(g, {}, {{"Exploding", true}, {"CanDamage", false}});
        CHECK(b);
        for (float radius : {0.0f, -1.0f, 1e-30f, 1e7f, std::numeric_limits<float>::infinity()})
            CHECK(DamageObjectsInRadius(b->m_hObject, b, {}, radius, 10, DamageType::Explode).cancelled);
        CHECK(DamageObjectsInRadius(b->m_hObject, b, {}, 200, -1, DamageType::Explode).cancelled);
        const auto suppressed = DamageObjectsInRadius(b->m_hObject, nullptr, {}, 200, 1, DamageType::Explode);
        CHECK(suppressed.null_damager && !suppressed.cancelled && suppressed.candidates == 1 &&
              suppressed.in_radius == 1 && suppressed.delivered == 0);
        for (int i = 0; i < 40; ++i)
            b->debris.Create({}, 0);
        CHECK(g.server.explosion_effects.size() == 32 && g.server.debris_effects.empty());
        CHECK(g.server.explosion_effects.back().targets.delivered == 1 && !b->m_damage.IsDead());
        CHECK(g.objects.remove(b->m_hObject));
        CHECK(DamageObjectsInRadius(b->m_hObject, b, {}, 200, 1, DamageType::Explode).cancelled);
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool player(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(player);
        auto position = g.objects.get(player->m_hObject)->state.m_Pos;
        auto* b = brush(g, vector_add(position, {100, 0, 0}), {{"Exploding", true}});
        CHECK(b);
        player->m_damage.SetHitPoints(250);
        player->m_damage.SetArmorPoints(100);
        CHECK(kill(g, b->m_hObject) && g.step(.002));
        CHECK(near(player->m_damage.m_fHitPoints, 220) && near(player->m_damage.m_fArmorPoints, 30));
        CHECK(player->m_damage.last_damage.type == DamageType::Explode && player->m_damage.last_damage.area_effect);
        CHECK(g.step(.01) && near(g.client_shell.stats.health, 220) && near(g.client_shell.stats.armor, 30));
        CHECK(g.leave_player() && g.unload_world() && g.objects.snapshot().empty());
        CHECK(g.server.message_count() == 0 && g.server.string_count() == 0 &&
              CDestructable::m_dwNumDestructables == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"properties", properties}, {"requests", requests}, {"radius", radius}, {"chain", chain},
        {"gates", gates},           {"lifetime", lifetime}, {"limits", limits}, {"player", player}};
    auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
