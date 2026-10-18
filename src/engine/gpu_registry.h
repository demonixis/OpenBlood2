#pragma once
#include "engine/world_view.h"
#include <SDL3/SDL_gpu.h>
#include <map>
#include <memory>
#include <unordered_map>

namespace OpenBlood2
{
    // Generational GPU resource handle. Index 0 is null. Releasing or clearing a slot
    // changes its generation, so a stale handle resolves to nullptr, never to a reused object.
    template <typename Tag> struct GpuHandle
    {
        std::uint32_t index = 0, generation = 0;
        explicit operator bool() const
        {
            return index != 0;
        }
        friend bool operator==(GpuHandle, GpuHandle) = default;
    };
    using TextureHandle = GpuHandle<struct GpuTextureTag>;
    using BufferHandle = GpuHandle<struct GpuBufferTag>;

    // Copy pass in the frame's command buffer when one is given, so uploads share the frame
    // submission; otherwise in a dedicated command buffer submitted by end_upload.
    SDL_GPUCopyPass* begin_upload(SDL_GPUDevice*, SDL_GPUCommandBuffer* frame, SDL_GPUCommandBuffer*& owned);
    bool end_upload(SDL_GPUCopyPass*, SDL_GPUCommandBuffer* owned);
    struct GpuRegistryStats
    {
        std::size_t textures = 0, texture_bytes = 0, targets = 0, buffers = 0, buffer_bytes = 0, shared = 0;
        std::uint64_t texture_uploads = 0, texture_hits = 0, flushes = 0;
        std::size_t staged_bytes = 0; // Last flush.
    };

    // Single owner of the presentation's SDL_GPU textures, buffers and samplers.
    // Material textures are content-addressed (resource key plus decoded pixels) and
    // shared by every renderer using the registry. Uploads are staged on the CPU and
    // recorded as one copy pass per flush. SDL defers the actual GPU destruction.
    class GpuRegistry
    {
      public:
        explicit GpuRegistry(SDL_GPUDevice* device) : device_(device) {}
        ~GpuRegistry();
        GpuRegistry(const GpuRegistry&) = delete;
        GpuRegistry& operator=(const GpuRegistry&) = delete;
        SDL_GPUDevice* device() const
        {
            return device_;
        }

        // One handle per material: slot 0 is white when the list is empty; failed and
        // animated-sprite entries get a null handle. Known content resolves without upload;
        // new textures upload at the next flush. On failure `out` is cleared.
        bool textures(std::span<const WorldMaterial>, std::vector<TextureHandle>& out, std::string& error);
        // Keeps handles for an unchanged, still valid material prefix (views only append
        // materials until clear()) and acquires the rest; `paths` tracks the prefix.
        bool refresh_textures(std::span<const WorldMaterial>, std::vector<TextureHandle>& handles,
                              std::vector<std::string>& paths, std::string& error);
        // Render targets and other textures with explicit lifetime (not content-addressed).
        TextureHandle create_texture(const SDL_GPUTextureCreateInfo&, std::string& error);
        SDL_GPUTexture* get(TextureHandle) const;
        void release(TextureHandle&);
        // Nearest filtering with point mip selection over authored DTX levels.
        SDL_GPUSampler* sampler(bool clamp, std::string& error);
        // Bilinear, clamped, single level: lightmap pages (texels sit on the light grid).
        SDL_GPUSampler* lightmap_sampler(std::string& error);

        BufferHandle create_buffer(SDL_GPUBufferUsageFlags, Uint32 bytes, std::string& error);
        // Replaces the buffer with a larger one when needed; contents are not preserved.
        bool reserve(BufferHandle&, SDL_GPUBufferUsageFlags, Uint32 bytes, std::string& error);
        SDL_GPUBuffer* get(BufferHandle) const;
        Uint32 size(BufferHandle) const;
        void release(BufferHandle&);
        // Copies the bytes now; the GPU copy is recorded at the next flush.
        bool stage(BufferHandle, const void* data, Uint32 bytes, bool cycle, std::string& error);

        // Immutable buffers shared by key while their owning asset is alive.
        struct Shared
        {
            BufferHandle buffer;
            std::uint32_t count = 0;
        };
        const Shared* shared(const void* key, const std::weak_ptr<const void>& owner) const;
        // Takes ownership of `entry.buffer`; replaces an entry whose owner changed.
        const Shared* share(const void* key, std::weak_ptr<const void> owner, Shared entry);

        // Records every staged upload as one copy pass in `frame`, or in a dedicated
        // submitted command buffer when frame is null. Staging is empty afterwards.
        bool flush(SDL_GPUCommandBuffer* frame, std::string& error);
        // World revision boundary: releases every content texture. Targets, buffers and
        // shared assets keep their owners; expired shared assets are pruned.
        void clear_textures();
        GpuRegistryStats stats;
        static constexpr std::size_t texture_limit = 16384, texture_byte_limit = 512 * 1024 * 1024,
                                     buffer_limit = 65536, staging_limit = 256 * 1024 * 1024;

      private:
        struct TextureSlot
        {
            SDL_GPUTexture* texture = nullptr;
            std::uint32_t generation = 1;
            bool content = false;
            std::size_t bytes = 0;
            std::string key;
        };
        struct BufferSlot
        {
            SDL_GPUBuffer* buffer = nullptr;
            std::uint32_t generation = 1;
            Uint32 bytes = 0;
        };
        struct SharedSlot
        {
            std::weak_ptr<const void> owner;
            Shared entry;
        };
        struct BufferCopy
        {
            BufferHandle buffer;
            std::size_t offset = 0;
            Uint32 bytes = 0;
            bool cycle = false;
        };
        struct TextureCopy
        {
            TextureHandle texture;
            std::size_t offset = 0;
            Uint32 level = 0, width = 0, height = 0;
        };
        TextureHandle add_texture(SDL_GPUTexture*, bool content, std::size_t bytes, std::string key);
        void prune_shared();
        SDL_GPUDevice* device_ = nullptr;
        std::vector<TextureSlot> textures_{TextureSlot{}}; // Slot 0 is the null handle.
        std::vector<std::uint32_t> free_textures_;
        std::vector<BufferSlot> buffers_{BufferSlot{}};
        std::vector<std::uint32_t> free_buffers_;
        std::unordered_map<std::string, TextureHandle> content_; // key + content hash -> texture
        std::map<const void*, SharedSlot> shared_;
        std::array<SDL_GPUSampler*, 3> samplers_{}; // Repeat, clamp, lightmap.
        std::vector<std::uint8_t> staging_;
        std::vector<BufferCopy> buffer_copies_;
        std::vector<TextureCopy> texture_copies_;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        Uint32 transfer_capacity_ = 0;
    };
} // namespace OpenBlood2
