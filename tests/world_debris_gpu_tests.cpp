#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_model.h"
#include "engine/world_renderer.h"
#include "engine/world_view.h"
#include "game/original/WorldModelDebris.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <iostream>
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
    bool render(Runtime& runtime, WorldRenderer& renderer, const WorldMesh& mesh, std::span<const WorldDraw> draws,
                const ObjectRecord& object, const std::string& label, bool visible)
    {
        Capture capture;
        std::string error;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240));
        const auto& shape = *object.world_model;
        const auto center = object_bounds_center(object);
        const auto extent = vector_subtract(shape.max, shape.min);
        const auto eye = vector_subtract(center, {0, 0, 1.5f * std::max({extent.x, extent.y, extent.z}) + 64});
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd && renderer.draw(cmd, capture.texture, mesh, draws, eye, {}, false, error));
        CHECK(renderer.draw(cmd, capture.texture, mesh, draws, eye, {}, false, error, DrawLoad::Load, nullptr,
                            WorldPass::Translucent));
        auto* copy = SDL_BeginGPUCopyPass(cmd);
        CHECK(copy);
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 320;
        region.h = 240;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 320;
        target.rows_per_layer = 240;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(cmd) && SDL_WaitForGPUIdle(runtime.gpu));
        const auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        unsigned changed = 0;
        for (unsigned i = 0; i < 320 * 240; ++i)
            changed += pixels[i * 4] != 6 || pixels[i * 4 + 1] != 9 || pixels[i * 4 + 2] != 13;
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        if (visible)
            CHECK(capture.save((label + ".png").c_str()));
        std::cout << label << ": " << changed << " non-background pixels\n";
        CHECK(visible ? changed > 100 : changed == 0);
        return true;
    }

    bool run(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        Runtime runtime;
        CHECK(runtime.initialize(true, error));
        WorldRenderer renderer(runtime.gpu);
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", error));
            WorldMesh mesh;
            CHECK(mesh.build(g.world, error, &g.resources));
            CHECK(renderer.prepare(mesh, g.world_revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
            HOBJECT piece = nullptr, parent = nullptr;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                if (std::string_view(r->state.m_Name) == "billboarddebris0")
                    piece = r->handle;
                if (std::string_view(r->state.m_Name) == "billboard")
                    parent = r->handle;
            }
            auto* r = g.objects.get(piece);
            CHECK(r && parent && dynamic_cast<WorldModelDebris*>(r->game.get()));
            auto draw = [&](const char* suffix, bool visible)
            {
                std::vector<WorldDraw> all, selected;
                WorldViewStats stats;
                if (!world_draws(mesh, g.objects, all, stats, error))
                    return false;
                for (auto& d : all)
                    if (mesh.ranges[d.range].name == "billboarddebris0")
                        selected.push_back(d);
                return render(runtime, renderer, mesh, selected, *r, "world-debris-" + std::to_string(cycle) + suffix,
                              visible);
            };
            CHECK(draw("-hidden", false));
            auto m = g.server.StartMessageToObject(g.server.HandleToObject(parent), parent, damage_message);
            CHECK(m && g.server.WriteDamage(m, {{}, {}, 100, DamageType::Death, parent}) == DE_OK &&
                  g.server.EndMessage(m) == DE_OK);
            CHECK(g.step(.01));
            CHECK(draw("-started", true));
            auto original = r->state.m_Pos;
            for (int n = 0; n < 30; ++n)
                CHECK(g.step(.01));
            CHECK(r->state.m_Pos.y < original.y);
            CHECK(draw("-moved", true));
            CHECK(g.objects.remove(piece));
            CHECK(draw("-removed", false));
            CHECK(g.objects.finish_frame());
            renderer.clear_geometry();
            CHECK(g.unload_world() && g.server.list_count() == 0 && g.server.string_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc != 2 ? 2 : run(path_from_utf8(argv[1])) ? 0 : 1;
}
