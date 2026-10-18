// GPU registry ownership: generational handles, content-addressed sharing, incremental
// material refresh, owner-scoped shared assets and staged flushes on a real device.
#include "engine/gpu_registry.h"
#include "engine/world_shaders.h"
#include <SDL3/SDL.h>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << " " << SDL_GetError() << '\n';                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    WorldMaterial material(std::string path, std::uint8_t red)
    {
        return {std::move(path), "", {1, 1, 1, 0, 0, {red, 0, 0, 255}, {}}};
    }
} // namespace
int main()
{
    std::string error;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    auto* device = SDL_CreateGPUDevice(WorldShaderFormats, true, nullptr);
    CHECK(device);
    {
        GpuRegistry registry(device);
        // Equal key and pixels share one texture; equal key with other pixels does not.
        std::vector<WorldMaterial> first{material("white", 255), material("a.dtx", 10)};
        std::vector<WorldMaterial> second{material("white", 255), material("a.dtx", 11)};
        std::vector<TextureHandle> a, b;
        CHECK(registry.textures(first, a, error) && a.size() == 2 && registry.get(a[0]) && registry.get(a[1]));
        CHECK(registry.textures(second, b, error) && b[0] == a[0] && !(b[1] == a[1]));
        CHECK(registry.stats.textures == 3 && registry.stats.texture_hits == 1 && registry.stats.texture_uploads == 3);
        // Failed and animated entries get null handles; an empty list gets white.
        auto failed = material("missing.dtx", 0);
        failed.error = "Missing";
        auto animated = material("anim.spr", 0);
        animated.frames = {1};
        std::vector<WorldMaterial> mixed{material("white", 255), failed, animated};
        CHECK(registry.textures(mixed, b, error) && b.size() == 3 && registry.get(b[0]) && !b[1] && !b[2]);
        CHECK(registry.textures({}, b, error) && b.size() == 1 && registry.get(b[0]));
        // Invalid payloads fail without handles.
        auto broken = material("broken.dtx", 0);
        broken.image.rgba.pop_back();
        std::vector<WorldMaterial> invalid{broken};
        CHECK(!registry.textures(invalid, b, error) && b.empty());
        error.clear();
        // Appended materials keep their prefix handles and acquire only the tail.
        std::vector<TextureHandle> handles;
        std::vector<std::string> paths;
        CHECK(registry.refresh_textures(first, handles, paths, error) && handles == a && paths.size() == 2);
        const auto uploads = registry.stats.texture_uploads;
        first.push_back(material("b.dtx", 20));
        CHECK(registry.refresh_textures(first, handles, paths, error) && handles.size() == 3);
        CHECK(handles[0] == a[0] && handles[1] == a[1] && registry.stats.texture_uploads == uploads + 1);
        CHECK(registry.flush(nullptr, error) && registry.stats.staged_bytes > 0);
        CHECK(registry.flush(nullptr, error) && registry.stats.staged_bytes == 0);
        // A world revision boundary invalidates every content handle, never reusing it silently.
        const auto stale = handles[1];
        registry.clear_textures();
        CHECK(!registry.get(stale) && registry.stats.textures == 0 && registry.stats.texture_bytes == 0);
        CHECK(registry.refresh_textures(first, handles, paths, error) && registry.get(handles[1]) &&
              !(handles[1] == stale));
        // Buffers: growth replaces the handle; released handles stay stale after slot reuse.
        std::string ignored;
        auto buffer = registry.create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, 64, error);
        CHECK(buffer && registry.size(buffer) == 64);
        auto old = buffer;
        CHECK(registry.reserve(buffer, SDL_GPU_BUFFERUSAGE_VERTEX, 32, error) && buffer == old);
        CHECK(registry.reserve(buffer, SDL_GPU_BUFFERUSAGE_VERTEX, 256, error) && !(buffer == old) &&
              !registry.get(old) && registry.size(buffer) >= 256);
        const std::array<std::uint8_t, 256> bytes{};
        CHECK(registry.stage(buffer, bytes.data(), 256, true, error));
        CHECK(!registry.stage(buffer, bytes.data(), 512, true, ignored) &&
              !registry.stage(old, bytes.data(), 4, true, ignored));
        auto reused = registry.create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, 16, error);
        CHECK(reused.index == old.index && !(reused == old) && !registry.get(old));
        CHECK(registry.flush(nullptr, error));
        registry.release(reused);
        CHECK(!reused && registry.stats.buffers == 1);
        // Shared assets are keyed by owner identity and pruned when the owner expires.
        auto owner = std::make_shared<int>(1);
        const void* key = owner.get();
        auto asset = registry.create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, 48, error);
        CHECK(registry.share(key, std::weak_ptr<const void>(owner), {asset, 3}));
        CHECK(registry.shared(key, std::weak_ptr<const void>(owner)) &&
              registry.shared(key, std::weak_ptr<const void>(owner))->count == 3);
        auto impostor = std::make_shared<int>(2);
        CHECK(!registry.shared(key, std::weak_ptr<const void>(impostor)));
        owner.reset();
        CHECK(registry.flush(nullptr, error) && registry.stats.shared == 0 && !registry.get(asset));
        // Explicit targets survive world clears.
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        info.width = info.height = 8;
        info.layer_count_or_depth = info.num_levels = 1;
        auto depth = registry.create_texture(info, error);
        registry.clear_textures();
        CHECK(registry.get(depth) && registry.stats.targets == 1);
        registry.release(depth);
        CHECK(!depth && registry.stats.targets == 0);
        CHECK(registry.sampler(false, error) && registry.sampler(true, error) &&
              registry.sampler(false, error) != registry.sampler(true, error));
    }
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    std::cout << "GPU registry handles, content sharing, incremental refresh, shared assets and flushes pass\n";
    return 0;
}
