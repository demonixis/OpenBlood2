#include "brush_fixture.h"
#include "engine/capture.h"
#include "engine/container.h"
#include "engine/runtime.h"
#include "engine/world_model.h"
#include "engine/world_renderer.h"
#include "engine/world_view.h"
#include "game/original/Door.h"
#include "player_fixture.h"
#include <algorithm>
#include <functional>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    void append(WorldModel& to, const WorldModel& from)
    {
        const auto points = to.points.size(), planes = to.planes.size(), surfaces = to.surfaces.size();
        to.points.insert(to.points.end(), from.points.begin(), from.points.end());
        to.planes.insert(to.planes.end(), from.planes.begin(), from.planes.end());
        for (auto surface : from.surfaces)
        {
            surface.plane += planes;
            to.surfaces.push_back(surface);
        }
        for (auto face : from.polygons)
        {
            face.surface += surfaces;
            for (auto& index : face.points)
                index += points;
            to.polygons.push_back(std::move(face));
        }
    }
    WorldModel compound(bool partial = false)
    {
        auto m = test_brush::box("compound", {0, 0, 0}, {4, 4, 4});
        append(m, test_brush::box("", {4, partial ? 1.0f : 0.0f, 4}, {8, partial ? 3.0f : 4.0f, 8}));
        m.translation = {1, 1, 1};
        return m;
    }
    bool geometry(const std::filesystem::path&)
    {
        ContainerShape s;
        std::string error;
        for (bool partial : {false, true})
        {
            auto m = compound(partial);
            CHECK(s.build(m, {}, error));
            CHECK(s.closed_components == 2 && s.shared_edges == 1 && !s.convex);
            CHECK(s.triangles.size() == 24 && s.origin.x == 1);
            CHECK(s.contains({2, 2, 2}) && s.contains({6, 2, 6}) && s.contains({4, 2, 4}));
            CHECK(!s.contains({6, 2, 2}) && !s.overlaps({6, 2, 2}, {.5f, .5f, .5f}));
            CHECK(s.overlaps({4.1f, 2, 2}, {.2f, .2f, .2f}));
            CollisionHit hit;
            CHECK(s.sweep({6, 2, 0}, {6, 2, 2}, {}, hit) == CollisionQuery::Miss);
            CHECK(s.sweep({6, 2, 2}, {6, 2, 10}, {}, hit) == CollisionQuery::Hit);
            CHECK(!hit.start_solid && std::abs(hit.fraction - .25f) < .0001f);
            CHECK(s.sweep({2, 2, 2}, {6, 2, 2}, {}, hit) == CollisionQuery::Hit && hit.start_solid);
            const auto q = rotation_from_euler({.1f, .7f, .2f});
            const DVector dest{100, 20, -50};
            CHECK(s.transform(q, dest) && s.closed_components == 2 && s.shared_edges == 1);
            auto pose = [&](DVector p)
            { return vector_add(dest, rotate_vector(vector_subtract(p, m.translation), q)); };
            CHECK(s.contains(pose({2, 2, 2})) && s.contains(pose({6, 2, 6})) && !s.contains(pose({6, 2, 2})));
            CHECK(s.build(m, q, error) && s.closed_components == 2 && s.shared_edges == 1);
        }
        auto m = compound();
        append(m, test_brush::box("", {8, 4, 8}, {10, 6, 10})); // Vertex contact only.
        append(m, test_brush::box("", {20, 0, 0}, {24, 4, 4})); // Disconnected shell.
        CHECK(s.build(m, {}, error) && s.closed_components == 4 && s.shared_edges == 1);
        CHECK(s.contains({9, 5, 9}) && s.contains({22, 2, 2}) && !s.contains({15, 2, 2}));
        std::cout << "Closed edge/vertex-touching and disconnected components: contents, gap sweeps, overlap, T "
                     "junctions and transforms pass\n";
        return true;
    }
    bool malformed(const std::filesystem::path&)
    {
        ContainerShape s;
        std::string error;
        for (int mutation = 0; mutation < 6; ++mutation)
        {
            auto m = compound();
            if (mutation == 0)
                m.polygons.pop_back();
            if (mutation == 1)
                m.polygons.push_back(m.polygons[0]);
            if (mutation == 2)
                append(m, test_brush::box("", {0, 0, 0}, {4, 4, 4}));
            if (mutation == 3)
            {
                auto& p = m.planes.back();
                p.normal = vector_scale(p.normal, -1);
                p.distance = -p.distance;
            }
            if (mutation == 4)
            {
                m = test_brush::box("faces", {0, 0, 0}, {4, 4, 4});
                append(m, test_brush::box("", {4, 0, 0}, {8, 4, 4}));
            }
            if (mutation == 5)
                m.points.resize(4097);
            CHECK(!s.build(m, {}, error) && !error.empty());
            CHECK(s.triangles.empty() && s.closed_components == 0 && s.shared_edges == 0);
            CHECK(!s.contains({2, 2, 2}));
        }
        std::cout << "Open, duplicate, oppositely oriented, face-coincident and over-limit shells reject without "
                     "partial output\n";
        return true;
    }
    bool ground(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        g.world.models.push_back(compound());
        for (bool box : {false, true})
        {
            auto door = g.objects.create(g.objects.find_class("Door"), {}, Creation::World,
                                         {{"Name", std::string("compound")}, {"BoxPhysics", box}});
            CHECK(door);
            for (auto p : {DVector{6, 10, 6}, DVector{6, 10, 2}})
            {
                auto h = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                          {{"Pos", p}, {"Flags", std::int32_t(FLAG_SOLID)}});
                DVector dims{.5f, .5f, .5f};
                CHECK(h && g.server.SetObjectDims(h, &dims) == DE_OK);
                CHECK(g.server.MoveObjectToGround(h) == DE_OK);
                const float expected = (box || p.z == 6) ? 4.5f : .5f;
                CHECK(std::abs(g.objects.get(h)->state.m_Pos.y - expected) < .01f);
                CHECK(g.objects.remove(h) && g.objects.finish_frame());
            }
            CHECK(g.objects.remove(door) && g.objects.finish_frame());
        }
        CHECK(g.unload_world() && g.server.message_count() == 0 && g.server.string_count() == 0);
        std::cout << "Ground rays use actual polygon pieces/gaps and authored box bounds independently of pivot\n";
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        g.world.models.push_back(compound());
        auto h = g.objects.create(g.objects.find_class("Door"), {}, Creation::World,
                                  {{"Name", std::string("compound")}, {"BoxPhysics", false}, {"Visible", false}});
        auto* r = g.objects.get(h);
        CHECK(r && r->world_model && r->world_model->closed_components == 2);
        const auto id = r->id;
        auto retained = r->world_model;
        CollisionHit hit;
        CHECK(sweep_object(*r, {6, 2, 0}, {6, 2, 2}, {}, hit) == CollisionQuery::Miss);
        CHECK(sweep_object(*r, {6, 2, 2}, {6, 2, 10}, {}, hit) == CollisionQuery::Hit);
        CHECK(g.server.MoveWorldModel(h, {21, 1, 1}) == MoverResult::Moved);
        CHECK(r->state.m_Pos.x == 21 && r->world_model->closed_components == 2);
        CHECK(sweep_object(*r, {26, 2, 2}, {26, 2, 10}, {}, hit) == CollisionQuery::Hit);
        CHECK(sweep_object(*r, {6, 2, 2}, {6, 2, 10}, {}, hit) == CollisionQuery::Miss);
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && !g.objects.get(id));
        CHECK(retained->contains({2, 2, 2})); // Value-owned geometry survives its original source object.
        CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        CHECK(retained->closed_components == 2 && retained->contains({6, 2, 6}));
        return true;
    }
    bool render(Runtime& runtime, WorldRenderer& renderer, const WorldMesh& mesh, std::span<const WorldDraw> draws,
                const ContainerShape& shape, const std::string& label, bool visible)
    {
        Capture capture;
        std::string error;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240));
        const auto center = vector_scale(vector_add(shape.min, shape.max), .5f);
        const auto extent = vector_subtract(shape.max, shape.min);
        const auto eye = vector_subtract(center, {0, 0, 1.5f * std::max({extent.x, extent.y, extent.z}) + 64});
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd && renderer.draw(cmd, capture.texture, mesh, draws, eye, {}, false, error));
        CHECK(renderer.draw(cmd, capture.texture, mesh, draws, eye, {}, false, error, DrawLoad::Load, nullptr,
                            WorldPass::Translucent));
        auto* copy = SDL_BeginGPUCopyPass(cmd);
        CHECK(copy);
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 320;
        region.h = 240;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 320;
        target.rows_per_layer = 240;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(cmd) && SDL_WaitForGPUIdle(runtime.gpu));
        const auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        unsigned changed = 0;
        for (unsigned i = 0; i < 320 * 240; ++i)
            changed += pixels[i * 4] != 6 || pixels[i * 4 + 1] != 9 || pixels[i * 4 + 2] != 13;
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        if (visible)
            CHECK(capture.save((label + ".png").c_str()));
        std::cout << label << ": " << changed << " non-background pixels\n";
        CHECK(visible ? changed > 100 : changed == 0);
        return true;
    }
    bool private_data(const std::filesystem::path& root, bool gpu = false)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        Runtime runtime;
        if (gpu)
            CHECK(runtime.initialize(true, error));
        WorldRenderer renderer(runtime.gpu);
        ObjectId previous{};
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", error));
            unsigned created = 0, unknown = 0, failed = 0, transferred = 0;
            for (auto& e : g.entries)
            {
                created += e.state == EntryState::Created;
                unknown += e.state == EntryState::Unsupported;
                failed += e.state == EntryState::Failed;
                transferred += e.state == EntryState::WorldSound;
            }
            std::cout << "CityHub: " << created << " created / " << transferred << " sounds / " << unknown
                      << " unsupported / " << failed << " failed\n";
            CHECK(created == 421 && transferred == 31 && unknown == 31 && failed == 2);
            WorldMesh mesh;
            CHECK(mesh.build(g.world, error, &g.resources));
            if (gpu)
                CHECK(renderer.prepare(mesh, g.world_revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
            for (const auto& name : {"startfences", "Buildings"})
            {
                ObjectRecord* r = nullptr;
                for (auto id : g.objects.snapshot())
                    if (std::string_view(g.objects.get(id)->state.m_Name) == name)
                        r = g.objects.get(id);
                CHECK(r && r->life == Life::Alive && r->world_model);
                const auto& s = *r->world_model;
                CHECK(s.closed_components == (std::string_view(name) == "startfences" ? 3u : 2u) &&
                      s.shared_edges == 2);
                CollisionHit hit;
                if (std::string_view(name) == "startfences")
                {
                    CHECK(!(r->state.m_Flags & FLAG_BOXPHYSICS));
                    CHECK(s.contains({-3376, 1280, -400}) && s.contains({-3000, 1280, -816}) &&
                          s.contains({-3000, 1280, 48}));
                    CHECK(!s.contains({-3000, 1280, -400}));
                    CHECK(sweep_object(*r, {-3000, 1280, -700}, {-3000, 1280, 0}, {}, hit) == CollisionQuery::Miss);
                    CHECK(sweep_object(*r, {-3000, 1280, 0}, {-3000, 1280, 100}, {}, hit) == CollisionQuery::Hit);
                    CHECK(std::abs(hit.fraction - .32f) < .0001f && !hit.start_solid);
                }
                else
                {
                    CHECK(r->state.m_Flags & FLAG_BOXPHYSICS);
                    const auto center = object_bounds_center(*r);
                    CHECK(sweep_object(*r, center, center, {}, hit) == CollisionQuery::Hit && hit.start_solid);
                }
                CHECK(r->id != previous);
                previous = r->id;
                std::vector<WorldDraw> draws;
                WorldViewStats stats;
                CHECK(world_draws(mesh, g.objects, draws, stats, error));
                auto drawn = [&]
                {
                    return std::any_of(draws.begin(), draws.end(),
                                       [&](auto& d) { return mesh.ranges[d.range].name == name; });
                };
                CHECK(drawn());
                auto draw_gpu = [&](const char* state, bool visible)
                {
                    if (!gpu)
                        return true;
                    std::vector<WorldDraw> selected;
                    for (auto& draw : draws)
                        if (mesh.ranges[draw.range].name == name)
                            selected.push_back(draw);
                    return render(runtime, renderer, mesh, selected, s,
                                  std::string("compound-") + name + "-" + std::to_string(cycle) + "-" + state, visible);
                };
                CHECK(draw_gpu("visible", true));
                const auto flags = r->state.m_Flags;
                r->state.m_Flags &= ~FLAG_VISIBLE;
                CHECK(world_draws(mesh, g.objects, draws, stats, error) && !drawn());
                CHECK(draw_gpu("absent", false));
                CHECK(s.closed_components > 1); // Hidden inspection preserves topology.
                r->state.m_Flags = flags;
                std::cout << name << ": " << s.closed_components << " closed components / " << s.shared_edges
                          << " shared edges / " << s.triangles.size() << " triangles\n";
                CHECK(g.objects.remove(r->handle));
                CHECK(world_draws(mesh, g.objects, draws, stats, error) && !drawn());
                CHECK(draw_gpu("absent", false));
                CHECK(g.objects.finish_frame());
            }
            renderer.clear_geometry();
            CHECK(g.unload_world() && !g.objects.get(previous));
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"geometry", geometry},
        {"malformed", malformed},
        {"lifetime", lifetime},
        {"ground", ground},
        {"private", [](auto& root) { return private_data(root); }},
        {"gpu", [](auto& root) { return private_data(root, true); }}};
    auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
