#include "camera_fixture.h"
#include "game/server_adapter.h"
#include "player_fixture.h"
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
    // Test-only owner of the original damage aggregate; never registered by the game.
    struct Target : BaseClass
    {
        CDestructable damage;
        bool remove_owner = false;
        Target()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD message, void* data, float value) override
        {
            if (message == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject, nullptr);
                damage.SetHitPoints(100);
                damage.m_fMaxHitPoints = 100;
            }
            return BaseClass::EngineMessageFn(message, data, value);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read) override
        {
            auto result = BaseClass::ObjectMessageFn(sender, message, read);
            if (remove_owner && message == damage_message)
            {
                g_pServerDE->RemoveObject(sender);
                g_pServerDE->RemoveObject(m_hObject);
            }
            return result;
        }
    };
    std::unique_ptr<BaseClass> make_target()
    {
        return std::make_unique<Target>();
    }
    Target* target(GameSession& game, DVector position, DVector dims = {10, 10, 10})
    {
        static ClassInfo type{"WeaponTestTarget", nullptr, make_target, {}};
        if (!game.objects.find_class(type.name))
        {
            type.parent = game.objects.find_class("BaseClass");
            if (!game.objects.register_class(type))
                return nullptr;
        }
        ObjectCreateStruct state;
        state.m_Pos = position;
        state.m_Flags = FLAG_SOLID;
        auto handle = game.objects.create(&type, state);
        if (!handle)
            return nullptr;
        game.server.SetObjectDims(handle, &dims);
        return dynamic_cast<Target*>(game.server.HandleToObject(handle));
    }
    bool entry(GameSession& game, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(game, root));
        std::string error;
        CHECK(game.enter_player(error));
        return true;
    }
    WeaponFire shot(GameSession& game, DBYTE type = WEAP_BERETTA, bool alt = false)
    {
        DVector position;
        game.server.GetObjectPos(game.client_shell.player, &position);
        position.y += 36;
        return {position, {0, 0, 1}, 42, type, alt, false};
    }
    bool inventory(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(entry(game, root));
        auto& inv = game.server_shell.Player(game.local_client)->m_InventoryMgr;
        CHECK(inv.GetWeapon(0)->data.m_nType == WEAP_MELEE && inv.GetWeapon(1)->data.m_nType == WEAP_BERETTA &&
              inv.m_nCurWeaponSlot == 1);
        CHECK(inv.MaxAmmo(1) == 500 && inv.AddAmmo(1, 1000) == InventoryResult::Ok && inv.GetAmmoCount(1) == 500);
        CHECK(inv.AddAmmo(1, 1) == InventoryResult::Full);
        inv.SetStrength(10);
        CHECK(inv.m_nAttribStrength == 10 && inv.MaxAmmo(1) == 600);
        CHECK(inv.AddAmmo(1, 1000) == InventoryResult::Ok && inv.GetAmmoCount(1) == 600);
        inv.SetStrength(255);
        inv.SetStrength(7); // Decrease above the table range must not index beyond column five.
        CHECK(inv.m_nAttribStrength == 7 && inv.MaxAmmo(1) == 600 && inv.GetAmmoCount(1) == 600);
        inv.SetStrength(1);
        CHECK(inv.GetAmmoCount(1) == 100);
        CHECK(inv.AddAmmo(1, -1) == InventoryResult::Invalid && inv.AddAmmo(0, 1) == InventoryResult::Invalid);
        CHECK(inv.ObtainWeapon(WEAP_BERETTA) == InventoryResult::Ok && inv.GetWeapon(1, true));
        CHECK(inv.ObtainWeapon(WEAP_BERETTA) == InventoryResult::AlreadyHave);
        CHECK(inv.ObtainWeapon(WEAP_MELEE) == InventoryResult::AlreadyHave);
        CHECK(inv.ObtainWeapon(15) == InventoryResult::NotAvailable && // Orb: not ported.
              inv.ObtainWeapon(1, 99) == InventoryResult::Invalid);
        CHECK(inv.ChangeWeaponSlot(99) == InventoryResult::NotAvailable && inv.m_nCurWeaponSlot == 1);
        std::string error;
        CHECK(game.select_weapon(0, error) && inv.m_nCurWeaponSlot == 0 && game.client_shell.weapons[0].type == 17);
        CHECK(game.select_weapon(1, error) && game.client_shell.weapons[1].type == 1);
        auto fire = shot(game);
        fire.left = true;
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Fired && inv.GetAmmoCount(1) == 99);
        const auto view_model = game.client_shell.weapons[0].model;
        game.resources = Resources{};
        game.models.clear();
        CHECK(!game.select_weapon(0, error) && !error.empty() && inv.m_nCurWeaponSlot == 1 &&
              game.client_shell.weapons[0].model == view_model);
        CHECK(game.leave_player() && !game.client_shell.weapons[0].model && !game.client_shell.weapons[1].model);
        CHECK(game.server.message_count() == 0);
        return true;
    }
    bool segments(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(entry(game, root));
        auto fire = shot(game);
        auto* near = target(game, vector_add(fire.position, {0, 0, 50}));
        CHECK(near);
        auto* far = target(game, vector_add(fire.position, {0, 0, 100}));
        CHECK(far);
        SegmentHit hit;
        CHECK(game.server.IntersectSegment(fire.position, vector_add(fire.position, {0, 0, 200}),
                                           game.client_shell.player, hit) == SegmentResult::Hit &&
              hit.object == near->m_hObject);
        game.server.SetObjectFlags(near->m_hObject, 0);
        CHECK(game.server.IntersectSegment(fire.position, vector_add(fire.position, {0, 0, 200}),
                                           game.client_shell.player, hit) == SegmentResult::Hit &&
              hit.object == far->m_hObject);
        CHECK(game.server.IntersectSegment({32, 100, 64}, {32, -100, 64}, game.client_shell.player, hit) ==
                  SegmentResult::Hit &&
              !hit.object && hit.polygon == 0);
        CHECK(game.server.IntersectSegment(fire.position, fire.position, nullptr, hit) == SegmentResult::Invalid);
        auto old = far->m_hObject;
        CHECK(game.objects.remove(old));
        CHECK(game.objects.finish_frame());
        CHECK(game.server.IntersectSegment({0, 10, 0}, {1, 10, 0}, old, hit) == SegmentResult::Invalid);
        game.server.collision_ready = false;
        CHECK(game.server.IntersectSegment({0, 10, 0}, {1, 10, 0}, nullptr, hit) == SegmentResult::Unavailable);
        return true;
    }
    bool firing(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(entry(game, root));
        auto fire = shot(game);
        auto* victim = target(game, vector_add(fire.position, {0, 0, 100}));
        CHECK(victim);
        auto& inv = game.server_shell.Player(game.local_client)->m_InventoryMgr;
        auto* weapon = inv.CurrentWeapon();
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Fired);
        const auto first = weapon->last_shot;
        CHECK(inv.GetAmmoCount(1) == 49 && first.damage >= 8 && first.damage <= 12 &&
              first.hit.object == victim->m_hObject && first.damage_delivered);
        CHECK(std::abs(victim->damage.m_fHitPoints - (100 - first.damage)) < .001f);
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Fired &&
              weapon->last_shot.damage == first.damage);
        CHECK(weapon->last_shot.direction.x == first.direction.x && inv.GetAmmoCount(1) == 48);
        fire.alternate = true;
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Fired);
        CHECK(weapon->last_shot.damage >= 10 && weapon->last_shot.damage <= 20);
        const auto before_factor = victim->damage.m_fHitPoints;
        inv.damage_multiplier = .125f; // Original easy Cultist: 0.5 * 0.25.
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Fired);
        CHECK(std::abs(victim->damage.m_fHitPoints - (before_factor - weapon->last_shot.damage * .125f)) < .001f);
        inv.damage_multiplier = std::numeric_limits<float>::quiet_NaN();
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Invalid);
        inv.damage_multiplier = 1;
        fire.weapon = 17;
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Invalid && inv.GetAmmoCount(1) == 46);
        fire.weapon = 1;
        fire.direction.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Invalid);
        fire = shot(game);
        inv.SetAmmoCount(1, 0);
        const auto hp = victim->damage.m_fHitPoints;
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Dry &&
              victim->damage.m_fHitPoints == hp);
        inv.SetAmmoCount(1, 1);
        game.server.collision_ready = false;
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Unavailable &&
              inv.GetAmmoCount(1) == 1);
        return true;
    }
    bool melee(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(entry(game, root));
        std::string error;
        CHECK(game.select_weapon(0, error));
        auto fire = shot(game, 17);
        auto* victim = target(game, vector_add(fire.position, {0, 0, 50}));
        CHECK(victim);
        auto& inv = game.server_shell.Player(game.local_client)->m_InventoryMgr;
        inv.SetAmmoCount(1, 0);
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Fired);
        auto* weapon = inv.CurrentWeapon();
        CHECK(weapon->last_shot.damage >= 15 && weapon->last_shot.damage <= 25 &&
              weapon->last_shot.hit.object == victim->m_hObject);
        CHECK(victim->damage.last_damage.type == DamageType::Melee && inv.GetAmmoCount(0) == 0 &&
              inv.GetAmmoCount(1) == 0);
        fire.alternate = true;
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Fired);
        CHECK(weapon->last_shot.damage >= 35 && weapon->last_shot.damage <= 60);
        auto position = vector_add(fire.position, {0, 0, 100});
        CHECK(game.server.TeleportObject(victim->m_hObject, &position) == DE_OK);
        const auto hp = victim->damage.m_fHitPoints;
        CHECK(game.server_shell.OnMessage(game.local_client, fire) == FireResult::Fired);
        CHECK(weapon->last_shot.collision == SegmentResult::Miss && victim->damage.m_fHitPoints == hp);
        return true;
    }
    bool damage(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(entry(game, root));
        auto* victim = target(game, {0, 20, 0});
        CHECK(victim);
        auto* sender = game.server_shell.Player(game.local_client);
        auto deliver = [&](float amount)
        {
            auto message = game.server.StartMessageToObject(sender, victim->m_hObject, damage_message);
            if (!message)
                return false;
            const auto valid =
                game.server.WriteDamage(
                    message, Damage{{0, 0, 1}, {0, 20, 0}, amount, DamageType::Normal, sender->m_hObject}) == DE_OK;
            return game.server.EndMessage(message) == DE_OK && valid;
        };
        victim->damage.SetArmorPoints(10);
        victim->damage.CalculateResistance(1);
        CHECK(deliver(20));
        CHECK(std::abs(victim->damage.m_fHitPoints - 83.2f) < .001f &&
              std::abs(victim->damage.m_fArmorPoints - 2.8f) < .001f);
        CHECK(!deliver(std::numeric_limits<float>::infinity()) && game.server.message_count() == 0);
        victim->damage.m_bGodMode = true;
        CHECK(deliver(100) && std::abs(victim->damage.m_fHitPoints - 83.2f) < .001f);
        victim->damage.m_bGodMode = false;
        CHECK(deliver(1000) && victim->damage.IsDead() && victim->damage.m_fHitPoints == 0);
        CHECK(deliver(10) && victim->damage.m_fHitPoints == 0);
        return true;
    }
    bool animation(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(entry(game, root));
        auto& client = game.client_shell;
        auto& inv = game.server_shell.Player(game.local_client)->m_InventoryMgr;
        client.weapon_input.primary = true;
        for (int i = 0; i < 120; ++i)
            CHECK(game.step(1.0 / 60));
        CHECK(client.weapons[0].fire_keys > 0 && client.weapons[0].shots > 0 &&
              inv.GetAmmoCount(1) == 50 - client.weapons[0].shots);
        CHECK(client.stats.bullets == inv.GetAmmoCount(1));
        client.weapon_input = {};
        for (int i = 0; i < 60; ++i)
            CHECK(game.step(1.0 / 60));
        const auto shots = client.weapons[0].shots;
        for (int i = 0; i < 60; ++i)
            CHECK(game.step(1.0 / 60));
        CHECK(client.weapons[0].shots == shots);
        client.weapon_input.alternate = true;
        for (int i = 0; i < 120; ++i)
            CHECK(game.step(1.0 / 60));
        CHECK(client.weapons[0].shots > shots && inv.CurrentWeapon()->m_bLastFireAlt);
        CHECK(game.server.message_count() == 0);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(entry(game, root));
        auto fire = shot(game);
        game.client_shell.rotation = {};
        auto* victim = target(game, vector_add(fire.position, {0, 0, 40}), {20, 20, 20});
        CHECK(victim);
        victim->remove_owner = true;
        const auto owner = game.client_shell.player;
        game.client_shell.weapon_input.primary = true;
        for (int i = 0; i < 120 && game.local_client; ++i)
            CHECK(game.step(1.0 / 60));
        CHECK(!game.local_client && !game.objects.get(owner) && !game.client_shell.weapons[0].model);
        CHECK(CDestructable::m_dwNumDestructables == 0 && game.server.message_count() == 0);
        CHECK(game.server_shell.OnMessage(nullptr, fire) == FireResult::Invalid);
        CHECK(game.unload_world() && game.server.string_count() == 0);
        return true;
    }
    bool rollback(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root, true, false));
        const auto count = game.objects.snapshot().size(), strings = game.server.string_count();
        std::string error;
        for (int i = 0; i < 3; ++i)
        {
            CHECK(!game.enter_player(error) && !error.empty());
            CHECK(game.objects.snapshot().size() == count && game.server.string_count() == strings);
            CHECK(!game.local_client && !game.client_shell.weapons[0].model && game.server.message_count() == 0);
            CHECK(CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
    bool private_weapons(const char* root)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = path_from_utf8(root);
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(game.request_world("01_SubwayA", error) && game.enter_player(error));
            CHECK(test_camera::complete_subway_intro(game));
            auto& client = game.client_shell;
            auto& inv = game.server_shell.Player(game.local_client)->m_InventoryMgr;
            for (int slot : {1, 0})
            {
                CHECK(game.select_weapon(static_cast<DBYTE>(slot), error));
                auto& view = client.weapons[0];
                CHECK(view.model);
                std::cout << "weapon " << unsigned(view.type) << ": " << view.model->vertices.size() << " vertices; "
                          << view.model->animations.size() << " clips\n";
                for (const auto& clip : view.model->animations)
                {
                    std::cout << "  " << clip.name << " " << clip.duration << " ms";
                    for (const auto& key : clip.keys)
                        if (!key.text.empty())
                            std::cout << " [" << key.time << ':' << key.text << ']';
                    std::cout << '\n';
                }
                for (bool alternate : {false, true})
                {
                    client.weapon_input = {};
                    for (int i = 0; i < 120; ++i)
                        CHECK(game.step(1.0 / 60));
                    const auto before = view.shots;
                    client.weapon_input = {!alternate, alternate, false};
                    for (int i = 0; i < 180; ++i)
                        CHECK(game.step(1.0 / 60));
                    CHECK(view.shots > before && view.fire_keys > 0);
                    CHECK(view.sound_indexed);
                    CHECK(inv.CurrentWeapon()->sounds_played == inv.CurrentWeapon()->shots &&
                          inv.CurrentWeapon()->sound_failures == 0);
                    std::vector<DVector> pose;
                    CHECK(sample_vertices(*view.model, view.animation.animation, view.animation.time, pose));
                    CHECK(pose.size() == view.model->vertices.size());
                }
            }
            CHECK(inv.ObtainWeapon(WEAP_BERETTA) == InventoryResult::Ok);
            CHECK(game.select_weapon(1, error) && client.weapons[1].model);
            const auto right_shots = inv.GetWeapon(1)->shots;
            client.weapon_input = {true, false, true};
            for (int i = 0; i < 180; ++i)
                CHECK(game.step(1.0 / 60));
            CHECK(client.weapons[1].shots > 0 && inv.GetWeapon(1)->shots == right_shots);
            CHECK(inv.GetAmmoCount(1) < 50 && inv.GetAmmoCount(1) >= 0);
            CHECK(game.unload_world() && !client.weapons[0].model && game.objects.snapshot().empty());
            CHECK(game.server.string_count() == 0 && game.server.message_count() == 0 &&
                  CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view name = argv[1];
    const auto root = path_from_utf8(argv[2]);
    bool ok = false;
    if (name == "inventory")
        ok = inventory(root);
    else if (name == "segments")
        ok = segments(root);
    else if (name == "firing")
        ok = firing(root);
    else if (name == "melee")
        ok = melee(root);
    else if (name == "damage")
        ok = damage(root);
    else if (name == "animation")
        ok = animation(root);
    else if (name == "lifetime")
        ok = lifetime(root);
    else if (name == "rollback")
        ok = rollback(root);
    else if (name == "private")
        ok = private_weapons(argv[2]);
    return ok ? 0 : 1;
}
