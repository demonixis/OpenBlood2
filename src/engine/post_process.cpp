#include "engine/post_process.h"
#include "engine/world_renderer.h"
#include "engine/world_shaders.h"

namespace OpenBlood2
{
    namespace
    {
        constexpr SDL_GPUTextureFormat bloom_format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        constexpr SDL_GPUTextureFormat occlusion_format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        constexpr SDL_GPUTextureFormat view_depth_format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    } // namespace
    PostProcess::PostProcess(GpuRegistry& registry) : registry_(registry), device_(registry.device()) {}
    PostProcess::~PostProcess()
    {
        release_targets();
        for (auto* pipeline : {down_, up_, composite_, fxaa_, ao_depth_, gtao_, ao_blur_})
            if (pipeline)
                SDL_ReleaseGPUGraphicsPipeline(device_, pipeline);
    }
    bool PostProcess::prepare_pipelines(SDL_GPUTextureFormat output, std::string& error)
    {
        if (pipeline_format_ == output && fxaa_)
            return true;
        for (auto** pipeline : {&down_, &up_, &composite_, &fxaa_, &ao_depth_, &gtao_, &ao_blur_})
        {
            if (*pipeline)
                SDL_ReleaseGPUGraphicsPipeline(device_, *pipeline);
            *pipeline = nullptr;
        }
        pipeline_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
        const auto formats = SDL_GetGPUShaderFormats(device_);
        SDL_GPUShaderCreateInfo vertex_info{}, down_info{}, up_info{}, composite_info{}, fxaa_info{}, depth_info{},
            gtao_info{}, blur_info{};
        if (!world_shader_info(WorldShader::PostVertex, formats, vertex_info) ||
            !world_shader_info(WorldShader::BloomDownFragment, formats, down_info) ||
            !world_shader_info(WorldShader::BloomUpFragment, formats, up_info) ||
            !world_shader_info(WorldShader::CompositeFragment, formats, composite_info) ||
            !world_shader_info(WorldShader::FxaaFragment, formats, fxaa_info) ||
            !world_shader_info(WorldShader::AoDepthFragment, formats, depth_info) ||
            !world_shader_info(WorldShader::GtaoFragment, formats, gtao_info) ||
            !world_shader_info(WorldShader::AoBlurFragment, formats, blur_info))
        {
            error = "No bundled post-processing shader for this GPU backend (SPIR-V or MSL required)";
            return false;
        }
        auto* vertex = SDL_CreateGPUShader(device_, &vertex_info);
        bool success = vertex != nullptr;
        struct Kind
        {
            SDL_GPUGraphicsPipeline** pipeline;
            const SDL_GPUShaderCreateInfo* fragment;
            SDL_GPUTextureFormat format;
            bool additive;
        };
        for (const auto& kind :
             {Kind{&down_, &down_info, bloom_format, false}, Kind{&up_, &up_info, bloom_format, true},
              Kind{&composite_, &composite_info, output, false}, Kind{&fxaa_, &fxaa_info, output, false},
              Kind{&ao_depth_, &depth_info, view_depth_format, false},
              Kind{&gtao_, &gtao_info, occlusion_format, false}, Kind{&ao_blur_, &blur_info, occlusion_format, false}})
        {
            auto* fragment = success ? SDL_CreateGPUShader(device_, kind.fragment) : nullptr;
            SDL_GPUColorTargetDescription target{};
            target.format = kind.format;
            if (kind.additive)
            {
                target.blend_state.enable_blend = true;
                target.blend_state.src_color_blendfactor = target.blend_state.dst_color_blendfactor =
                    target.blend_state.src_alpha_blendfactor = target.blend_state.dst_alpha_blendfactor =
                        SDL_GPU_BLENDFACTOR_ONE;
                target.blend_state.color_blend_op = target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            }
            SDL_GPUGraphicsPipelineCreateInfo create{};
            create.vertex_shader = vertex;
            create.fragment_shader = fragment;
            create.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            create.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            create.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            create.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
            create.target_info.color_target_descriptions = &target;
            create.target_info.num_color_targets = 1;
            if (fragment)
                *kind.pipeline = SDL_CreateGPUGraphicsPipeline(device_, &create);
            success = success && fragment && *kind.pipeline;
            if (!success)
                error = SDL_GetError();
            if (fragment)
                SDL_ReleaseGPUShader(device_, fragment);
        }
        if (vertex)
            SDL_ReleaseGPUShader(device_, vertex);
        if (!success)
        {
            if (error.empty())
                error = "Cannot create the post-processing pipelines";
            return false;
        }
        pipeline_format_ = output;
        return true;
    }
    bool PostProcess::target(TextureHandle& handle, SDL_GPUTextureFormat format, Uint32 width, Uint32 height,
                             SDL_GPUSampleCount samples, std::string& error)
    {
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = format;
        // Multisampled targets are only resolved; single-sampled ones feed the next pass.
        info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                     (samples == SDL_GPU_SAMPLECOUNT_1 ? SDL_GPU_TEXTUREUSAGE_SAMPLER : SDL_GPUTextureUsageFlags(0));
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = samples;
        handle = registry_.create_texture(info, error);
        if (handle)
            ++stats.targets;
        return bool(handle);
    }
    void PostProcess::release_targets()
    {
        for (auto* handle : {&multisample_, &scene_, &multisample_glow_, &glow_, &multisample_depth_, &depth_copy_,
                             &display_, &view_depth_, &occlusion_, &occlusion_blur_})
            registry_.release(*handle);
        for (auto& level : levels_)
            registry_.release(level);
        level_count_ = 0;
        stats.targets = 0;
    }
    bool PostProcess::begin(SDL_GPUTextureFormat output, Uint32 width, Uint32 height, bool enhanced,
                            const PostSettings& settings, std::string& error)
    {
        if (!width || !height || width > 16384 || height > 16384)
        {
            error = "invalid output size";
            return false;
        }
        if (!prepare_pipelines(output, error))
            return false;
        const bool ao = enhanced && settings.ambient_occlusion != AmbientOcclusion::Off;
        const bool hdr = enhanced && (settings.bloom || settings.tonemap != Tonemap::Off || ao);
        const bool bloom = enhanced && settings.bloom && width >= 4 && height >= 4;
        const bool aux = bloom || ao;
        const bool fxaa = settings.antialiasing == Antialiasing::Fxaa;
        const auto scene_format = hdr ? scene_hdr_format : output;
        const Uint32 requested = settings.antialiasing == Antialiasing::Msaa4   ? 4
                                 : settings.antialiasing == Antialiasing::Msaa2 ? 2
                                                                                : 1;
        // The highest supported count up to the request, for every multisampled attachment; queried
        // again only when the request or the formats change.
        if (supported_.format != scene_format || supported_.requested != requested || supported_.aux != aux)
        {
            supported_ = {scene_format, requested, aux, SDL_GPU_SAMPLECOUNT_1};
            for (Uint32 count = requested; count > 1 && supported_.samples == SDL_GPU_SAMPLECOUNT_1; count /= 2)
            {
                const auto candidate = count == 4 ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_2;
                if (SDL_GPUTextureSupportsSampleCount(device_, scene_format, candidate) &&
                    SDL_GPUTextureSupportsSampleCount(device_, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, candidate) &&
                    (!aux || (SDL_GPUTextureSupportsSampleCount(device_, scene_glow_format, candidate) &&
                              SDL_GPUTextureSupportsSampleCount(device_, scene_depth_copy_format, candidate))))
                    supported_.samples = candidate;
            }
        }
        const auto samples = supported_.samples;
        stats.hdr = hdr;
        stats.bloom = bloom;
        stats.fxaa = fxaa;
        stats.tonemap = hdr ? settings.tonemap : Tonemap::Off;
        stats.ambient_occlusion = ao ? settings.ambient_occlusion : AmbientOcclusion::Off;
        ambient_occlusion_ = stats.ambient_occlusion;
        stats.requested_samples = requested;
        stats.samples = samples == SDL_GPU_SAMPLECOUNT_4 ? 4 : samples == SDL_GPU_SAMPLECOUNT_2 ? 2 : 1;
        tonemap_ = stats.tonemap;
        if (output == output_format_ && width == width_ && height == height_ && scene_format == scene_format_ &&
            samples == samples_ && hdr == hdr_ && bloom == bloom_ && ao == ao_ && fxaa == fxaa_pass_)
            return true;
        release_targets();
        output_format_ = output;
        scene_format_ = scene_format;
        samples_ = samples;
        hdr_ = hdr;
        aux_ = aux;
        bloom_ = bloom;
        ao_ = ao;
        fxaa_pass_ = fxaa;
        width_ = width;
        height_ = height;
        const bool direct = !hdr && !fxaa;
        bool success =
            (samples == SDL_GPU_SAMPLECOUNT_1 || target(multisample_, scene_format, width, height, samples, error)) &&
            (direct || target(scene_, scene_format, width, height, SDL_GPU_SAMPLECOUNT_1, error)) &&
            (!hdr || !fxaa || target(display_, output, width, height, SDL_GPU_SAMPLECOUNT_1, error));
        if (success && aux)
            success = (samples == SDL_GPU_SAMPLECOUNT_1 ||
                       (target(multisample_glow_, scene_glow_format, width, height, samples, error) &&
                        target(multisample_depth_, scene_depth_copy_format, width, height, samples, error))) &&
                      target(glow_, scene_glow_format, width, height, SDL_GPU_SAMPLECOUNT_1, error) &&
                      target(depth_copy_, scene_depth_copy_format, width, height, SDL_GPU_SAMPLECOUNT_1, error);
        if (success && ao)
        {
            const Uint32 w = (width + 1) / 2, h = (height + 1) / 2;
            success = target(view_depth_, view_depth_format, w, h, SDL_GPU_SAMPLECOUNT_1, error) &&
                      target(occlusion_, occlusion_format, w, h, SDL_GPU_SAMPLECOUNT_1, error) &&
                      target(occlusion_blur_, occlusion_format, w, h, SDL_GPU_SAMPLECOUNT_1, error);
        }
        if (success && bloom)
        {
            // Half resolution first, then halving while both sides keep two texels.
            Uint32 w = (width + 1) / 2, h = (height + 1) / 2;
            for (; success && level_count_ < bloom_level_limit && w >= 2 && h >= 2; w = (w + 1) / 2, h = (h + 1) / 2)
            {
                success = target(levels_[level_count_], bloom_format, w, h, SDL_GPU_SAMPLECOUNT_1, error);
                level_sizes_[level_count_++] = {w, h};
            }
        }
        if (!success)
        {
            release_targets();
            width_ = height_ = 0; // Plan again next frame.
            error = "targets: " + error;
            return false;
        }
        return true;
    }
    SceneTarget PostProcess::scene_target(SDL_GPUTexture* output) const
    {
        SceneTarget result;
        auto* single = !hdr_ && !fxaa_pass_ ? output : registry_.get(scene_);
        const bool multisampled = samples_ != SDL_GPU_SAMPLECOUNT_1;
        result.color = multisampled ? registry_.get(multisample_) : single;
        result.resolve = multisampled ? single : nullptr;
        if (aux_)
        {
            result.glow = registry_.get(multisampled ? multisample_glow_ : glow_);
            result.glow_resolve = multisampled ? registry_.get(glow_) : nullptr;
            result.depth_copy = registry_.get(multisampled ? multisample_depth_ : depth_copy_);
            result.depth_copy_resolve = multisampled ? registry_.get(depth_copy_) : nullptr;
        }
        return result;
    }
    bool PostProcess::draw(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* target, bool load,
                           SDL_GPUGraphicsPipeline* pipeline, std::span<SDL_GPUTexture* const> sources,
                           std::span<SDL_GPUSampler* const> samplers, const void* uniform, Uint32 uniform_bytes,
                           std::string& error)
    {
        std::array<SDL_GPUTextureSamplerBinding, 6> bindings{};
        if (!target || sources.size() > bindings.size() || sources.size() != samplers.size())
        {
            error = "Post-processing pass without its textures";
            return false;
        }
        for (std::size_t i = 0; i < sources.size(); ++i)
        {
            if (!sources[i] || !samplers[i])
            {
                error = "Post-processing pass without its textures";
                return false;
            }
            bindings[i] = {sources[i], samplers[i]};
        }
        SDL_GPUColorTargetInfo color{};
        color.texture = target;
        color.load_op = load ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_DONT_CARE;
        color.store_op = SDL_GPU_STOREOP_STORE;
        auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
        if (!pass)
        {
            error = SDL_GetError();
            return false;
        }
        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        SDL_BindGPUFragmentSamplers(pass, 0, bindings.data(), Uint32(sources.size()));
        SDL_PushGPUFragmentUniformData(commands, 0, uniform, uniform_bytes);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
        ++stats.passes;
        return true;
    }
    bool PostProcess::finish(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* output, const PostView& view,
                             std::string& error)
    {
        stats.passes = 0;
        stats.bloom_levels = bloom_ ? level_count_ : 0;
        ++stats.frames;
        if (!hdr_ && !fxaa_pass_)
            return true;
        auto* linear = registry_.lightmap_sampler(error);
        auto* nearest = linear ? registry_.sampler(true, error) : nullptr;
        if (!nearest)
            return false;
        auto* scene = registry_.get(scene_);
        if (ao_)
        {
            if (!(view.height > 0) || !(view.slice >= 0 && view.slice < 1) || !(view.projection[0] > 0) ||
                !(view.projection[1] > 0))
            {
                error = "invalid ambient-occlusion view";
                return false;
            }
            // Half-size linear depth and occlusion, then the depth-aware blur across and down; the
            // composite upsamples it.
            const auto& samples = ao_samples[ambient_occlusion_ == AmbientOcclusion::High];
            const float slice_scale = 1 / (1 - view.slice);
            const Uint32 half_width = (width_ + 1) / 2, half_height = (height_ + 1) / 2;
            const std::array<float, 8> depth{0,
                                             0,
                                             view.projection[2],
                                             view.projection[3],
                                             1.f / float(width_),
                                             1.f / float(height_),
                                             view.slice,
                                             slice_scale};
            SDL_GPUTexture* const copy = registry_.get(depth_copy_);
            SDL_GPUTexture* const linear_depth = registry_.get(view_depth_);
            if (!draw(commands, linear_depth, false, ao_depth_, {&copy, 1}, {&nearest, 1}, depth.data(), sizeof(depth),
                      error))
                return false;
            const std::array<float, 16> gtao{view.projection[0],
                                             view.projection[1],
                                             0,
                                             0,
                                             0,
                                             view.top,
                                             float(width_),
                                             view.height,
                                             1.f / float(half_width),
                                             1.f / float(half_height),
                                             ao_screen_radius,
                                             0,
                                             ao_radius,
                                             float(samples[0]),
                                             float(samples[1]),
                                             ao_power};
            if (!draw(commands, registry_.get(occlusion_), false, gtao_, {&linear_depth, 1}, {&nearest, 1}, gtao.data(),
                      sizeof(gtao), error))
                return false;
            for (int axis = 0; axis < 2; ++axis)
            {
                const std::array<float, 4> blur{axis ? 0 : 1.f / float(half_width), axis ? 1.f / float(half_height) : 0,
                                                0, 0};
                const std::array<SDL_GPUTexture*, 2> sources{registry_.get(axis ? occlusion_blur_ : occlusion_),
                                                             linear_depth};
                const std::array<SDL_GPUSampler*, 2> samplers{nearest, nearest};
                if (!draw(commands, registry_.get(axis ? occlusion_ : occlusion_blur_), false, ao_blur_, sources,
                          samplers, blur.data(), sizeof(blur), error))
                    return false;
            }
        }
        if (bloom_)
        {
            // Downsample the glow into the chain, then add each coarser level into the finer one.
            for (std::size_t i = 0; i < level_count_; ++i)
            {
                const auto source = i ? level_sizes_[i - 1] : std::array<Uint32, 2>{width_, height_};
                const std::array<float, 4> uniform{1.f / float(source[0]), 1.f / float(source[1]), i ? 0.f : 1.f, 0};
                SDL_GPUTexture* const texture = registry_.get(i ? levels_[i - 1] : glow_);
                if (!draw(commands, registry_.get(levels_[i]), false, down_, {&texture, 1}, {&linear, 1},
                          uniform.data(), sizeof(uniform), error))
                    return false;
            }
            for (std::size_t i = level_count_ - 1; i > 0; --i)
            {
                const std::array<float, 4> uniform{1.f / float(level_sizes_[i][0]), 1.f / float(level_sizes_[i][1]), 0,
                                                   0};
                SDL_GPUTexture* const texture = registry_.get(levels_[i]);
                if (!draw(commands, registry_.get(levels_[i - 1]), true, up_, {&texture, 1}, {&linear, 1},
                          uniform.data(), sizeof(uniform), error))
                    return false;
            }
        }
        auto* display = hdr_ && fxaa_pass_ ? registry_.get(display_) : output;
        if (hdr_)
        {
            // The chain sums one blurred copy of the glow per level.
            const float bloom = bloom_ ? bloom_strength / float(level_count_) : 0.f;
            const Uint32 half_width = (width_ + 1) / 2, half_height = (height_ + 1) / 2;
            const std::array<float, 16> uniform{float(tonemap_),
                                                bloom,
                                                filmic_exposure,
                                                ao_ ? 1.f : 0.f,
                                                1.f / float(level_sizes_[0][0] ? level_sizes_[0][0] : 1),
                                                1.f / float(level_sizes_[0][1] ? level_sizes_[0][1] : 1),
                                                0,
                                                0,
                                                view.projection[2],
                                                view.projection[3],
                                                view.slice,
                                                1 / (1 - view.slice),
                                                float(half_width),
                                                float(half_height),
                                                1.f / float(half_width),
                                                1.f / float(half_height)};
            const std::array<SDL_GPUTexture*, 6> sources{scene,
                                                         bloom_ ? registry_.get(levels_[0]) : scene,
                                                         ao_ ? registry_.get(glow_) : scene,
                                                         ao_ ? registry_.get(occlusion_) : scene,
                                                         ao_ ? registry_.get(view_depth_) : scene,
                                                         ao_ ? registry_.get(depth_copy_) : scene};
            const std::array<SDL_GPUSampler*, 6> samplers{nearest, linear, nearest, nearest, nearest, nearest};
            if (!draw(commands, display, false, composite_, sources, samplers, uniform.data(), sizeof(uniform), error))
                return false;
        }
        if (fxaa_pass_)
        {
            const std::array<float, 4> uniform{1.f / float(width_), 1.f / float(height_), 0, 0};
            SDL_GPUTexture* const texture = hdr_ ? display : scene;
            if (!draw(commands, output, false, fxaa_, {&texture, 1}, {&linear, 1}, uniform.data(), sizeof(uniform),
                      error))
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
