#include "engine/capture.h"

namespace blood2 {

bool Capture::initialize(SDL_GPUDevice* gpu, SDL_GPUTextureFormat format, Uint32 w, Uint32 h) {
    device = gpu;
    width = w;
    height = h;
    if (w == 0 || h == 0 || w > 8192 || h > 8192) return SDL_SetError("Capture dimensions must be within 8192x8192");
    if (format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) pixel_format = SDL_PIXELFORMAT_RGBA32;
    else if (format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) pixel_format = SDL_PIXELFORMAT_BGRA32;
    else return SDL_SetError("Capture requires an SDR RGBA8/BGRA8 swapchain");
    SDL_GPUTextureCreateInfo image{};
    image.type = SDL_GPU_TEXTURETYPE_2D;
    image.format = format;
    image.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    image.width = w;
    image.height = h;
    image.layer_count_or_depth = 1;
    image.num_levels = 1;
    image.sample_count = SDL_GPU_SAMPLECOUNT_1;
    texture = SDL_CreateGPUTexture(gpu, &image);
    if (!texture) return false;
    SDL_GPUTransferBufferCreateInfo buffer{};
    buffer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    buffer.size = w * h * 4;
    transfer = SDL_CreateGPUTransferBuffer(gpu, &buffer);
    return transfer != nullptr;
}

bool Capture::enqueue(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain) {
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    if (!copy) return false;
    SDL_GPUTextureRegion source{};
    source.texture = texture;
    source.w = width;
    source.h = height;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = transfer;
    destination.pixels_per_row = width;
    destination.rows_per_layer = height;
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUBlitInfo blit{};
    blit.source.texture = texture;
    blit.source.w = width;
    blit.source.h = height;
    blit.destination.texture = swapchain;
    blit.destination.w = width;
    blit.destination.h = height;
    blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
    blit.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(commands, &blit);
    return true;
}

bool Capture::save(const char* filename) {
    if (!SDL_WaitForGPUIdle(device)) return false;
    void* pixels = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (!pixels) return false;
    SDL_Surface* surface = SDL_CreateSurfaceFrom(static_cast<int>(width), static_cast<int>(height),
                                                pixel_format, pixels, static_cast<int>(width * 4));
    const bool saved = surface && SDL_SavePNG(surface, filename);
    SDL_DestroySurface(surface);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    return saved;
}

Capture::~Capture() {
    if (transfer) SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (texture) SDL_ReleaseGPUTexture(device, texture);
}

}
