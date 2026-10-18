#include "app/hud.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
namespace OpenBlood2
{
    HudState hud_state(const PlayerStats& s, bool in_world, bool external_camera)
    {
        HudState h;
        h.stats = s;
        if (!in_world || external_camera)
            return h;
        h.visible = weapon_definition(s.weapon) && std::isfinite(s.health) && std::isfinite(s.armor) &&
                    std::isfinite(s.ammo) && std::isfinite(s.alternate_ammo) && std::isfinite(s.max_health) &&
                    std::isfinite(s.max_armor);
        return h;
    }
    bool draw_screen_fade(float scale, bool in_world)
    {
        if (!in_world || !std::isfinite(scale) || scale < 0 || scale >= 1)
            return false;
        const auto* viewport = ImGui::GetMainViewport();
        const auto alpha = static_cast<int>(std::lround((1.f - scale) * 255.f));
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            viewport->Pos, {viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y},
            IM_COL32(0, 0, 0, alpha));
        return true;
    }
    HudRenderer::~HudRenderer()
    {
        if (device_)
            SDL_WaitForGPUIdle(device_);
        if (texture_)
            SDL_ReleaseGPUTexture(device_, texture_);
        if (transfer_)
            SDL_ReleaseGPUTransferBuffer(device_, transfer_);
    }
    bool HudRenderer::upload(std::string& error)
    {
        constexpr unsigned width = 655, height = 75;
        if (!texture_)
        {
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = width;
            info.height = height;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            info.sample_count = SDL_GPU_SAMPLECOUNT_1;
            texture_ = SDL_CreateGPUTexture(device_, &info);
        }
        if (!transfer_)
        {
            SDL_GPUTransferBufferCreateInfo info{};
            info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            info.size = width * height * 4;
            transfer_ = SDL_CreateGPUTransferBuffer(device_, &info);
        }
        if (!texture_ || !transfer_)
        {
            error = SDL_GetError();
            return false;
        }
        auto* data = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device_, transfer_, true));
        if (!data)
        {
            error = SDL_GetError();
            return false;
        }
        std::fill_n(data, width * height * 4, 0);
        unsigned offset = 0;
        for (const auto* tab : status_->tabs())
        {
            const auto& surface = *tab->hTab;
            if (surface.width > width - offset || surface.height > height ||
                surface.rgba.size() != std::size_t(surface.width) * surface.height * 4)
            {
                SDL_UnmapGPUTransferBuffer(device_, transfer_);
                error = "Original HUD surface exceeds atlas bounds";
                return false;
            }
            for (unsigned y = 0; y < surface.height; ++y)
                for (unsigned x = 0; x < surface.width; ++x)
                {
                    const auto s = (y * surface.width + x) * 4, d = (y * width + offset + x) * 4;
                    std::copy_n(surface.rgba.data() + s, 4, data + d);
                    if (data[d] == 255 && data[d + 1] == 0 && data[d + 2] == 255)
                        data[d + 3] = 0;
                }
            offset += surface.width;
        }
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
        source.pixels_per_row = width;
        source.rows_per_layer = height;
        SDL_GPUTextureRegion destination{};
        destination.texture = texture_;
        destination.w = width;
        destination.h = height;
        destination.d = 1;
        SDL_UploadToGPUTexture(pass, &source, &destination, true);
        SDL_EndGPUCopyPass(pass);
        if (!SDL_SubmitGPUCommandBuffer(commands))
        {
            error = SDL_GetError();
            return false;
        }
        uploaded_revision_ = status_->revision;
        return true;
    }
    bool HudRenderer::draw(const HudState& h, const Resources& resources, std::string& error)
    {
        error.clear();
        if (!h.visible)
            return true;
        const auto* viewport = ImGui::GetMainViewport();
        if (viewport->Size.x < 135 || viewport->Size.y < 100)
            return true;
        if (!status_ || resources_ != &resources)
        {
            status_ = std::make_unique<CNewStatusBar>(resources);
            resources_ = &resources;
            uploaded_revision_ = 0;
            if (!status_->Init())
            {
                error = status_->canvas.error;
                return false;
            }
        }
        if (!status_->Update(h.stats, unsigned(viewport->Size.x), unsigned(viewport->Size.y)))
        {
            error = status_->canvas.error.empty() ? "Invalid original HUD state" : status_->canvas.error;
            return false;
        }
        if (uploaded_revision_ != status_->revision && !upload(error))
            return false;
        auto* draw = ImGui::GetBackgroundDrawList();
        unsigned offset = 0, i = 0;
        for (const auto* tab : status_->tabs())
        {
            const auto& s = *tab->hTab;
            if (i != 3 || tab->value)
            {
                const ImVec2 p{viewport->Pos.x + tab->locX, viewport->Pos.y + tab->locY};
                draw->AddImage((ImTextureID)(intptr_t)texture_, p, {p.x + s.width, p.y + s.height},
                               {float(offset) / 655.f, 0}, {float(offset + s.width) / 655.f, float(s.height) / 75.f});
            }
            offset += s.width;
            ++i;
        }
        return true;
    }
} // namespace OpenBlood2
