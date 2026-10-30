#include "client/network_world.h"
#include "client/object_scene.h"
#include "game/client_replication.h"
#include "game/game_settings.h"
#include "game/server_adapter.h"
#include <iostream>

using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << "\n";                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        GameSession game;
        CHECK(game.initialize());
        game.server.client_audio = nullptr;
        CHECK(game.resources.mount_game(config, error));
        Resources resources;
        CHECK(resources.mount_game(config, error));
        ModelStore models(resources);
        SpriteStore sprites(resources);
        ClientWorld replica;
        unsigned maps = 0;
        std::size_t largest = 0;
        for (const auto& [path, resource] : resources.index())
        {
            (void)resource;
            const bool ctf = path.starts_with("worlds/multi/ctf_");
            if ((!ctf && !path.starts_with("worlds/multi/bb_")) || !path.ends_with(".dat"))
                continue;
            WorldRequest request;
            request.filename = path;
            request.game_type = ctf ? GAMETYPE_CTF : GAMETYPE_BLOODBATH;
            CHECK(game.load_request(request, error));
            std::vector<std::uint8_t> data;
            World geometry;
            CHECK(resources.read(path, data, error) && read_world(data, geometry, error));
            const auto clock = game.server.save_clock();
            ClientWorldFrame captured;
            captured.world = ++maps;
            captured.sequence = 1;
            CHECK(capture_client_objects(game, captured, error));
            CHECK(game.server.save_clock() == clock);
            const auto snapshot = snapshot_values(captured);
            std::vector<std::uint8_t> bytes;
            if (!encode_world_snapshot(snapshot, bytes))
            {
                std::cerr << "Cannot encode " << path << '\n';
                for (const auto& object : snapshot.objects)
                {
                    auto probe = snapshot;
                    probe.objects = {object};
                    if (!valid_snapshot(probe))
                        std::cerr << "Invalid object " << object.id.slot << " type " << object.type << " "
                                  << object.filename << '\n';
                }
                CHECK(false);
            }
            largest = std::max(largest, bytes.size());
            WorldSnapshot decoded;
            CHECK(decode_world_snapshot(bytes, decoded));
            CHECK(replica.begin(snapshot.world));
            CHECK(receive_network_world(decoded, geometry, models, sprites, replica, error));
            std::vector<std::uint8_t> roundtrip;
            CHECK(encode_world_snapshot(snapshot_values(replica.frame()), roundtrip) && roundtrip == bytes);
            for (const auto& object : captured.objects)
            {
                const auto* remote = replica.get(object.id);
                CHECK(remote);
                if (object.model)
                    CHECK(remote->model && remote->model.get() != object.model.get());
                if (object.sprite)
                    CHECK(remote->sprite && remote->sprite.get() != object.sprite.get());
            }
            const auto retained = replica;
            CHECK(game.unload_world());
            ModelView scene;
            CHECK(append_client_models(retained, true, scene, resources, error));
            std::cout << "PASS " << path << ": " << snapshot.objects.size() << " objects, " << bytes.size()
                      << " bytes, independent assets survive server unload\n";
        }
        CHECK(maps == 22);
        std::cout << maps << " world baselines; largest " << largest << " bytes\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
