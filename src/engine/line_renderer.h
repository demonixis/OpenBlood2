#pragma once

#include "engine/xr/xr_math.h"
#include <SDL3/SDL_gpu.h>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2
{
    // Unlit coloured line lists (ballistic tracers and XR overlays) with the
    // asymmetric line shaders. Colours are gamma-encoded like the scene's.
    struct LineVertex
    {
        std::array<float, 3> position{};
        std::array<float, 4> color{};
    };
    static_assert(sizeof(LineVertex) == 28);

    void add_line(std::vector<LineVertex>&, std::array<float, 3> a, std::array<float, 3> b, std::array<float, 4> color);
    // The twelve edges of a box: half extents `scale` around `pose` (all in the same units).
    void add_box(std::vector<LineVertex>&, const xr::Pose& pose, std::array<float, 3> scale,
                 std::array<float, 4> color);

    class LineRenderer
    {
      public:
        explicit LineRenderer(SDL_GPUDevice*);
        LineRenderer(const LineRenderer&) = delete;
        LineRenderer& operator=(const LineRenderer&) = delete;
        ~LineRenderer();
        // (Re)creates the pipeline for these target formats.
        bool prepare(SDL_GPUTextureFormat color, SDL_GPUTextureFormat depth, std::string& error,
                     SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1, unsigned targets = 1, bool blend = false);
        // Copies the vertices into the GPU buffer in a copy pass of `commands`, before its render
        // passes; earlier frames still in flight keep their data (cycled buffers).
        bool upload(SDL_GPUCommandBuffer*, std::span<const LineVertex>, std::string& error);
        // Draws uploaded vertices [first, first + count) inside an already begun render pass.
        void draw(SDL_GPURenderPass*, SDL_GPUCommandBuffer*, const xr::LineUniform&, Uint32 width, Uint32 height,
                  float min_depth, float max_depth, std::uint32_t first, std::uint32_t count, Uint32 top = 0) const;
        std::uint32_t uploaded() const
        {
            return uploaded_;
        }

      private:
        SDL_GPUDevice* device_ = nullptr;
        SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
        SDL_GPUTextureFormat color_ = SDL_GPU_TEXTUREFORMAT_INVALID, depth_ = SDL_GPU_TEXTUREFORMAT_INVALID;
        SDL_GPUSampleCount samples_ = SDL_GPU_SAMPLECOUNT_1;
        unsigned targets_ = 1;
        bool blend_ = false;
        SDL_GPUBuffer* buffer_ = nullptr;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        std::uint32_t capacity_ = 0, uploaded_ = 0;
    };
} // namespace OpenBlood2
