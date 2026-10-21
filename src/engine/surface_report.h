#pragma once
#include "engine/surface_materials.h"
#include "engine/world_view.h"
#include <functional>
#include <map>
#include <span>

namespace OpenBlood2
{
    // Why a material has, or lacks, a generated normal map in the enhanced renderer.
    enum class NormalMapState : std::uint8_t
    {
        Generated,  // Generated from the albedo (or would be, once the renderer prepares it).
        Disabled,   // NORMAL MAPS [OFF].
        ClassOff,   // The resolved class has normal strength 0.
        NotTexture, // Lightmap pages and the white slot.
        Animated,   // SPR materials: their frames are separate materials.
        Failed      // Unresolved resource: the original magenta marker, no enhanced shading.
    };
    const char* normal_map_state_name(NormalMapState);
    // The rule SurfaceMaterials::refresh applies; shared so inspection never disagrees with it.
    NormalMapState normal_map_state(const WorldMaterial&, const SurfaceMaterial&, bool normal_maps);
    // What the enhanced renderer resolves for one world material (roadmap 5.5).
    struct SurfaceMaterialReport
    {
        std::uint32_t material = 0; // Index in WorldMesh::materials.
        std::string path, binding;  // binding: the deciding rule pattern or texture override path.
        SurfaceSource source = SurfaceSource::Default;
        SurfaceMaterial values;
        NormalMapState normal_map = NormalMapState::NotTexture;
        std::uint32_t fullbright_faces = 0; // DAT emissive identification (SURF_BRIGHT/unlit faces).
        bool emissive() const
        {
            return values.emissive > 0;
        }
    };
    // Every texture material of a world mesh with its resolved class and maps, the fallbacks
    // (default class, failed resources) and the manifest's texture overrides naming no mounted
    // resource. Inspection only: building it changes no rendering state.
    struct SurfaceReport
    {
        std::vector<SurfaceMaterialReport> materials;
        std::map<std::string, std::size_t> classes; // Materials per resolved class name.
        std::size_t defaults = 0, failed = 0, emissive = 0, generated = 0, fullbright_materials = 0;
        std::size_t fullbright_faces = 0, emissive_fullbright = 0; // emissive_fullbright: both sources.
        std::vector<std::string> unbound_overrides;
    };
    // `exists` answers whether a normalized resource path is mounted; null skips that check.
    SurfaceReport surface_report(const SurfaceManifest&, const WorldMesh&, bool normal_maps,
                                 const std::function<bool(std::string_view)>& exists = {});
} // namespace OpenBlood2
