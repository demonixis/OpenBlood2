#include "audio_fixture.h"
#include "engine/collision.h"
#include "game/original/BloodShells.h"
#include "game/original/Corpse.h"
#include <algorithm>
#include <cmath>
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
    bool close(DVector a, DVector b, float epsilon = .002f)
    {
        auto d = vector_subtract(a, b);
        return vector_dot(d, d) < epsilon * epsilon;
    }
    bool floor(GameSession& g)
    {
        WorldModel m;
        m.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        m.surfaces.resize(1);
        m.surfaces[0].flags = 1;
        m.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(m, error));
        g.server.collision_ready = true;
        g.world_path = "diagnostic-player-death.dat";
        g.state = SessionState::Inspection;
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        CHECK(g.enter_player(error));
        return true;
    }
    bool damage(GameSession& g, HOBJECT attacker, DamageType type, float amount = 1000)
    {
        const auto h = g.client_shell.player;
        DVector pos;
        CHECK(g.server.GetObjectPos(h, &pos) == DE_OK);
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(attacker), h, damage_message);
        CHECK(message && g.server.WriteDamage(message, {{0, 0, 1}, pos, amount, type, attacker}) == DE_OK);
        CHECK(g.server.EndMessage(message) == DE_OK);
        return true;
    }
    bool camera(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root) && floor(g));
        auto h = g.client_shell.player;
        DVector body;
        CHECK(g.server.GetObjectPos(h, &body) == DE_OK);
        const auto eye = vector_add(body, {0, 36, 0});
        CPlayerDeathCamera c;
        c.Begin(g.server.GetTime());
        CHECK(c.Update(g.server, h, eye) && c.fraction == 0 && close(c.position, eye));
        // Repeated Begin cannot restart an active orbit, even when the message is duplicated.
        CHECK(g.objects.advance(.5));
        c.Begin(g.server.GetTime());
        CHECK(c.Update(g.server, h, eye) && std::abs(c.angle - .4f) < .00001f && c.fraction == .5f);
        CHECK(close(c.position, vector_add(eye, vector_scale(vector_subtract(c.requested, eye), .5f))));
        auto yaw = rotation_from_euler({0, .7f, 0});
        CHECK(g.server.SetObjectRotation(h, &yaw) == DE_OK);
        CHECK(g.objects.advance(.5) && c.Update(g.server, h, eye));
        // Original world components are transformed by the target basis a second time.
        const auto expected = vector_add(body, {-108 * std::sin(2 * .7f + .8f), 65, -108 * std::cos(2 * .7f + .8f)});
        CHECK(close(c.requested, expected) && close(c.position, expected));
        CHECK(close(rotate_vector(c.rotation, {0, 0, 1}),
                    vector_scale(vector_subtract(eye, c.position),
                                 1 / std::sqrt(vector_dot(vector_subtract(eye, c.position),
                                                          vector_subtract(eye, c.position))))));
        ObjectCreateStruct wall;
        wall.m_Flags = FLAG_SOLID;
        wall.m_Pos = vector_add(c.anchor, vector_scale(vector_subtract(c.requested, c.anchor), .5f));
        auto obstacle = g.objects.create(g.objects.find_class("BaseClass"), wall);
        DVector dims{10, 50, 10};
        CHECK(obstacle && g.server.SetObjectDims(obstacle, &dims) == DE_OK);
        CHECK(c.Update(g.server, h, eye) && c.result == SegmentResult::Hit && c.hit.object == obstacle);
        CHECK(close(c.position, vector_add(c.hit.point, c.hit.normal)));
        const auto fixed = c.position;
        CHECK(c.Update(g.server, h, eye) && close(c.position, fixed));
        CHECK(g.objects.remove(h) && !c.Update(g.server, h, eye) && !c.error.empty());
        CHECK(g.unload_world());
        return true;
    }
    bool guards(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root) && floor(g));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        // A null-killer aggregate fixture: the typed damage boundary correctly rejects
        // forged null senders, so no invalid packet is used to reach this source guard.
        p->m_damage.m_fHitPoints = 0;
        p->m_damage.m_bDead = DTRUE;
        CHECK(g.step(.01) && !p->m_bDead && !p->death_error.empty() && !g.client_shell.death_camera.active);
        // Deliberately unsupported powers must stay visible, with no committed partial death.
        p->m_damage.last_damage.attacker = p->m_hObject;
        p->m_bPowerupActivated[PU_SILENT] = true;
        p->HandleDeath();
        CHECK(!p->m_bDead && p->deaths == 0 && !p->death_error.empty());
        p->m_bPowerupActivated[PU_SILENT] = false;
        // Original freefall commits no corpse. Use a labelled aggregate branch fixture,
        // so this generated ABC does not stand in for the real death animation.
        p->m_damage.reaction_damage_type = DamageType::FreeFall;
        CHECK(g.step(.01) && p->m_bDead && p->deaths == 1 && p->death_messages == 1);
        CHECK(p->death_voice_event == 8 && !p->death_corpses && !p->death_gibs && p->death_error.empty());
        CHECK(g.client_shell.death_camera.active && !g.client_shell.weapons_visible);
        CHECK(!g.client_shell.player_look_allowed());
        CHECK(!p->Respawn());
        PlayerState fields;
        std::string error;
        CHECK(p->SaveFields(fields, error) && fields.dead && fields.move_state == 16);
        auto ammo = p->m_InventoryMgr.GetAmmoCount(1);
        CHECK(p->HandleWeaponFireMessage({}) == FireResult::Invalid && !g.command_weapon(0, error));
        CHECK(!p->OnCommandItem(1) && !p->OnCommandDetonate());
        CHECK(g.step(.05) && p->deaths == 1 && p->m_InventoryMgr.GetAmmoCount(1) == ammo);
        CHECK(g.leave_player() && !g.client_shell.death_camera.active && !g.client_shell.death_notifications);
        CHECK(g.unload_world());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error) && floor(g));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        PlayerRestoreState alive;
        CHECK(g.capture_player(alive, error));
        // These are real resource/model/animation tests on a labelled diagnostic floor,
        // not an authored encounter or a retail playthrough.
        const DamageType types[]{DamageType::Normal, DamageType::Fire, DamageType::Melee, DamageType::FreeFall,
                                 DamageType::Death};
        unsigned bodies = 0, gib_events = 0;
        for (auto type : types)
        {
            p = g.server_shell.Player(g.local_client);
            CHECK(p && !p->m_bDead);
            auto other = g.objects.create(g.objects.find_class("BaseClass"), {});
            CHECK(other);
            CHECK(p->m_InventoryMgr.AddItem(1, 1) == InventoryResult::Ok);
            CHECK(p->m_InventoryMgr.AddItem(2, 1) == InventoryResult::Ok && p->OnCommandItem(2));
            auto light = p->m_InventoryMgr.flashlight->light;
            CHECK(p->m_InventoryMgr.AddKey({"first", "First", "a", "b", 1}) == InventoryResult::Ok);
            CHECK(p->m_InventoryMgr.AddKey({"second", "Second", "a", "b", 1}) == InventoryResult::Ok);
            CHECK(p->m_InventoryMgr.AddInventoryWeapon(7, 2) == InventoryResult::Ok);
            CHECK(p->m_InventoryMgr.AddInventoryWeapon(8, 2) == InventoryResult::Ok);
            // Settle the live body before the lethal hit; death retains its damage impulse.
            for (int i = 0; i < 60; ++i)
                CHECK(g.step(1. / 60));
            const auto before = g.objects.snapshot();
            // Preserve lethal identity after the killer leaves; never dereference it.
            CHECK(damage(g, other, type));
            CHECK(g.objects.remove(other) && g.objects.finish_frame());
            CHECK(g.step(.01));
            CHECK(p->m_bDead && p->deaths == 1 && p->death_messages == 1 && p->death_voice_event == 4);
            CHECK(p->death_error.empty());
            if (!p->death_effect_error.empty())
                std::cerr << "effect: " << p->death_effect_error << '\n';
            CHECK(p->death_effect_error.empty());
            CHECK(!(g.server.GetObjectFlags(p->m_hObject) & FLAG_VISIBLE));
            CHECK(g.server.GetObjectFlags(p->m_hObject) & FLAG_SOLID);
            CHECK(!p->m_InventoryMgr.medkit && !p->m_InventoryMgr.flashlight && !g.server.IsObjectAlive(light));
            CHECK(p->m_InventoryMgr.keys.size() == 1 && p->m_InventoryMgr.keys[0].name == "second");
            CHECK(p->m_InventoryMgr.GetWeapon(10) && p->m_InventoryMgr.GetWeapon(11));
            CHECK(p->m_InventoryMgr.GetAmmoCount(10) == 2 && p->m_InventoryMgr.GetAmmoCount(11) == 2);
            CHECK(p->m_InventoryMgr.m_nCurWeaponSlot == 1);
            CHECK(g.client_shell.death_notifications == 1 && g.client_shell.death_camera.active &&
                  !g.client_shell.weapons_visible);
            auto* corpse = dynamic_cast<CCorpse*>(g.server.HandleToObject(p->last_death_corpse));
            if (type == DamageType::Normal)
            {
                CHECK(corpse && corpse->configured && corpse->sound_root.empty() && corpse->limb_loss);
                CHECK(p->death_slot == 1 && !p->death_gibs && p->death_corpses == 1);
                CHECK(!(g.server.GetObjectFlags(corpse->m_hObject) & FLAG_SOLID));
                CHECK(g.server.GetModelAnimation(corpse->m_hObject) == p->m_Anim_Sound.m_nAnim_DEATH[1]);
                ++bodies;
            }
            else if (type == DamageType::FreeFall)
            {
                CHECK(!corpse && !p->death_corpses && !p->death_gibs);
            }
            else
            {
                // FIRE and MELEE also satisfy the original bitwise EXPLODE (5) check.
                CHECK(!corpse && p->death_gibs == 1);
                ++gib_events;
                CHECK(!g.server.flesh_gibs.active.empty());
            }
            g.client_shell.movement_input.forward = true;
            g.client_shell.weapon_input.primary = true;
            const auto start = g.objects.get(p->m_hObject)->state.m_Pos;
            const auto ammo = p->m_InventoryMgr.GetAmmoCount(1);
            for (int i = 0; i < 90; ++i)
            {
                CHECK(g.step(1. / 60));
                CHECK(g.client_shell.move_mgr.m_dwControlFlags == 0);
            }
            const auto finish = g.objects.get(p->m_hObject)->state.m_Pos;
            std::cout << "death type=" << static_cast<int>(type) << " position " << start.x << "," << start.y << ","
                      << start.z << " -> " << finish.x << "," << finish.y << "," << finish.z
                      << " controls=" << g.client_shell.move_mgr.m_dwControlFlags << "\n";
            CHECK(p->deaths == 1 && close(g.objects.get(p->m_hObject)->velocity, {}, .05f));
            if (type == DamageType::Death)
                CHECK(close(finish, start, .05f) && p->m_damage.damage_impulses == 0);
            else
                CHECK(!close(finish, start, .05f) && p->m_damage.damage_impulses == 1);
            // Keep forward/fire held after passive momentum has stopped. No second
            // impulse, movement command or repeated death may restart the body.
            for (int i = 0; i < 90; ++i)
                CHECK(g.step(1. / 60));
            CHECK(p->deaths == 1 && close(g.objects.get(p->m_hObject)->state.m_Pos, finish, .05f));
            CHECK(p->m_InventoryMgr.GetAmmoCount(1) == ammo && g.client_shell.death_camera.fraction == 1);
            if (corpse)
                CHECK(corpse->animation_done);
            CHECK(g.leave_player());
            if (corpse)
                CHECK(g.server.IsObjectAlive(corpse->m_hObject));
            // Remove this isolated encounter's children before restoring the same live checkpoint.
            for (auto id : g.objects.snapshot())
                if (std::find(before.begin(), before.end(), id) == before.end())
                    CHECK(g.objects.remove(g.objects.get(id)->handle));
            CHECK(g.objects.finish_frame() && g.restore_player(alive, error));
            CHECK(!g.client_shell.death_camera.active && g.client_shell.weapons_visible);
        }
        std::cout << "Original player death: " << bodies << " ordinary corpse / " << gib_events
                  << " gib paths / freefall omission / five live-checkpoint re-associations\n";
        CHECK(g.unload_world() && g.objects.record_count() == 0 && g.server.audio.voices().empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    const std::filesystem::path root = argv[2];
    return (mode == "camera"    ? camera(root)
            : mode == "guards"  ? guards(root)
            : mode == "private" ? private_data(root)
                                : false)
               ? 0
               : 1;
}
