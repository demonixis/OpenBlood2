#include "app/menu_renderer.h"
#include <imgui.h>
#include <cstring>
namespace OpenBlood2
{
    MenuRenderer::~MenuRenderer()
    {
        if (texture_ || transfer_) SDL_WaitForGPUIdle(device_);
        if (texture_) SDL_ReleaseGPUTexture(device_, texture_);
        if (transfer_) SDL_ReleaseGPUTransferBuffer(device_, transfer_);
    }
    bool MenuRenderer::draw(MenuCanvas& canvas, std::string& error)
    {
        if (!canvas.surface) { error="Menu has no composed surface"; return false; }
        if (!texture_)
        {
            SDL_GPUTextureCreateInfo info{};
            info.type=SDL_GPU_TEXTURETYPE_2D;
            info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width=640; info.height=480; info.layer_count_or_depth=1; info.num_levels=1;
            texture_=SDL_CreateGPUTexture(device_, &info);
        }
        if (!transfer_)
        {
            SDL_GPUTransferBufferCreateInfo info{};
            info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            info.size=640*480*4;
            transfer_=SDL_CreateGPUTransferBuffer(device_, &info);
        }
        if (!texture_ || !transfer_) { error=SDL_GetError(); return false; }
        if (uploaded_ != canvas.revision)
        {
            auto* bytes=SDL_MapGPUTransferBuffer(device_, transfer_, true);
            if (!bytes) { error=SDL_GetError(); return false; }
            std::memcpy(bytes, canvas.surface->rgba.data(), 640*480*4);
            SDL_UnmapGPUTransferBuffer(device_, transfer_);
            auto* commands=SDL_AcquireGPUCommandBuffer(device_);
            if (!commands) { error=SDL_GetError(); return false; }
            auto* pass=SDL_BeginGPUCopyPass(commands);
            if (!pass) { error=SDL_GetError(); SDL_CancelGPUCommandBuffer(commands); return false; }
            SDL_GPUTextureTransferInfo source{};
            source.transfer_buffer=transfer_; source.pixels_per_row=640; source.rows_per_layer=480;
            SDL_GPUTextureRegion dest{};
            dest.texture=texture_; dest.w=640; dest.h=480; dest.d=1;
            SDL_UploadToGPUTexture(pass, &source, &dest, true);
            SDL_EndGPUCopyPass(pass);
            if (!SDL_SubmitGPUCommandBuffer(commands)) { error=SDL_GetError(); return false; }
            uploaded_=canvas.revision;
        }
        const auto* viewport=ImGui::GetMainViewport();
        const auto fit=MenuViewport::fit(viewport->Size.x, viewport->Size.y);
        auto* draw=ImGui::GetBackgroundDrawList();
        draw->AddRectFilled(viewport->Pos, {viewport->Pos.x+viewport->Size.x, viewport->Pos.y+viewport->Size.y}, IM_COL32(0,0,0,255));
        const ImVec2 p{viewport->Pos.x+fit.x, viewport->Pos.y+fit.y};
        draw->AddImage((ImTextureID)(intptr_t)texture_, p, {p.x+640*fit.scale,p.y+480*fit.scale});
        return true;
    }
}
