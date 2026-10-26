#include "engine/gpu_registry.h"
#include <algorithm>
#include <cstring>

namespace OpenBlood2
{
    namespace
    {
        // FNV-1a over dimensions and every decoded level: equal keys mean equal pixels.
        std::uint64_t content_hash(const Texture& image)
        {
            std::uint64_t hash = 1469598103934665603ull;
            auto mix = [&](const std::uint8_t* data, std::size_t size)
            {
                for (std::size_t i = 0; i < size; ++i)
                    hash = (hash ^ data[i]) * 1099511628211ull;
            };
            const std::uint32_t header[] = {image.width, image.height, image.mipmaps};
            mix(reinterpret_cast<const std::uint8_t*>(header), sizeof(header));
            mix(image.rgba.data(), image.rgba.size());
            mix(image.mip_rgba.data(), image.mip_rgba.size());
            return hash;
        }
    } // namespace
    SDL_GPUCopyPass* begin_upload(SDL_GPUDevice* device, SDL_GPUCommandBuffer* frame, SDL_GPUCommandBuffer*& owned)
    {
        owned = frame ? nullptr : SDL_AcquireGPUCommandBuffer(device);
        auto* pass = frame || owned ? SDL_BeginGPUCopyPass(frame ? frame : owned) : nullptr;
        if (!pass && owned)
        {
            SDL_CancelGPUCommandBuffer(owned);
            owned = nullptr;
        }
        return pass;
    }
    bool end_upload(SDL_GPUCopyPass* pass, SDL_GPUCommandBuffer* owned)
    {
        SDL_EndGPUCopyPass(pass);
        return !owned || SDL_SubmitGPUCommandBuffer(owned);
    }
    GpuRegistry::~GpuRegistry()
    {
        for (auto& slot : textures_)
            if (slot.texture)
                SDL_ReleaseGPUTexture(device_, slot.texture);
        for (auto& slot : buffers_)
            if (slot.buffer)
                SDL_ReleaseGPUBuffer(device_, slot.buffer);
        for (auto* sampler : samplers_)
            if (sampler)
                SDL_ReleaseGPUSampler(device_, sampler);
        if (transfer_)
            SDL_ReleaseGPUTransferBuffer(device_, transfer_);
    }
    TextureHandle GpuRegistry::add_texture(SDL_GPUTexture* texture, bool content, std::size_t bytes, std::string key)
    {
        std::uint32_t index;
        if (!free_textures_.empty())
        {
            index = free_textures_.back();
            free_textures_.pop_back();
        }
        else
        {
            index = static_cast<std::uint32_t>(textures_.size());
            textures_.emplace_back();
        }
        auto& slot = textures_[index];
        slot.texture = texture;
        slot.content = content;
        slot.bytes = bytes;
        slot.key = std::move(key);
        (content ? stats.textures : stats.targets) += 1;
        if (content)
            stats.texture_bytes += bytes;
        return {index, slot.generation};
    }
    SDL_GPUTexture* GpuRegistry::get(TextureHandle handle) const
    {
        return handle.index && handle.index < textures_.size() &&
                       textures_[handle.index].generation == handle.generation
                   ? textures_[handle.index].texture
                   : nullptr;
    }
    void GpuRegistry::release(TextureHandle& handle)
    {
        if (!get(handle))
        {
            handle = {};
            return;
        }
        auto& slot = textures_[handle.index];
        SDL_ReleaseGPUTexture(device_, slot.texture);
        (slot.content ? stats.textures : stats.targets) -= 1;
        if (slot.content)
        {
            stats.texture_bytes -= slot.bytes;
            content_.erase(slot.key);
        }
        slot = {nullptr, slot.generation + 1, false, 0, {}};
        free_textures_.push_back(handle.index);
        handle = {};
    }
    TextureHandle GpuRegistry::create_texture(const SDL_GPUTextureCreateInfo& info, std::string& error)
    {
        if (stats.textures + stats.targets >= texture_limit)
        {
            error = "GPU texture count limit exceeded";
            return {};
        }
        auto* texture = SDL_CreateGPUTexture(device_, &info);
        if (!texture)
        {
            error = SDL_GetError();
            return {};
        }
        return add_texture(texture, false, 0, {});
    }
    bool GpuRegistry::textures(std::span<const WorldMaterial> materials, std::vector<TextureHandle>& out,
                               std::string& error)
    {
        static const Texture white{1, 1, 1, 0, 0, {255, 255, 255, 255}, {}};
        out.clear();
        const auto count = std::max(std::size_t(1), materials.size());
        if (count > 4097)
        {
            error = "GPU texture count limit exceeded";
            return false;
        }
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const Texture* image = &white;
            if (!materials.empty())
                image = materials[i].error.empty() && materials[i].frames.empty() ? &materials[i].image : nullptr;
            if (!image)
            {
                out.push_back({});
                continue;
            }
            auto key = (materials.empty() ? std::string("<white>") : materials[i].path) + '#' +
                       std::to_string(content_hash(*image));
            if (const auto found = content_.find(key); found != content_.end() && get(found->second))
            {
                out.push_back(found->second);
                ++stats.texture_hits;
                continue;
            }
            const auto handle = upload_content(std::move(key), *image, error);
            if (!handle)
            {
                out.clear();
                return false;
            }
            out.push_back(handle);
        }
        return true;
    }
    TextureHandle GpuRegistry::upload_content(std::string key, const Texture& image, std::string& error)
    {
        const auto bytes = image.rgba.size() + image.mip_rgba.size();
        if (!image.width || !image.height || image.width > 1024 || image.height > 1024 || image.mipmaps > 11 ||
            image.rgba.size() != std::size_t(image.width) * image.height * 4 ||
            image.mip_rgba.size() != texture_mip_bytes(image) || bytes > texture_byte_limit - stats.texture_bytes ||
            bytes > staging_limit - staging_.size())
        {
            error = "Invalid GPU texture dimensions/payload/budget";
            return {};
        }
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = image.width;
        info.height = image.height;
        info.layer_count_or_depth = 1;
        info.num_levels = std::max(1u, image.mipmaps);
        if (stats.textures + stats.targets >= texture_limit)
        {
            error = "GPU texture count limit exceeded";
            return {};
        }
        auto* texture = SDL_CreateGPUTexture(device_, &info);
        if (!texture)
        {
            error = SDL_GetError();
            return {};
        }
        const auto handle = add_texture(texture, true, bytes, key);
        content_.emplace(std::move(key), handle);
        std::size_t offset = staging_.size();
        staging_.insert(staging_.end(), image.rgba.begin(), image.rgba.end());
        staging_.insert(staging_.end(), image.mip_rgba.begin(), image.mip_rgba.end());
        for (Uint32 level = 0, w = image.width, h = image.height; level < info.num_levels;
             ++level, w = std::max(1u, w / 2), h = std::max(1u, h / 2))
        {
            texture_copies_.push_back({handle, offset, level, w, h});
            offset += std::size_t(w) * h * 4;
        }
        ++stats.texture_uploads;
        return handle;
    }
    TextureHandle GpuRegistry::derived_texture(std::string_view kind, const Texture& image, std::string& error)
    {
        auto key = std::string(kind) + '#' + std::to_string(content_hash(image));
        if (const auto found = content_.find(key); found != content_.end() && get(found->second))
        {
            ++stats.texture_hits;
            return found->second;
        }
        return upload_content(std::move(key), image, error);
    }
    bool GpuRegistry::refresh_textures(std::span<const WorldMaterial> materials, std::vector<TextureHandle>& handles,
                                       std::vector<std::string>& paths, std::string& error)
    {
        std::size_t keep = 0;
        while (!materials.empty() && keep < handles.size() && keep < paths.size() && keep < materials.size() &&
               paths[keep] == materials[keep].path && (!handles[keep] || get(handles[keep])))
            ++keep;
        handles.resize(keep);
        paths.resize(keep);
        if (materials.size() > 4097)
        {
            error = "GPU texture count limit exceeded";
            handles.clear();
            paths.clear();
            return false;
        }
        if (keep == 0 || keep < materials.size())
        {
            std::vector<TextureHandle> added;
            if (!textures(materials.subspan(keep), added, error))
            {
                handles.clear();
                paths.clear();
                return false;
            }
            handles.insert(handles.end(), added.begin(), added.end());
        }
        for (std::size_t i = paths.size(); i < materials.size(); ++i)
            paths.push_back(materials[i].path);
        return true;
    }
    SDL_GPUSampler* GpuRegistry::sampler(bool clamp, std::string& error)
    {
        auto*& sampler = samplers_[clamp];
        if (!sampler)
        {
            SDL_GPUSamplerCreateInfo info{};
            info.min_filter = info.mag_filter = SDL_GPU_FILTER_NEAREST;
            info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            info.max_lod = 1000; // Keep the complete authored DTX mip chain.
            if (!clamp)
            {
                // Isotropic point filtering loses skin/wall detail at grazing angles.
                info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
                info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
                info.enable_anisotropy = true;
                info.max_anisotropy = 8;
            }
            info.address_mode_u = info.address_mode_v = info.address_mode_w =
                clamp ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            sampler = SDL_CreateGPUSampler(device_, &info);
            if (!sampler)
                error = SDL_GetError();
        }
        return sampler;
    }
    SDL_GPUSampler* GpuRegistry::normal_sampler(std::string& error)
    {
        auto*& sampler = samplers_[3];
        if (!sampler)
        {
            SDL_GPUSamplerCreateInfo info{};
            info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
            info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            info.max_lod = 1000;
            info.address_mode_u = info.address_mode_v = info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            sampler = SDL_CreateGPUSampler(device_, &info);
            if (!sampler)
                error = SDL_GetError();
        }
        return sampler;
    }
    SDL_GPUSampler* GpuRegistry::sprite_sampler(std::string& error)
    {
        auto*& sampler = samplers_[4];
        if (!sampler)
        {
            SDL_GPUSamplerCreateInfo info{};
            info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
            info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            info.max_lod = 1000;
            info.address_mode_u = info.address_mode_v = info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            sampler = SDL_CreateGPUSampler(device_, &info);
            if (!sampler)
                error = SDL_GetError();
        }
        return sampler;
    }
    SDL_GPUSampler* GpuRegistry::lightmap_sampler(std::string& error)
    {
        auto*& sampler = samplers_[2];
        if (!sampler)
        {
            SDL_GPUSamplerCreateInfo info{};
            info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
            info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            info.address_mode_u = info.address_mode_v = info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            sampler = SDL_CreateGPUSampler(device_, &info);
            if (!sampler)
                error = SDL_GetError();
        }
        return sampler;
    }
    BufferHandle GpuRegistry::create_buffer(SDL_GPUBufferUsageFlags usage, Uint32 bytes, std::string& error)
    {
        if (stats.buffers >= buffer_limit || !bytes)
        {
            error = "GPU buffer count/size limit exceeded";
            return {};
        }
        SDL_GPUBufferCreateInfo info{usage, bytes, 0};
        auto* buffer = SDL_CreateGPUBuffer(device_, &info);
        if (!buffer)
        {
            error = SDL_GetError();
            return {};
        }
        std::uint32_t index;
        if (!free_buffers_.empty())
        {
            index = free_buffers_.back();
            free_buffers_.pop_back();
        }
        else
        {
            index = static_cast<std::uint32_t>(buffers_.size());
            buffers_.emplace_back();
        }
        auto& slot = buffers_[index];
        slot.buffer = buffer;
        slot.bytes = bytes;
        ++stats.buffers;
        stats.buffer_bytes += bytes;
        return {index, slot.generation};
    }
    SDL_GPUBuffer* GpuRegistry::get(BufferHandle handle) const
    {
        return handle.index && handle.index < buffers_.size() && buffers_[handle.index].generation == handle.generation
                   ? buffers_[handle.index].buffer
                   : nullptr;
    }
    Uint32 GpuRegistry::size(BufferHandle handle) const
    {
        return get(handle) ? buffers_[handle.index].bytes : 0;
    }
    void GpuRegistry::release(BufferHandle& handle)
    {
        if (get(handle))
        {
            auto& slot = buffers_[handle.index];
            SDL_ReleaseGPUBuffer(device_, slot.buffer);
            --stats.buffers;
            stats.buffer_bytes -= slot.bytes;
            slot = {nullptr, slot.generation + 1, 0};
            free_buffers_.push_back(handle.index);
        }
        handle = {};
    }
    bool GpuRegistry::reserve(BufferHandle& handle, SDL_GPUBufferUsageFlags usage, Uint32 bytes, std::string& error)
    {
        const auto current = size(handle);
        if (bytes <= current)
            return true;
        const auto next = create_buffer(usage, std::max<Uint32>(bytes, current + current / 2), error);
        if (!next)
            return false;
        release(handle);
        handle = next;
        return true;
    }
    bool GpuRegistry::stage(BufferHandle handle, const void* data, Uint32 bytes, bool cycle, std::string& error)
    {
        if (!get(handle) || bytes > size(handle) || bytes > staging_limit - staging_.size())
        {
            error = "Invalid or oversized GPU buffer upload";
            return false;
        }
        const auto* first = static_cast<const std::uint8_t*>(data);
        buffer_copies_.push_back({handle, staging_.size(), bytes, cycle});
        staging_.insert(staging_.end(), first, first + bytes);
        return true;
    }
    const GpuRegistry::Shared* GpuRegistry::shared(const void* key, const std::weak_ptr<const void>& owner) const
    {
        const auto found = shared_.find(key);
        if (found == shared_.end() || found->second.owner.expired() || found->second.owner.owner_before(owner) ||
            owner.owner_before(found->second.owner) || !get(found->second.entry.buffer))
            return nullptr;
        return &found->second.entry;
    }
    const GpuRegistry::Shared* GpuRegistry::share(const void* key, std::weak_ptr<const void> owner, Shared entry)
    {
        auto& slot = shared_[key];
        if (slot.entry.buffer != entry.buffer)
            release(slot.entry.buffer);
        slot.owner = std::move(owner);
        slot.entry = entry;
        stats.shared = shared_.size();
        return &slot.entry;
    }
    void GpuRegistry::prune_shared()
    {
        for (auto it = shared_.begin(); it != shared_.end();)
        {
            if (it->second.owner.expired())
            {
                release(it->second.entry.buffer);
                it = shared_.erase(it);
            }
            else
                ++it;
        }
        stats.shared = shared_.size();
    }
    bool GpuRegistry::flush(SDL_GPUCommandBuffer* frame, std::string& error)
    {
        stats.staged_bytes = staging_.size();
        prune_shared();
        auto reset = [&]()
        {
            staging_.clear();
            buffer_copies_.clear();
            texture_copies_.clear();
        };
        if (buffer_copies_.empty() && texture_copies_.empty())
        {
            reset();
            return true;
        }
        const auto bytes = static_cast<Uint32>(staging_.size());
        if (bytes > transfer_capacity_)
        {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                 std::max<Uint32>(bytes, transfer_capacity_ + transfer_capacity_ / 2),
                                                 0};
            auto* next = SDL_CreateGPUTransferBuffer(device_, &info);
            if (!next)
            {
                error = SDL_GetError();
                reset();
                return false;
            }
            if (transfer_)
                SDL_ReleaseGPUTransferBuffer(device_, transfer_);
            transfer_ = next;
            transfer_capacity_ = info.size;
        }
        auto* mapped = SDL_MapGPUTransferBuffer(device_, transfer_, true);
        if (!mapped)
        {
            error = SDL_GetError();
            reset();
            return false;
        }
        std::memcpy(mapped, staging_.data(), bytes);
        SDL_UnmapGPUTransferBuffer(device_, transfer_);
        SDL_GPUCommandBuffer* owned = nullptr;
        auto* pass = begin_upload(device_, frame, owned);
        if (!pass)
        {
            error = SDL_GetError();
            reset();
            return false;
        }
        // A handle released after staging simply skips its copy.
        for (const auto& copy : texture_copies_)
            if (auto* texture = get(copy.texture))
            {
                SDL_GPUTextureTransferInfo source{};
                source.transfer_buffer = transfer_;
                source.offset = static_cast<Uint32>(copy.offset);
                source.pixels_per_row = copy.width;
                source.rows_per_layer = copy.height;
                SDL_GPUTextureRegion target{};
                target.texture = texture;
                target.mip_level = copy.level;
                target.w = copy.width;
                target.h = copy.height;
                target.d = 1;
                SDL_UploadToGPUTexture(pass, &source, &target, false);
            }
        for (const auto& copy : buffer_copies_)
            if (auto* buffer = get(copy.buffer))
            {
                SDL_GPUTransferBufferLocation source{transfer_, static_cast<Uint32>(copy.offset)};
                SDL_GPUBufferRegion target{buffer, 0, copy.bytes};
                SDL_UploadToGPUBuffer(pass, &source, &target, copy.cycle);
            }
        reset();
        ++stats.flushes;
        if (!end_upload(pass, owned))
        {
            error = SDL_GetError();
            return false;
        }
        return true;
    }
    void GpuRegistry::clear_textures()
    {
        for (std::uint32_t i = 1; i < textures_.size(); ++i)
            if (textures_[i].texture && textures_[i].content)
            {
                TextureHandle handle{i, textures_[i].generation};
                release(handle);
            }
        content_.clear();
        prune_shared();
    }
} // namespace OpenBlood2
