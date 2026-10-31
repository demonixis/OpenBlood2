#include "app/scoreboard.h"
#include <cmath>
#include <cstring>
#include <imgui.h>
namespace OpenBlood2
{
    ScoreboardRenderer::~ScoreboardRenderer()
    {
        if (texture_ || transfer_)
            SDL_WaitForGPUIdle(device_);
        if (texture_)
            SDL_ReleaseGPUTexture(device_, texture_);
        if (transfer_)
            SDL_ReleaseGPUTransferBuffer(device_, transfer_);
    }
    bool ScoreboardRenderer::draw(const ClientFragTable& table, std::string& error)
    {
        if (!table.displayed)
            return true;
        const auto& image = table.surface();
        // Team columns and small-screen switching can change the required width.
        // Finish previous submitted draws before releasing their owned texture.
        if (texture_ && (image.width != width_ || image.height > height_))
        {
            SDL_WaitForGPUIdle(device_);
            SDL_ReleaseGPUTexture(device_, texture_);
            if (transfer_)
                SDL_ReleaseGPUTransferBuffer(device_, transfer_);
            texture_ = nullptr;
            transfer_ = nullptr;
            revision_ = UINT64_MAX;
        }
        if (!texture_)
        {
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            width_ = image.width;
            height_ = image.height; // CPU composition bounds storage by supported rows, not viewport height.
            info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = width_;
            info.height = height_;
            info.layer_count_or_depth = info.num_levels = 1;
            texture_ = SDL_CreateGPUTexture(device_, &info);
        }
        if (!transfer_)
        {
            SDL_GPUTransferBufferCreateInfo info{};
            info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            info.size = width_ * height_ * 4;
            transfer_ = SDL_CreateGPUTransferBuffer(device_, &info);
        }
        if (!texture_ || !transfer_)
        {
            error = SDL_GetError();
            return false;
        }
        if (revision_ != table.revision)
        {
            auto* bytes = SDL_MapGPUTransferBuffer(device_, transfer_, true);
            if (!bytes)
            {
                error = SDL_GetError();
                return false;
            }
            std::memcpy(bytes, image.rgba.data(), image.rgba.size());
            SDL_UnmapGPUTransferBuffer(device_, transfer_);
            auto* commands = SDL_AcquireGPUCommandBuffer(device_);
            if (!commands)
            {
                error = SDL_GetError();
                return false;
            }
            auto* pass = SDL_BeginGPUCopyPass(commands);
            if (!pass)
            {
                SDL_CancelGPUCommandBuffer(commands);
                error = SDL_GetError();
                return false;
            }
            SDL_GPUTextureTransferInfo source{};
            source.transfer_buffer = transfer_;
            source.pixels_per_row = image.width;
            source.rows_per_layer = image.height;
            SDL_GPUTextureRegion destination{};
            destination.texture = texture_;
            destination.w = image.width;
            destination.h = image.height;
            destination.d = 1;
            SDL_UploadToGPUTexture(pass, &source, &destination, true);
            SDL_EndGPUCopyPass(pass);
            if (!SDL_SubmitGPUCommandBuffer(commands))
            {
                error = SDL_GetError();
                return false;
            }
            revision_ = table.revision;
        }
        const auto origin = ImGui::GetMainViewport()->Pos;
        const auto density = ImGui::GetIO().DisplayFramebufferScale;
        if (!std::isfinite(density.x) || !std::isfinite(density.y) || density.x <= 0 || density.y <= 0)
        {
            error = "Invalid scoreboard framebuffer density";
            return false;
        }
        const ImVec2 min{origin.x + table.x / density.x, origin.y + table.y / density.y};
        const ImVec2 max{min.x + image.width / density.x, min.y + image.height / density.y};
        ImGui::GetForegroundDrawList()->AddImage((ImTextureID)(intptr_t)texture_, min, max, {0, 0},
                                                 {float(image.width) / width_, float(image.height) / height_});
        return true;
    }
} // namespace OpenBlood2
