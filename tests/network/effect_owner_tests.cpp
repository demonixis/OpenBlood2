#include "../resource_fixture.h"
#include "client/original_effects.h"
#include "client/session.h"
#include "engine/properties.h"
#include <fstream>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool resources_at(Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        test_model::Writer texture;
        texture.u32(0);
        texture.u32(0xfffffffeu);
        texture.u16(2);
        texture.u16(2);
        texture.u16(1);
        texture.u16(0);
        texture.u32(0);
        texture.u32(0);
        texture.b.resize(1068);
        texture.b[44] = 255;
        texture.b[45] = 255;
        texture.b.resize(1072);
        test_model::Writer sprite;
        for (unsigned v : {1, 10, 1, 0, 0})
            sprite.u32(v);
        const std::string frame = "frame.dtx";
        sprite.u16(frame.size());
        sprite.b.insert(sprite.b.end(), frame.begin(), frame.end());
        test_resource::Directory dir;
        dir.files["frame.dtx"] = texture.b;
        dir.files["gate.spr"] = sprite.b;
        dir.children["spritetextures"].files["lensflare32_1.dtx"] = texture.b;
        test_model::Writer rez;
        rez.b.resize(168);
        rez.b[0] = rez.b[62] = rez.b[124] = 13;
        rez.b[1] = rez.b[63] = rez.b[125] = 10;
        rez.b[126] = 26;
        rez.patch(127, 1);
        const auto [offset, size] = test_resource::write_dir(rez, dir);
        rez.patch(131, offset);
        rez.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        if (ec)
            return false;
        {
            std::ofstream out(root / "owners.rez", std::ios::binary);
            out.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
            if (!out)
                return false;
        }
        return resources.mount(root / "owners.rez", error);
    }
    bool run(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(resources_at(resources, root, error));
        SpriteStore sprites(resources);
        ClientSession client(std::make_unique<OriginalEffects>());
        auto& fx = static_cast<OriginalEffects&>(*client.effect_runtime());
        CHECK(fx.world == client.world.frame().world);
        ClientWorldFrame frame;
        frame.world = fx.world;
        frame.sequence = 1;
        ClientObject object;
        object.id = {3, 7};
        object.pose.position = {10, 20, 30};
        object.user_flags = 1;
        frame.objects.push_back(object);
        auto publish = [&]
        {
            auto copy = frame;
            return client.receive(std::move(copy), error);
        };
        CHECK(publish());
        auto create = [&]
        {
            const EffectOwner owner{client.world.reference(frame.objects[0].id), frame.objects[0].pose,
                                    frame.objects[0].user_flags, "synthetic owner"};
            PolyGridSettings grid;
            grid.dims = {10, 0, 10};
            CHECK(fx.polygrids.publish(owner, grid, sprites, frame.time, error));
            CHECK(!fx.polygrids.publish(owner, grid, sprites, frame.time, error));
            ParticleStreamSettings stream;
            stream.texture = "frame.dtx";
            stream.on = true;
            stream.count = 3;
            stream.delay = .05f;
            CHECK(fx.particle_streams.publish(owner, stream, &resources, frame.time, error));
            CHECK(!fx.particle_streams.publish(owner, stream, &resources, frame.time, error));
            CHECK(fx.object_particles.publish(owner, 9, &resources, frame.time, error));
            WarpGateSettings warp;
            warp.sprite.resource = "gate.spr";
            for (auto& system : warp.particles)
            {
                system.resource = "frame.dtx";
                system.count = 2;
            }
            CHECK(fx.warp_gates.publish(owner, warp, sprites, resources, frame.time, error));
            CHECK(error.empty());
            CLightFX light;
            CHECK(light.Init(owner.id, {}, float(frame.time), owner.pose.position));
            fx.light_effects.push_back(light);
            return true;
        };
        auto advance = [&](double now, const ClientWorld* replacement = nullptr)
        {
            const auto& scene = replacement ? *replacement : client.world;
            return fx.polygrids.advance(now, scene) && fx.particle_streams.advance(now, scene) &&
                   fx.object_particles.advance(now, scene) && fx.warp_gates.advance(now, scene) &&
                   fx.advance_lights(float(now), scene);
        };
        auto empty = [&]
        {
            return fx.polygrids.active.empty() && fx.particle_streams.active.empty() &&
                   fx.object_particles.active.empty() && fx.warp_gates.active.empty() && fx.light_effects.empty();
        };
        CHECK(create() && advance(.01));
        const auto grid_updates = fx.polygrids.active[0].effect.updates;
        const auto initial_rotation = fx.polygrids.active[0].effect.pose.rotation;
        frame.sequence++;
        frame.time = .1;
        frame.objects[0].pose.position = {40, 50, 60};
        frame.objects[0].pose.rotation = rotation_from_euler({0, 1, 0});
        frame.objects[0].user_flags = 0;
        CHECK(publish() && advance(.1));
        CHECK(fx.polygrids.active[0].effect.pose.position.x == 40 && !fx.polygrids.active[0].effect.visible &&
              fx.polygrids.active[0].effect.updates == grid_updates);
        CHECK(fx.polygrids.active[0].effect.pose.rotation.m_Spin == initial_rotation.m_Spin);
        CHECK(fx.particle_streams.active[0].effect.position.x == 40 && fx.particle_streams.active[0].effect.state == 0);
        CHECK(fx.object_particles.active[0].effect.position.x == 40);
        CHECK(fx.warp_gates.active[0].sprite.pose.position.x == 40);
        CHECK(fx.light_effects[0].position.x == 40 && !fx.light_effects[0].visible);
        // Same slot, different generation must never inherit the previous owner's effects.
        frame.sequence++;
        frame.time = .2;
        frame.objects[0].id.generation++;
        CHECK(publish() && advance(.2) && empty());
        CHECK(fx.polygrids.expired == 1 && fx.particle_streams.expired == 1 && fx.object_particles.expired == 1 &&
              fx.warp_gates.expired == 1);
        CHECK(create() && advance(.21));
        // A new world with the same object slot/generation is also a different owner.
        ClientWorld replacement;
        CHECK(replacement.begin(frame.world + 1));
        frame.world++;
        frame.sequence = 1;
        CHECK(replacement.apply(std::move(frame), error) && advance(.22, &replacement) && empty());
        client.reset_local_world();
        CHECK(fx.world == client.world.frame().world && empty() && !fx.polygrids.failed);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
