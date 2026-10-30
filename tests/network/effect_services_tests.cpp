#include "../brush_fixture.h"
#include "../resource_fixture.h"
#include "audio_fixture.h"
#include "client/effect_services.h"
#include "client/original_effects.h"
#include "client/session.h"
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
    bool setup(Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        CHECK(test_network_audio::setup(resources, root, error));
        std::vector<std::uint8_t> wave;
        CHECK(resources.read("tone.wav", wave, error));
        test_model::Writer texture;
        texture.u32(0);
        texture.u32(0xfffffffeu);
        texture.u16(2);
        texture.u16(2);
        texture.u16(1);
        texture.u16(0);
        texture.u32(0);
        texture.u32(0);
        texture.b.resize(1072);
        texture.b[44] = texture.b[45] = 255;
        test_model::Writer sprite;
        for (unsigned v : {1, 10, 1, 0, 0})
            sprite.u32(v);
        const std::string frame = "frame.dtx";
        sprite.u16(frame.size());
        sprite.b.insert(sprite.b.end(), frame.begin(), frame.end());
        test_resource::Directory dir;
        dir.files[frame] = texture.b;
        for (unsigned i = 0; i <= 3; ++i)
        {
            const auto n = std::to_string(i);
            dir.children["sounds"].children["gibs"].children["flesh"].files["gib_impact" + n + ".wav"] = wave;
            if (!i)
                continue;
            dir.children["spritetextures"].children["particles"].files["blooddrop_" + n + ".dtx"] = texture.b;
            dir.children["sprites"].files["blood" + n + ".spr"] = sprite.b;
            dir.children["sprites"].files["smokepuff" + n + ".spr"] = sprite.b;
            dir.children["models"].children["gibs"].children["flesh"].files["gib" + n + ".abc"] = test_model::abc().b;
            dir.children["skins"].children["gibs"].children["flesh"].files["gib" + n + ".dtx"] = texture.b;
        }
        for (unsigned i : {1, 2})
            dir.children["sounds"]
                .children["weapons"]
                .children["shelldrops"]
                .children["stone"]
                .files["shell" + std::to_string(i) + ".wav"] = wave;
        dir.children["models"].children["ammo"].files["sshell.abc"] = test_model::abc().b;
        dir.children["skins"].children["ammo"].files["sshell.dtx"] = texture.b;
        dir.children["spritetextures"].children["particles"].files["particlebubble.dtx"] = texture.b;
        test_model::Writer rez;
        rez.b.resize(168);
        rez.b[0] = rez.b[62] = rez.b[124] = 13;
        rez.b[1] = rez.b[63] = rez.b[125] = 10;
        rez.b[126] = 26;
        rez.patch(127, 1);
        const auto [offset, size] = test_resource::write_dir(rez, dir);
        rez.patch(131, offset);
        rez.patch(135, size);
        {
            std::ofstream out(root / "effects.rez", std::ios::binary);
            out.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
            CHECK(out);
        }
        CHECK(resources.mount(root / "effects.rez", error));
        return true;
    }
    bool run(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(setup(resources, root, error));
        ModelStore models(resources);
        SpriteStore sprites(resources);
        ClientSession client(std::make_unique<OriginalEffects>()), other(std::make_unique<OriginalEffects>());
        auto& fx = static_cast<OriginalEffects&>(*client.effect_runtime());
        auto& untouched = static_cast<OriginalEffects&>(*other.effect_runtime());
        auto primary = std::make_shared<CollisionWorld>();
        auto floor = test_brush::box("floor", {-4000, -100, -4000}, {4000, 0, 4000});
        for (auto& surface : floor.surfaces)
            surface.flags = 1;
        CHECK(primary->build(floor, error));
        ClientCollisionFrame frame;
        frame.world = client.world.frame().world;
        frame.sequence = 1;
        frame.primary = primary;
        CHECK(client.collision.apply(std::move(frame), error));
        primary.reset(); // All motion below uses only the client's retained scene.
        ClientEffectQueries queries(&client.collision), unavailable(nullptr);
        ClientSegmentHit hit;
        CHECK(queries.effect_world_segment({0, 1, 0}, {0, -1, 0}, hit) == SegmentResult::Hit);
        EffectServices services{queries, fx, &resources, &sprites, &client.audio, 0, .01};
        EffectOwner owner{{fx.world, {1, 1}}, {}, 0, "detached effect source"};
        CHECK(fx.flesh_gibs.publish(owner, {0, 1, 0}, {0, -200, 0}, {}, 1, 2, services, models, resources, 0, error));
        CHECK(error.empty() && fx.flesh_gibs.active.size() >= 2 && !fx.blood_spurts.active.empty());
        // Immediate launch emits the first burst and can hit before the next update.
        CHECK(fx.blood_spurts.active.front().updates == 1 && fx.blood_spurts.active.front().emitted == 12);
        DebrisEffect debris;
        debris.source = "synthetic debris";
        debris.position = {20, 1, 0};
        debris.velocity = {0, -200, 0};
        debris.flags = 0x4000 | 3;
        debris.amount = 1;
        debris.model = "models/gibs/flesh/gib1.abc";
        debris.texture = "skins/gibs/flesh/gib1.dtx";
        debris.sound = "sounds/gibs/flesh/gib_impact1.wav";
        CHECK(fx.destruction_debris.publish_debris(debris, 2, services, models, resources, 0, error) && error.empty());
        ShotgunEffectSettings shot;
        shot.eject = true;
        shot.source = {50, 10, 0};
        shot.destination = {100, 10, 0};
        shot.forward = {1, 0, 0};
        shot.normal = {0, 1, 0};
        CHECK(fx.shotgun_effects.publish(owner, shot, models, sprites, resources, 0, services, error) && error.empty());
        CHECK(fx.shotgun_effects.casings.size() == 2 && !fx.weapon_sprites.smoke.empty());
        for (unsigned tick = 1; tick <= 180; ++tick)
        {
            services.time = tick * .01;
            CHECK(fx.flesh_gibs.advance(services.time, services));
            CHECK(fx.destruction_debris.advance(services.time, services));
            fx.shotgun_effects.advance(services.time, services);
            CHECK(fx.shotgun_effects.update_error.empty());
            CHECK(fx.blood_spurts.advance(services.time, services));
            CHECK(fx.blood_splats.advance(services.time));
            CHECK(client.audio.advance(.01, {}, {}));
        }
        CHECK(fx.flesh_gibs.active.front().impacts > 0 && fx.flesh_gibs.active.front().splat_event);
        CHECK(fx.destruction_debris.active.front().impacts > 0 && !fx.destruction_debris.active.front().blood);
        CHECK(fx.shotgun_effects.casings.front().impacts > 0 && fx.shotgun_effects.casings.front().sound_error.empty());
        CHECK(client.audio.effects_started > 0 && client.audio.effects_failed == 0 && !fx.blood_splats.active.empty());
        CHECK(untouched.flesh_gibs.active.empty() && untouched.blood_splats.active.empty() &&
              other.audio.effects_started == 0);
        EffectServices missing{unavailable, fx, &resources, &sprites, nullptr, services.time, .01};
        BloodSpurtSettings blood;
        blood.position = {0, 10, 0};
        CParticleExplosionFX rejected;
        std::minstd_rand random{1};
        CHECK(!rejected.CreateObject(blood, resources, missing.queries, services.time, random, error) &&
              !error.empty());
        SoundParameters sound;
        sound.resource = "tone.wav";
        CHECK(!missing.sound(sound, {}, error) && !error.empty());
        const auto epoch = client.world.frame().world;
        client.reset_local_world();
        CHECK(client.world.frame().world > epoch && fx.flesh_gibs.active.empty() && fx.blood_splats.active.empty() &&
              client.audio.mixer.voices().empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
