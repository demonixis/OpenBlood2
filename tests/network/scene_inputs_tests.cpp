#include "app/model_lighting.h"
#include "app/model_scene.h"
#include "app/polygrid_scene.h"
#include "app/sprite_scene.h"
#include "client/network_world.h"
#include "game/original/WeaponDefs.h"
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
    bool run(const char* data)
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
        object.pose.position = eye;
        values.objects.push_back(object);
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
#ifdef BLOOD2_SCENE_GPU
        CHECK(SDL_Init(SDL_INIT_VIDEO));
        auto* gpu = SDL_CreateGPUDevice(WorldShaderFormats, true, nullptr);
        CHECK(gpu);
        bool ok = [&]()
        {
            Capture capture;
            CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 640, 360));
            ScenePresenter presenter(gpu);
            presenter.post = {Antialiasing::Off, false, Tonemap::Off, AmbientOcclusion::Off, Reflections::Off};
            std::vector<Uint8> original, enhanced, again;
            CHECK(render(gpu, presenter, input, capture, original, error));
            CHECK(!presenter.rebuilt || presenter.mesh_revision == input.world_revision);
            const auto count = presenter.registry.stats.textures;
            presenter.shading = SurfaceShading::Standard;
            CHECK(render(gpu, presenter, input, capture, enhanced, error));
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
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
