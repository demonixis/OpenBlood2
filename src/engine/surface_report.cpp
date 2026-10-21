#include "engine/surface_report.h"

namespace OpenBlood2
{
    namespace
    {
        bool texture_path(std::string_view path)
        {
            return path.size() > 4 && (path.ends_with(".dtx") || path.ends_with(".DTX"));
        }
        bool sprite_path(std::string_view path)
        {
            return path.size() > 4 && (path.ends_with(".spr") || path.ends_with(".SPR"));
        }
    } // namespace
    const char* normal_map_state_name(NormalMapState state)
    {
        switch (state)
        {
            case NormalMapState::Generated:
                return "generated";
            case NormalMapState::Disabled:
                return "off (option)";
            case NormalMapState::ClassOff:
                return "off (class)";
            case NormalMapState::NotTexture:
                return "none";
            case NormalMapState::Animated:
                return "none (animated)";
            case NormalMapState::Failed:
                return "none (failed resource)";
        }
        return "?";
    }
    NormalMapState normal_map_state(const WorldMaterial& entry, const SurfaceMaterial& material, bool normal_maps)
    {
        if (!texture_path(entry.path))
            return sprite_path(entry.path) || !entry.frames.empty() ? NormalMapState::Animated
                                                                    : NormalMapState::NotTexture;
        if (!entry.error.empty())
            return NormalMapState::Failed;
        if (!entry.frames.empty())
            return NormalMapState::Animated;
        if (material.normal <= 0)
            return NormalMapState::ClassOff;
        return normal_maps ? NormalMapState::Generated : NormalMapState::Disabled;
    }
    SurfaceReport surface_report(const SurfaceManifest& manifest, const WorldMesh& mesh, bool normal_maps,
                                 const std::function<bool(std::string_view)>& exists)
    {
        SurfaceReport report;
        for (std::size_t i = 0; i < mesh.materials.size(); ++i)
        {
            const auto& entry = mesh.materials[i];
            if (!texture_path(entry.path) && !sprite_path(entry.path))
                continue; // The white slot and lightmap pages.
            SurfaceMaterialReport row;
            row.material = static_cast<std::uint32_t>(i);
            row.path = entry.path;
            // SPR materials keep the default class: the renderer resolves only DTX paths.
            const auto found = texture_path(entry.path) ? manifest.lookup(entry.path) : SurfaceResolution{};
            static const SurfaceMaterial fallback;
            row.source = found.source;
            row.values = found.material ? *found.material : fallback;
            row.binding = found.source == SurfaceSource::Rule      ? manifest.rule_pattern(found.index)
                          : found.source == SurfaceSource::Texture ? manifest.override_path(found.index)
                                                                   : std::string{};
            row.normal_map = normal_map_state(entry, row.values, normal_maps);
            row.fullbright_faces = i < mesh.fullbright_faces.size() ? mesh.fullbright_faces[i] : 0;
            ++report.classes[row.values.name];
            report.defaults += row.source == SurfaceSource::Default;
            report.failed += row.normal_map == NormalMapState::Failed;
            report.emissive += row.emissive();
            report.generated += row.normal_map == NormalMapState::Generated;
            report.fullbright_materials += row.fullbright_faces > 0;
            report.fullbright_faces += row.fullbright_faces;
            report.emissive_fullbright += row.emissive() && row.fullbright_faces > 0;
            report.materials.push_back(std::move(row));
        }
        if (exists)
            for (std::size_t i = 0; i < manifest.overrides(); ++i)
                if (!exists(manifest.override_path(i)))
                    report.unbound_overrides.push_back(manifest.override_path(i));
        return report;
    }
} // namespace OpenBlood2
