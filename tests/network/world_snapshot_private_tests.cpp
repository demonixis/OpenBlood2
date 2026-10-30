#include "client/network_world.h"
#include "client/object_scene.h"
#include "game/client_replication.h"
#include "game/game_settings.h"
#include "game/server_adapter.h"
#include <iostream>
#include <random>

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
        NetworkWorldReceiver receiver;
        const auto& replica = receiver.world();
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
            CHECK(capture_client_objects(game, captured, error, true));
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
            CHECK(receiver.begin(snapshot.world));
            CHECK(receiver.receive(decoded, geometry, models, sprites, error));
            CHECK(receiver.collision().frame().primary.get() != &game.collision);
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
                const auto* collider = receiver.collision().get(object.id);
                const auto* authoritative = game.objects.get(object.id);
                CHECK(collider && authoritative);
                const auto& shape =
                    authoritative->world_model ? authoritative->world_model : authoritative->container();
                CHECK(bool(collider->shape) == bool(shape));
                if (shape)
                {
                    CHECK(collider->shape.get() != shape.get() &&
                          collider->shape->triangles.size() == shape->triangles.size());
                    for (std::size_t i = 0; i < shape->triangles.size(); ++i)
                        for (unsigned j = 0; j < 3; ++j)
                        {
                            const auto a = shape->triangles[i][j], b = collider->shape->triangles[i][j];
                            CHECK(a.x == b.x && a.y == b.y && a.z == b.z);
                        }
                    ClientMotionState body;
                    body.position = authoritative->state.m_Pos;
                    body.dimensions = authoritative->dimensions;
                    const auto head = vector_scale(vector_add(shape->min, shape->max), .5f);
                    ClientContainerState client;
                    ContainerState server;
                    CHECK(receiver.collision().environment({snapshot.world, object.id}, body, head, client));
                    CHECK(game.server.GetContainers(authoritative->handle, head, server));
                    const auto same = [&](const ClientContainerInfo& a, const ContainerInfo& b)
                    {
                        const auto* owner = game.objects.get(b.object);
                        return a.object == (owner ? owner->id : ObjectId{}) && a.code == b.code &&
                               a.hidden == b.hidden && a.current.x == b.current.x && a.current.y == b.current.y &&
                               a.current.z == b.current.z && a.gravity == b.gravity && a.viscosity == b.viscosity;
                    };
                    CHECK(client.count == server.count && client.total == server.total &&
                          same(client.head, server.head));
                    for (std::size_t i = 0; i < client.count; ++i)
                        CHECK(same(client.body[i], server.body[i]));
                }
            }
            std::mt19937 random(9156);
            for (unsigned probe = 0; probe < 32; ++probe)
            {
                const auto point = [&]
                {
                    const auto axis = [&](float low, float high)
                    { return low + (high - low) * float(random() % 1001) / 1000; };
                    return DVector{axis(geometry.min.x, geometry.max.x), axis(geometry.min.y, geometry.max.y),
                                   axis(geometry.min.z, geometry.max.z)};
                };
                const auto from = point(), to = point();
                SegmentHit server;
                ClientSegmentHit client;
                const auto result = game.server.IntersectSegment(from, to, nullptr, server);
                CHECK(receiver.collision().intersect(from, to, {snapshot.world, {}}, client) == result);
                if (result == SegmentResult::Hit)
                {
                    const auto* owner = game.objects.get(server.object);
                    CHECK(client.object == (owner ? owner->id : ObjectId{}));
                    CHECK(std::abs(client.fraction - server.fraction) < 1e-6 && client.polygon == server.polygon);
                }
            }
            const auto retained = replica;
            const auto retained_collision = receiver.collision();
            CHECK(game.unload_world());
            ModelView scene;
            CHECK(append_client_models(retained, true, scene, resources, error));
            std::cout << "PASS " << path << ": " << snapshot.objects.size() << " objects, " << bytes.size()
                      << " bytes, independent assets/collision survive server unload, 32 matching rays\n";
            CHECK(retained_collision.frame().primary && !retained_collision.frame().objects.empty());
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
