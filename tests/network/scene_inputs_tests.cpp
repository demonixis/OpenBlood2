#include "app/model_lighting.h"
#include "app/model_scene.h"
#include "app/polygrid_scene.h"
#include "app/sprite_scene.h"
#include "client/network_audio.h"
#include "client/network_effects.h"
#include "client/network_world.h"
#include "effect_fixture.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#ifdef BLOOD2_SCENE_GPU
#include "app/scene_presenter.h"
#include "engine/capture.h"
#include "engine/world_shaders.h"
#endif
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
#ifdef BLOOD2_SCENE_GPU
    bool render(SDL_GPUDevice* gpu, ScenePresenter& presenter, const ClientScene& input, Capture& capture,
                std::vector<Uint8>& pixels, std::string& error)
    {
        CHECK(presenter.update(input, error) && presenter.render_world);
        auto* commands = SDL_AcquireGPUCommandBuffer(gpu);
        CHECK(commands && presenter.record(input, commands, capture.texture, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                           capture.width, capture.height, error));
        auto* copy = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureRegion source{};
        source.texture = capture.texture;
        source.w = capture.width;
        source.h = capture.height;
        source.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = capture.transfer;
        destination.pixels_per_row = capture.width;
        destination.rows_per_layer = capture.height;
        SDL_DownloadFromGPUTexture(copy, &source, &destination);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(gpu));
        auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu, capture.transfer, false));
        CHECK(bytes);
        pixels.assign(bytes, bytes + std::size_t(capture.width) * capture.height * 4);
        SDL_UnmapGPUTransferBuffer(gpu, capture.transfer);
        return true;
    }
#endif
    bool run(const char* data, bool ballistics, bool impacts, bool instants, bool streams, bool objects)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        Resources resources;
        ModelStore models(resources);
        SpriteStore sprites(resources);
        CHECK(resources.mount_game(config, error));
        std::vector<std::uint8_t> bytes;
        World geometry;
        constexpr auto path = "worlds/multi/bb_crypt.dat";
        CHECK(resources.read(path, bytes, error) && read_world(bytes, geometry, error));
        DVector eye;
        bool found = false;
        for (const auto& object : geometry.objects)
            if (object.class_name == "GameStartPoint")
                if (const auto* p = object.find("Pos"))
                    if (const auto* v = std::get_if<DVector>(p))
                    {
                        eye = *v;
                        eye.y += 36;
                        found = true;
                        break;
                    }
        CHECK(found);
        ClientWeaponView weapon;
        weapon.type = WEAP_BERETTA;
        const auto& definition = *weapon_definition(weapon.type);
        weapon.model_path = definition.m_szViewModelFilename;
        weapon.skin_path = definition.m_szViewModelSkin;
        CHECK(models.load(weapon.model_path, weapon.model, error));
        weapon.animation.animation = animation_index(*weapon.model, "static_model");
        CHECK(weapon.animation.animation != no_animation);
        ClientWorldFrame values;
        values.world = 7;
        values.sequence = 1;
        values.player = {0, 1};
        values.player_alive = true;
        ClientObject object;
        object.id = values.player;
        object.type = OT_MODEL;
        object.model = weapon.model;
        object.filename = weapon.model_path;
        object.skin = weapon.skin_path;
        object.animation = weapon.animation;
        object.flags = FLAG_VISIBLE | FLAG_NOLIGHT;
        if (ballistics)
        {
            object.collision.enabled = true;
            object.collision.motion.dimensions = {12, 36, 12};
        }
        object.pose.position = eye;
        values.objects.push_back(object);
        if (streams)
            for (unsigned kind = 0; kind < 2; ++kind)
            {
                ClientObject emitter;
                emitter.id = {kind + 1, 1};
                emitter.user_flags = 1;
                emitter.pose.position = vector_add(eye, {kind ? 20.f : -20.f, 0, 80});
                emitter.particle_stream.emplace();
                auto& recipe = *emitter.particle_stream;
                recipe.on = true;
                recipe.system = kind != 0;
                recipe.count = 12;
                recipe.particles_per_second = 600;
                recipe.position_radius = 10;
                recipe.radius = 1500;
                recipe.min_color = recipe.max_color = kind ? DVector{255, 40, 40} : DVector{40, 255, 40};
                values.objects.push_back(std::move(emitter));
            }
        if (objects)
            values.time = 10;
        if (objects)
            for (unsigned i = 0; i < 3; ++i)
            {
                ClientObject emitter;
                emitter.id = {i + 1, 1};
                emitter.pose.position = vector_add(eye, {float(i * 20) - 20, -5, 100});
                // Same-owner death-ray rings plus an independent shield/trail.
                const unsigned types[]{37, 19, 2};
                emitter.object_particles.push_back({i * 2 + 1, 10, types[i], 1, {}, "scene wire recipe"});
                if (!i)
                    emitter.object_particles.push_back({2, 10, 38, 1, {}, "scene wire recipe"});
                values.objects.push_back(std::move(emitter));
            }
        Net::WorldSnapshot decoded;
        CHECK(Net::encode_world_snapshot(Net::snapshot_values(values), bytes) &&
              Net::decode_world_snapshot(bytes, decoded));
        NetworkWorldReceiver receiver;
        CHECK(receiver.begin(7) && receiver.receive(decoded, geometry, models, sprites, error));
        ClientEffects effects;
        CHECK(effects.begin(7));
        ClientScene input{geometry, resources, receiver.world(), effects};
        input.world_path = path;
        input.world_revision = 1;
        input.in_world = true;
        input.view.position = input.camera_position = eye;
        input.weapons[1] = &weapon;
        ModelView scene, explosions;
        SpriteView sprite;
        PolyGridView grid;
        CHECK(update_model_scene(input, scene, error));
        CHECK(scene.close_draws.size() == 1 && scene.world_draws.empty());
        CHECK(update_explosion_model_scene(input, explosions, error) && explosions.world_draws.empty());
        CHECK(update_sprite_scene(input, sprite, error) && update_polygrid_scene(input, grid, error));
        const auto preserved = weapon.animation;
        input.player_visible = true;
        CHECK(update_model_scene(input, scene, error) && scene.world_draws.size() == 1);
        input.player_visible = false;
        weapon.flash_visible = 3;
        CHECK(!update_sprite_scene(input, sprite, error) && error == "Invalid client muzzle flash state");
        weapon.flash_visible = 0;
        CHECK(weapon.animation.time == preserved.time && weapon.animation.revision == preserved.revision);
        WorldMesh mesh;
        CHECK(mesh.build(geometry, error, &resources));
        ModelLighting lighting;
        CHECK(lighting.build(mesh, geometry, error));
        std::vector<WorldDraw> draws{{0, {}, {}}};
        CHECK(lighting.apply(receiver.world(), eye, false, mesh, draws, scene, error));
        ClientEffects wrong_effects;
        CHECK(wrong_effects.begin(8));
        ClientScene wrong_world{geometry, resources, receiver.world(), wrong_effects};
        CHECK(!update_polygrid_scene(wrong_world, grid, error));
        CHECK(!update_model_scene(wrong_world, scene, error));
        CHECK(!update_explosion_model_scene(wrong_world, explosions, error));
        CHECK(!update_sprite_scene(wrong_world, sprite, error));
        NetworkEffects ballistic;
        NetworkAudio audio;
        const auto add_ballistic = [&]()
        {
            CHECK(ballistic.begin({7, 1, {0, 1}}) && audio.begin({7, 1, {0, 1}}));
            Net::EffectEvent event{7, 1, 1, {0, 1}, "synthetic wire impact", {}};
            auto& shot = std::get<ShotgunEffectSettings>(event.recipe);
            shot.source = vector_add(eye, {-10, -5, 40});
            shot.destination = vector_add(eye, {0, 0, 60});
            shot.forward = {0, 0, 1};
            shot.normal = {0, 0, -1};
            shot.eject = true;
            shot.ammo = AMMO_BULLET;
            shot.surface = SURFTYPE_METAL;
            Net::EffectEvent decoded_effect;
            CHECK(Net::encode_effect(event, bytes) &&
                  Net::decode_effect(bytes, decoded_effect) == Net::DecodeEffect::Ok);
            if (!streams && !objects)
                CHECK(ballistic.receive(decoded_effect, error));
            if (impacts)
                for (unsigned kind = 1; kind < (instants ? 10u : 5u); ++kind)
                {
                    auto message = effect_fixture::sample(kind);
                    message.player = 1;
                    const auto at = vector_add(eye, {float(kind * 4) - 10, -4, 35});
                    if (auto* sprites = std::get_if<WeaponSpriteSettings>(&message.recipe))
                    {
                        sprites->flash->position = sprites->smoke->position = at;
                        sprites->flash->normal = sprites->smoke->normal = {0, 0, -1};
                    }
                    else if (auto* spark = std::get_if<SparkSettings>(&message.recipe))
                        spark->position = at;
                    else if (auto* blood = std::get_if<BloodSpurtRecipe>(&message.recipe))
                        blood->settings.position = at;
                    else if (auto* splat = std::get_if<BloodSplatSettings>(&message.recipe))
                        splat->position = at;
                    else if (auto* explosion = std::get_if<ExplosionRecipe>(&message.recipe))
                    {
                        explosion->position = vector_add(eye, {20, -5, 150});
                        explosion->normal = {0, 0, -1};
                    }
                    else if (auto* flesh = std::get_if<FleshRecipe>(&message.recipe))
                        flesh->position = at;
                    else if (auto* debris = std::get_if<DebrisRecipe>(&message.recipe))
                        debris->position = at;
                    else if (auto* laser = std::get_if<LaserRecipe>(&message.recipe))
                    {
                        laser->source = vector_add(eye, {-25, -5, 50});
                        laser->destination = vector_add(eye, {25, 5, 75});
                    }
                    else if (auto* bolt = std::get_if<LightningSettings>(&message.recipe))
                    {
                        bolt->source = vector_add(eye, {-25, 10, 70});
                        bolt->destination = vector_add(eye, {25, 20, 95});
                    }
                    CHECK(Net::encode_effect(message, bytes) &&
                          Net::decode_effect(bytes, decoded_effect) == Net::DecodeEffect::Ok &&
                          ballistic.receive(decoded_effect, error));
                }
            if (instants)
                for (unsigned component = 1; component < 6; ++component)
                {
                    auto e = effect_fixture::sample(5);
                    e.player = 1;
                    e.sequence = 10 + component;
                    auto& s = std::get<ExplosionRecipe>(e.recipe);
                    s.component = ExplosionComponent(component);
                    if (s.component == ExplosionComponent::Flames)
                        s.type = 8;
                    s.position = vector_add(eye, {20, -5, 150});
                    s.normal = {0, 0, -1};
                    CHECK(Net::encode_effect(e, bytes) &&
                          Net::decode_effect(bytes, decoded_effect) == Net::DecodeEffect::Ok &&
                          ballistic.receive(decoded_effect, error));
                }
            CHECK(ballistic.advance(1. / 60, receiver.world(), receiver.collision(), models, sprites, resources, audio,
                                    error));
            CHECK(ballistic.advance(1. / 60, receiver.world(), receiver.collision(), models, sprites, resources, audio,
                                    error));
            if (ballistic.failed)
                error = ballistic.last_error;
            CHECK(!ballistic.failed);
            auto frame = ballistic.scene().frame();
            CHECK(effects.receive(std::move(frame), error));
            CHECK(instants ? effects.frame().models.size() >= 12
                           : effects.frame().models.size() == ((streams || objects) ? 0u : 1u));
            if (objects)
                CHECK(ballistic.runtime().object_particles.started == 4 &&
                      ballistic.runtime().object_particles.particle_count() > 0);
            if (streams)
                CHECK(ballistic.runtime().particle_streams.started == 2 &&
                      ballistic.runtime().particle_streams.active[0].effect.emitted >= 12 &&
                      ballistic.runtime().particle_streams.active[1].effect.emitted > 0);
            CHECK(update_model_scene(input, scene, error) &&
                  scene.world_draws.size() == ((streams || objects) ? 0u : 1u));
            CHECK(update_explosion_model_scene(input, explosions, error) &&
                  (instants ? explosions.world_draws.size() >= 11 : explosions.world_draws.empty()));
            CHECK(update_sprite_scene(input, sprite, error) && sprite.entries.size() >= 2);
            if (impacts)
            {
                CHECK(sprite.entries.size() >= 7);
                for (const auto count : std::span(ballistic.received_kinds).first(5))
                    CHECK(count == 1);
                if (instants)
                {
                    for (unsigned kind = 5; kind < 10; ++kind)
                        CHECK(ballistic.received_kinds[kind] == (kind == 5 ? 6u : 1u));
                    CHECK(sprite.entries.size() >= 100 && !effects.frame().lights.empty());
                }
            }
            return true;
        };
#ifndef BLOOD2_SCENE_GPU
        if (ballistics)
            CHECK(add_ballistic());
#endif
#ifdef BLOOD2_SCENE_GPU
        CHECK(SDL_Init(SDL_INIT_VIDEO));
        auto* gpu = SDL_CreateGPUDevice(WorldShaderFormats, true, nullptr);
        CHECK(gpu);
        std::cout << "GPU="
                  << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu), SDL_PROP_GPU_DEVICE_NAME_STRING, "?")
                  << '\n';
        bool ok = [&]()
        {
            Capture capture;
            CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 640, 360));
            ScenePresenter presenter(gpu);
            presenter.post = {Antialiasing::Off, false, Tonemap::Off, AmbientOcclusion::Off, Reflections::Off};
            std::vector<Uint8> original, enhanced, again;
            CHECK(render(gpu, presenter, input, capture, original, error));
            if (ballistics)
            {
                const auto baseline = original;
                CHECK(add_ballistic());
                CHECK(render(gpu, presenter, input, capture, original, error));
                std::size_t changed = 0;
                for (std::size_t i = 0; i < original.size(); i += 4)
                    changed += !std::equal(original.begin() + i, original.begin() + i + 3, baseline.begin() + i);
                CHECK(changed > 20 && capture.save(objects    ? "network-object-original.png"
                                                   : streams  ? "network-stream-original.png"
                                                   : instants ? "network-instant-original.png"
                                                   : impacts  ? "network-impact-original.png"
                                                              : "network-ballistic-original.png"));
                std::cout << "Independent decoded "
                          << (objects    ? "owner-bound ObjectFX"
                              : streams  ? "persistent streams/systems"
                              : instants ? "all instant families"
                              : impacts  ? "mixed impact"
                                         : "ballistic")
                          << " rendering: changed pixels=" << changed << '\n';
            }
            CHECK(!presenter.rebuilt || presenter.mesh_revision == input.world_revision);
            const auto count = presenter.registry.stats.textures;
            presenter.shading = SurfaceShading::Standard;
            CHECK(render(gpu, presenter, input, capture, enhanced, error));
            if (ballistics)
                CHECK(capture.save(objects    ? "network-object-enhanced.png"
                                   : streams  ? "network-stream-enhanced.png"
                                   : instants ? "network-instant-enhanced.png"
                                   : impacts  ? "network-impact-enhanced.png"
                                              : "network-ballistic-enhanced.png"));
            presenter.shading = SurfaceShading::Original;
            CHECK(render(gpu, presenter, input, capture, again, error));
            CHECK(original == again && original != enhanced && count > 0);
            auto* stale_commands = SDL_AcquireGPUCommandBuffer(gpu);
            CHECK(stale_commands);
            const auto rendered_frames = presenter.stats.frames;
            CHECK(!presenter.record(wrong_world, stale_commands, capture.texture, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                    640, 360, error));
            CHECK(SDL_CancelGPUCommandBuffer(stale_commands) && presenter.stats.frames == rendered_frames);
            CHECK(!presenter.update(wrong_world, error));
            CHECK(presenter.update(input, error) && !presenter.rebuilt);
            std::cout << "Independent GPU scene: " << presenter.world_draws.size() << " world draws, "
                      << presenter.model_scene.close_draws.size()
                      << " view weapons, Original/Enhanced/Original stable\n";
            return true;
        }();
        SDL_DestroyGPUDevice(gpu);
        SDL_Quit();
        CHECK(ok);
#endif
        std::cout << "Independent scene inputs: wire replica/local content/weapon pose/lighting, invalid flash and "
                     "world identity checked\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const bool objects = argc == 3 && std::string_view(argv[2]) == "--objects";
    const bool streams = argc == 3 && std::string_view(argv[2]) == "--streams";
    const bool instants = argc == 3 && std::string_view(argv[2]) == "--instants";
    const bool impacts = instants || (argc == 3 && std::string_view(argv[2]) == "--impacts");
    return (argc == 2 ||
            (argc == 3 && (objects || streams || impacts || std::string_view(argv[2]) == "--ballistics"))) &&
                   run(argv[1], argc == 3, impacts, instants, streams, objects)
               ? 0
               : 1;
}
