#include "client/session.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run()
    {
        ClientSession client;
        const auto world = client.world.frame().world;
        std::string error;
        ClientCollisionFrame scene;
        scene.world = world;
        scene.sequence = 1;
        scene.primary = std::make_shared<CollisionWorld>();
        for (unsigned i = 0; i < 5; ++i)
        {
            ClientCollider object;
            object.id = {i, 1};
            object.position = {float(i * 100), 41, 0};
            object.dimensions = {16, 41, 16};
            scene.objects.push_back(object);
        }
        CHECK(client.collision.apply(std::move(scene), error));
        ClientCameraFrame frame;
        frame.world = world;
        frame.sequence = 1;
        frame.time = 3;
        frame.player = {{0, 1}, {0, 41, 0}, {}};
        frame.eye_level = 29;
        frame.alive = true;
        ClientCameraDescriptor first, second;
        first.object = {2, 1};
        first.position = {200, 100, 20};
        first.active = true;
        first.settings = {1, false, true, false};
        second.object = {1, 1};
        second.position = {100, 150, 10};
        second.active = true;
        frame.cameras = {first, second}; // Creation order, not slot order.
        CHECK(client.camera.receive(std::move(frame), error));
        auto& camera = client.camera;
        camera.position = {0, 41, 0};
        camera.camera_position = {0, 70, 0};
        CHECK(camera.select(client.collision, 0, 0, false, error));
        CHECK(camera.view.object == first.object && camera.view.settings.type == 1);
        CHECK(!camera.player_visible && !camera.weapons_visible && camera.listener_position.y == 41);
        CHECK(!camera.look_allowed() && !camera.can_toggle(true));
        frame = camera.frame();
        ++frame.sequence;
        frame.eye = {{3, 1}, {300, 60, 0}, {}};
        frame.orb = {{4, 1}, {400, 50, 0}, {}};
        CHECK(camera.receive(std::move(frame), error) && camera.select(client.collision, .2f, 1, false, error));
        CHECK(camera.kind == ClientCameraKind::Eye && camera.view.position.x == 300 && camera.look_allowed());
        CHECK(camera.listener_position.x == 300 && camera.player_visible && !camera.weapons_visible);
        frame = camera.frame();
        ++frame.sequence;
        frame.eye = {};
        CHECK(camera.receive(std::move(frame), error) && camera.select(client.collision, .2f, 1, false, error));
        CHECK(camera.kind == ClientCameraKind::Orb && camera.view.position.x == 400);
        frame = camera.frame();
        ++frame.sequence;
        frame.alive = false;
        camera.death.Begin(2.7f);
        CHECK(camera.receive(std::move(frame), error) && camera.select(client.collision, 0, 0, false, error));
        CHECK(camera.kind == ClientCameraKind::Scripted && !camera.player_visible && !camera.look_allowed());
        frame = camera.frame();
        ++frame.sequence;
        frame.cameras.clear();
        CHECK(camera.receive(std::move(frame), error) && camera.select(client.collision, 0, 0, false, error));
        CHECK(camera.kind == ClientCameraKind::Death && !camera.view.object);
        CHECK(std::abs(camera.death.fraction - .3f) < .00001f && std::abs(camera.death.angle - .24f) < .00001f);
        camera.death = {};
        camera.third_person = true;
        frame = camera.frame();
        ++frame.sequence;
        frame.eye = frame.orb = {};
        frame.alive = true;
        CHECK(camera.receive(std::move(frame), error) && camera.select(client.collision, 0, 0, false, error));
        CHECK(camera.kind == ClientCameraKind::Chase && camera.third_person_active && camera.player_visible);
        CHECK(camera.view.position.z == -150 && camera.view.position.y == 90);
        CHECK(camera.select(client.collision, 0, 0, true, error) && !camera.third_person_active && camera.third_person);
        camera.xr_view = true;
        CHECK(camera.select(client.collision, 0, 0, false, error) && !camera.third_person && !camera.can_toggle(true));
        auto rejects = [&](ClientCameraFrame invalid)
        {
            const auto sequence = camera.frame().sequence;
            return !camera.receive(std::move(invalid), error) && camera.frame().sequence == sequence;
        };
        CHECK(rejects(camera.frame()));
        frame = camera.frame();
        ++frame.sequence;
        ++frame.world;
        CHECK(rejects(frame));
        frame = camera.frame();
        ++frame.sequence;
        frame.time = std::numeric_limits<float>::quiet_NaN();
        CHECK(rejects(frame));
        frame = camera.frame();
        ++frame.sequence;
        frame.cameras = {first, first};
        CHECK(rejects(frame));
        frame.cameras = {first};
        frame.cameras[0].settings.type = 3;
        CHECK(rejects(frame));
        frame.cameras.resize(4097, first);
        CHECK(rejects(frame));
        frame = camera.frame();
        ++frame.sequence;
        frame.player.object.generation = 0;
        CHECK(rejects(frame));
        frame = camera.frame();
        ++frame.sequence;
        frame.player.rotation.m_Spin = 0;
        CHECK(rejects(frame));
        const ClientObjectId owner{world, {0, 1}};
        CHECK(client.hud.bind(owner));
        PlayerStats stats;
        stats.weapon = 1;
        stats.health = 75;
        stats.armor = 25;
        stats.ammo = 8;
        auto reserved = stats;
        reserved.weapon_slot = 12; // Three reserved inventory slots are outside the ten displayed normal slots.
        CHECK(valid_hud_stats(reserved));
        reserved.weapon_slot = 13;
        CHECK(!valid_hud_stats(reserved));
        CHECK(client.hud.receive(owner, 1, stats));
        CHECK(!client.hud.receive(owner, 1, stats));
        CHECK(!client.hud.receive({world + 1, owner.object}, 2, stats));
        CHECK(!client.hud.receive({world, {0, 2}}, 2, stats));
        auto bad = stats;
        bad.max_health = -1;
        CHECK(!client.hud.receive(owner, 2, bad) && client.hud.stats == stats);
        bad = stats;
        bad.health = std::numeric_limits<float>::infinity();
        CHECK(!client.hud.receive(owner, 2, bad));
        InventoryItems items;
        items.show = true;
        items.current = InventoryItemView{1, "medkit.pcx", "medkit_h.pcx", "", 301, 50};
        CHECK(client.hud.receive(owner, 2, items) && client.hud.item_messages == 1 && client.hud.item_shows == 1);
        CHECK(!client.hud.items.show && !client.hud.receive(owner, 2, items) && client.hud.item_shows == 1);
        items.current->icon = "../escape.pcx";
        CHECK(!client.hud.receive(owner, 3, items) && client.hud.item_messages == 1);
        auto hud = hud_state(client.hud.stats, true, false);
        CHECK(hud.visible && hud.crosshair_visible && hud.stats.health == 75);
        CHECK(!hud_state(stats, true, true).visible && !hud_state(stats, true, true).crosshair_visible);
        CHECK(hud_state(stats, true, false, true).visible && !hud_state(stats, true, false, true).crosshair_visible);
        auto detached = client;
        client.camera.xr_view = true;
        client.reset_local_world();
        CHECK(!client.hud.owner().object && !client.camera.frame().player.object && client.camera.xr_view);
        CHECK(!client.hud.receive(owner, 3, stats) && !client.camera.select(client.collision, 0, 0, false, error));
        CHECK(detached.hud.stats == stats && detached.camera.select(detached.collision, 0, 0, false, error));
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
