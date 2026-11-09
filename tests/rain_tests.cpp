#include "game/original/Rain.h"
#include "game/server_adapter.h"
#include <array>
#include <cmath>
#include <iostream>

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
    bool mount(GameSession& game, const char* path)
    {
        Config config;
        config.data_dir = path;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        return true;
    }

    bool send(GameSession& game, HOBJECT target, const char* command)
    {
        auto message = game.server.StartMessageToObject(game.server.HandleToObject(target), target, 1000);
        auto text = game.server.CreateString(command);
        CHECK(message && text && game.server.WriteToMessageHString(message, text) == DE_OK);
        game.server.FreeString(text);
        CHECK(game.server.EndMessage(message) == DE_OK);
        return true;
    }

    bool effects(const char* path)
    {
        GameSession game;
        CHECK(mount(game, path));
        const auto type = game.objects.find_class("Rain");
        CHECK(type && !game.objects.create(type, {}, Creation::World, {{"Density", -1.f}}));
        const auto handle = game.objects.create(type, {}, Creation::World,
                                                {{"Name", std::string("weather")}, {"Dims", DVector{20, 30, 40}},
                                                 {"Density", 5.f}, {"AddGravity", false}, {"TimeLimit", .5f},
                                                 {"Triggered", true}, {"Direction", DVector{0, -600, 0}}});
        auto* rain = dynamic_cast<Rain*>(game.server.HandleToObject(handle));
        CHECK(rain && !rain->effect && game.server.particle_streams.active.empty());
        CHECK(send(game, handle, "TRIGGEROFF") && !rain->effect);
        CHECK(send(game, handle, "trigger") && rain->effect && rain->error.empty());
        const auto first = rain->effect;
        CHECK(send(game, handle, "TRIGGER") && rain->effect != first);
        auto& pool = game.server.particle_streams;
        CHECK(pool.active.size() == 2);
        std::string error;
        CHECK(game.publish_client_objects(error) && pool.advance(0, game.client_session.world));
        CHECK(pool.active[0].effect.particles.empty());
        CHECK(pool.advance(.125, game.client_session.world));
        for (const auto& entry : pool.active)
        {
            const auto& fx = entry.effect;
            CHECK(fx.settings.gravity == 0 && fx.texture == "SpriteTextures\\blooddrop2.dtx");
            CHECK(fx.emitted == 12 && fx.particles.size() == 12 && fx.dropped == 0);
            CHECK(fx.direction.z == 1); // Owner forward overrides the authored direction.
            CHECK(std::abs(fx.particles[0].position.z - fx.particles[1].position.z - 3) < 1e-5f);
            CHECK(std::abs(fx.particles[1].position.z - fx.particles[2].position.z - 4) < 1e-5f);
            CHECK(std::abs(fx.particles[1].color.y - fx.particles[0].color.y * .8f) < 1e-6f);
        }
        CHECK(pool.advance(.6, game.client_session.world) && pool.active.empty() && pool.expired == 2);
        return true;
    }

    bool restore(const char* path)
    {
        GameSession game;
        CHECK(mount(game, path));
        const auto type = game.objects.find_class("Rain");
        const std::array handles{
            game.objects.create(type, {}, Creation::World,
                                {{"Name", std::string("automatic")}, {"Density", 15.f}, {"Dims", DVector{30, 40, 50}},
                                 {"Pulse", .25f}, {"Color2", DVector{15, 30, 45}}}),
            game.objects.create(type, {}, Creation::World, {{"Name", std::string("manual")}, {"Triggered", true}})};
        CHECK(handles[0] && handles[1] && game.server.particle_streams.active.size() == 1);
        CHECK(send(game, handles[1], "TRIGGER") && game.server.particle_streams.active.size() == 2);
        constexpr const char* current = "Save\\Current\\Current.sav";
        SaveObjectsReport saved;
        CHECK(game.server.save_objects(current, handles, 0, 0, saved) && saved.saved == 2);
        auto image = *game.server.save_store.find(SaveStoreSlot::Current);
        CHECK(game.unload_world());
        game.server.save_store.put(SaveStoreSlot::Current, std::move(image));
        RestoreObjectsReport restored;
        CHECK(game.server.restore_objects(current, 0, RESTOREOBJECTS_RESTORETIME, restored) &&
              restored.objects.size() == 2);
        const auto* automatic = dynamic_cast<Rain*>(game.server.HandleToObject(restored.objects[0]));
        const auto* manual = dynamic_cast<Rain*>(game.server.HandleToObject(restored.objects[1]));
        CHECK(automatic && automatic->effect && automatic->settings.density == 15 &&
              automatic->settings.pulse == .25f && automatic->settings.color2.y == 30);
        CHECK(manual && manual->triggered && !manual->effect && game.server.particle_streams.active.size() == 1);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view test = argv[1];
    return (test == "effects" ? effects(argv[2]) : test == "restore" && restore(argv[2])) ? 0 : 1;
}
