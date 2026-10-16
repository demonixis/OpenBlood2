#pragma once
#include "engine/world_view.h"
#include <SDL3/SDL_gpu.h>

namespace OpenBlood2
{
    class WorldRenderer
    {
      public:
        explicit WorldRenderer(SDL_GPUDevice* device) : device_(device) {}
        ~WorldRenderer();
        WorldRenderer(const WorldRenderer&) = delete;
        WorldRenderer& operator=(const WorldRenderer&) = delete;
        bool prepare(const WorldMesh&, std::uint64_t revision, SDL_GPUTextureFormat, Uint32 width, Uint32 height,
                     std::string& error);
        bool draw(SDL_GPUCommandBuffer*, SDL_GPUTexture*, const WorldMesh&, std::span<const WorldDraw>, DVector eye,
                  DRotation view, bool letterbox, std::string& error);
        void clear_geometry();
        WorldViewStats stats;
        Uint32 width() const
        {
            return width_;
        }
        Uint32 height() const
        {
            return height_;
        }

      private:
        SDL_GPUDevice* device_ = nullptr;
        SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
        SDL_GPUBuffer* vertices_ = nullptr;
        SDL_GPUTexture* depth_ = nullptr;
        std::vector<SDL_GPUTexture*> textures_;
        SDL_GPUSampler* sampler_ = nullptr;
        bool upload_materials(const WorldMesh&, std::string&);
        SDL_GPUTextureFormat color_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
        Uint32 width_ = 0, height_ = 0;
        std::uint64_t revision_ = UINT64_MAX;
        bool pipeline(SDL_GPUTextureFormat, std::string&);
    };
} // namespace OpenBlood2
