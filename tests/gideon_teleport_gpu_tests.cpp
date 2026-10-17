#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "gideon_fixture.h"
#include <cmath>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << " " << error << " " << SDL_GetError() << "\n";                         \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    DVector sub(DVector a, DVector b)
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    DVector cross(DVector a, DVector b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    bool render(Runtime& runtime, WorldRenderer& renderer, ModelView& scene, DVector eye, DRotation camera,
                Capture& capture, std::vector<std::uint8_t>& pixels, std::string& error)
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        if (!renderer.draw(commands, capture.texture, scene.mesh, scene.world_draws, eye, camera, false, error))
        {
            SDL_SubmitGPUCommandBuffer(commands);
            return false;
        }
        if (scene.mesh.blended && !renderer.draw(commands, capture.texture, scene.mesh, scene.world_draws, eye, camera,
                                                 false, error, DrawLoad::Load, nullptr, WorldPass::Translucent))
        {
            SDL_SubmitGPUCommandBuffer(commands);
            return false;
        }
        auto* copy = SDL_BeginGPUCopyPass(commands);
        if (!copy)
        {
            SDL_SubmitGPUCommandBuffer(commands);
            return false;
        }
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 400;
        region.h = 300;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 400;
        target.rows_per_layer = 300;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(runtime.gpu))
            return false;
        auto* data = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        if (!data)
            return false;
        pixels.assign(data, data + 400 * 300 * 4);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    Runtime runtime;
    std::string error;
    CHECK(runtime.initialize(true, error));
    WorldRenderer renderer(runtime.gpu);
    GameSession g;
    CHECK(g.initialize());
    auto floor = test_brush::box("distant floor", {-10000, -2000, -10000}, {10000, -1990, 10000});
    for (auto& surface : floor.surfaces)
        surface.flags = 1;
    CHECK(g.collision.build(floor, error));
    g.server.collision_ready = true;
    std::filesystem::create_directories(argv[1]);
    if (argc == 3)
    {
        Config c;
        c.data_dir = argv[2];
        CHECK(g.resources.mount_game(c, error));
    }
    else
        CHECK(test_shield::mount(g.resources, argv[1], error));
    auto* a = test_gideon::actor(g, argc == 3, {0, 0, 0});
    CHECK(a);
    auto* record = g.objects.get(a->m_hObject);
    ModelView view;
    CHECK(update_model_scene(g, view, error));
    CHECK(view.entries.size() == 1 && view.world_draws.size() == 1);
    DVector eye{0, 120, -600};
    DRotation camera;
    if (argc == 2)
    {
        const auto va = view.mesh.vertices[0].position, vb = view.mesh.vertices[1].position,
                   vc = view.mesh.vertices[2].position;
        auto n = cross(sub(vb, va), sub(vc, va));
        float l = std::hypot(n.x, n.y, n.z);
        CHECK(l > 0);
        n = {n.x / l, n.y / l, n.z / l};
        CHECK(align_rotation(n, camera));
        eye = {(va.x + vb.x + vc.x) / 3 - n.x * 10, (va.y + vb.y + vc.y) / 3 - n.y * 10,
               (va.z + vb.z + vc.z) / 3 - n.z * 10};
    }
    Capture capture;
    std::vector<std::uint8_t> full, half, zero, again;
    CHECK(render(runtime, renderer, view, eye, camera, capture, full, error));
    CHECK(a->MC_FadeOut());
    for (int i = 0; i < 8; ++i)
        CHECK(a->MC_FadeOut());
    CHECK(record->alpha == .5f && update_model_scene(g, view, error) && view.mesh.blended == 1);
    auto clock = record->animation.time;
    CHECK(render(runtime, renderer, view, eye, camera, capture, half, error));
    CHECK(capture.save((std::filesystem::path(argv[1]) / "gideon-half.png").c_str()));
    CHECK(update_model_scene(g, view, error) && render(runtime, renderer, view, eye, camera, capture, again, error));
    CHECK(half == again && record->animation.time == clock && a->fade_calls == 9);
    unsigned ink = 0, changed = 0;
    const int bg[]{6, 9, 13};
    for (unsigned i = 0; i < 400 * 300; ++i)
    {
        bool nonempty = false;
        for (unsigned c = 0; c < 3; ++c)
        {
            const auto j = i * 4 + c;
            nonempty |= full[j] != bg[c];
            changed += half[j] != full[j];
            if (argc == 2)
                CHECK(std::abs(int(half[j]) - int((full[j] + bg[c]) / 2)) <= 1);
        }
        ink += nonempty;
    }
    CHECK(ink > 50 && ink < 100000 && changed > 50);
    for (int i = 0; i < 8; ++i)
        CHECK(a->MC_FadeOut());
    CHECK(record->alpha == 0 && update_model_scene(g, view, error));
    CHECK(render(runtime, renderer, view, eye, camera, capture, zero, error));
    for (unsigned i = 0; i < 400 * 300; ++i)
        for (unsigned c = 0; c < 3; ++c)
            CHECK(zero[i * 4 + c] == bg[c]);
    a->Metacmd = 4;
    CHECK(a->AI_STATE_Teleport() && update_model_scene(g, view, error) && view.mesh.blended == 0);
    CHECK(render(runtime, renderer, view, eye, camera, capture, again, error) && again == full);
    // Original dodge uses server time and cached lateral velocity, independently
    // of both the inspector and rendering. Exercise the actual posed model too.
    a->m_fRollSpeed = 3.5f;
    const auto start = record->state.m_Pos;
    CHECK(a->MC_Dodge_Right() && g.step(.5) && a->MC_Dodge_Right());
    CHECK(std::abs(record->state.m_Pos.x - start.x - 1.75f) < .003f && a->m_bAnimating);
    CHECK(update_model_scene(g, view, error) && render(runtime, renderer, view, eye, camera, capture, again, error));
    CHECK(again != full && capture.save((std::filesystem::path(argv[1]) / "gideon-dodge.png").c_str()));
    clock = record->animation.time;
    const auto calls = a->dodge_calls;
    CHECK(update_model_scene(g, view, error) && render(runtime, renderer, view, eye, camera, capture, half, error));
    CHECK(again == half && record->animation.time == clock && a->dodge_calls == calls);
    CHECK(g.step(.5) && a->MC_Dodge_Right() && !a->m_bAnimating && a->dodge_finishes == 1);
    CHECK(std::abs(record->state.m_Pos.x - start.x - 3.5f) < .003f && record->velocity.x == 3.5f);
    CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame() && update_model_scene(g, view, error));
    CHECK(view.entries.empty() && render(runtime, renderer, view, eye, camera, capture, again, error) && again == zero);
    CHECK(g.unload_world());
    view.clear();
    renderer.clear_geometry();
    std::cout << (argc == 3 ? "External Gideon" : "Synthetic triangle") << ": " << ink
              << " visible pixels; original 16-step fade reaches empty frame; reappearance exact; repeated reads do "
                 "not advance clocks. Dodge motion integrates once and finishes at one second.\n";
    return 0;
}
