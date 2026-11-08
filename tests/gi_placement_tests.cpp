// GI lot G4: placement files, the light-probe lattice, reflection probes and the PLAC section
// (gi_contracts.md). The private case places probes in every mounted world and prints the audit.
#include "engine/gi_placement.h"
#include "engine/config.h"
#include "engine/resources.h"
#include "scratch_dir.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <random>

using namespace OpenBlood2;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace
{
    // An axis-aligned box with solid faces and a BSP chain. A room (primary convention) is empty
    // inside and solid outside; a brush (world-model convention) is solid inside.
    WorldModel box_model(DVector lo, DVector hi, bool room, std::string name)
    {
        WorldModel m;
        m.name = std::move(name);
        for (int i = 0; i < 8; ++i)
            m.points.push_back({i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z});
        const std::array<std::array<std::uint16_t, 4>, 6> faces{
            {{0, 2, 6, 4}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 5, 7, 6}}};
        const std::array<DVector, 6> inward{{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
        const std::array<float, 6> at{lo.x, hi.x, lo.y, hi.y, lo.z, hi.z};
        for (std::uint32_t f = 0; f < 6; ++f)
        {
            const float sign = room ? 1.f : -1.f;
            const DVector n{inward[f].x * sign, inward[f].y * sign, inward[f].z * sign};
            m.planes.push_back({n, n.x * (f < 2 ? at[f] : 0) + n.y * (f >= 2 && f < 4 ? at[f] : 0) +
                                       n.z * (f >= 4 ? at[f] : 0)});
            Surface s;
            s.flags = 1; // SURF_SOLID.
            s.plane = f;
            m.surfaces.push_back(s);
            Polygon p;
            p.surface = f;
            p.points.assign(faces[f].begin(), faces[f].end());
            m.polygons.push_back(p);
            BspNode node;
            node.plane = f;
            node.polygon = f;
            const std::int32_t next = f < 5 ? std::int32_t(f + 1) : (room ? BspNode::inside : BspNode::outside);
            // sides[0]: back (below -0.0001), sides[1]: front.
            node.sides = room ? std::array<std::int32_t, 2>{BspNode::outside, next}
                              : std::array<std::int32_t, 2>{next, BspNode::inside};
            m.bsp_nodes.push_back(node);
        }
        m.root_node = 0;
        return m;
    }
    WorldObject named(std::string class_name, std::string name)
    {
        WorldObject o;
        o.class_name = std::move(class_name);
        o.properties.push_back({"Name", 0, 0, std::move(name)});
        return o;
    }
    // A 1024 x 256 x 1024 room (Y up), optionally with a door block inside.
    struct Scene
    {
        World world;
        CollisionWorld collision;
        GiPlacementScene scene;
        bool build(bool door)
        {
            world.models.push_back(box_model({0, 0, 0}, {1024, 256, 1024}, true, "PhysicsBSP"));
            if (door)
            {
                world.models.push_back(box_model({384, 0, 384}, {640, 256, 640}, false, "Door1"));
                world.objects.push_back(named("Door", "Door1"));
            }
            std::string error;
            return collision.build(world.models[0], error) && scene.build(world, collision, error);
        }
    };
    GiPlacement placement_for_test()
    {
        Sha256Digest digest{};
        digest[0] = 0xab;
        return default_gi_placement("worlds/test.dat", digest);
    }
    bool has_probe_at(const GiLightLattice& lattice, DVector p, bool relocated)
    {
        for (const auto& probe : lattice.probes)
            if (probe.position.x == p.x && probe.position.y == p.y && probe.position.z == p.z)
                return probe.relocated == relocated;
        return false;
    }
    std::uint64_t accounted(const GiPlacementStats& s)
    {
        return s.probes + s.excluded + s.solid + s.unknown + s.clearance + s.world_model;
    }

    bool lattice()
    {
        Scene s;
        CHECK(s.build(false));
        auto placement = placement_for_test();
        GiLightLattice lattice;
        GiPlacementStats stats;
        std::vector<GiRejectedNode> rejected;
        std::string error;
        CHECK(resolve_light_probes(s.scene, placement, lattice, stats, &rejected, error));
        CHECK(lattice.spacing == 128 && lattice.dims == (std::array<std::uint32_t, 3>{9, 3, 9}));
        CHECK(stats.nodes == 243 && accounted(stats) == stats.nodes && stats.over_budget.empty());
        // Every node holds a probe: the 49 inner ones at the node, the others moved off the walls.
        CHECK(stats.probes == 243 && stats.relocated == 194 && rejected.empty());
        CHECK(has_probe_at(lattice, {512, 128, 512}, false));
        CHECK(has_probe_at(lattice, {32, 128, 512}, true));  // Wall: a quarter spacing inward.
        CHECK(has_probe_at(lattice, {512, 32, 512}, true));  // Floor.
        CHECK(has_probe_at(lattice, {32, 32, 32}, true));    // Corner: the diagonal neighbour.
        for (std::size_t i = 1; i < lattice.probes.size(); ++i)
            CHECK(lattice.probes[i].node > lattice.probes[i - 1].node);
        // Deterministic: a second run gives identical sections.
        std::vector<std::uint8_t> a, b;
        GiLightLattice again;
        CHECK(resolve_light_probes(s.scene, placement, again, stats, nullptr, error));
        CHECK(encode_gi_placement_section(lattice, {}, a, error) && encode_gi_placement_section(again, {}, b, error));
        CHECK(a == b);
        // Volumes: an exclude box removes nodes; include boxes restrict the lattice to their union.
        placement.volumes.push_back({false, {{-1, -1, -1}, {200, 300, 1100}}}); // x = 0 and 128 planes.
        CHECK(resolve_light_probes(s.scene, placement, lattice, stats, nullptr, error));
        CHECK(stats.excluded == 54 && stats.probes == 189 && accounted(stats) == 243);
        placement.volumes = {{true, {{400, -1, 400}, {700, 300, 700}}}}; // x, z in {512, 640}.
        CHECK(resolve_light_probes(s.scene, placement, lattice, stats, nullptr, error));
        CHECK(stats.probes == 12 && stats.excluded == 231);
        for (const auto& probe : lattice.probes)
            CHECK(probe.position.x >= 400 && probe.position.x <= 700);
        // Clearance 64: the inner nodes next to the walls (128 away) stay; none closer.
        placement.volumes.clear();
        placement.clearance = 64;
        CHECK(resolve_light_probes(s.scene, placement, lattice, stats, nullptr, error));
        CHECK(has_probe_at(lattice, {128, 128, 128}, false) && !has_probe_at(lattice, {32, 128, 512}, true));
        CHECK(stats.probes == 49 && stats.clearance == 194); // 3/8 spacing (48) cannot leave a 64 margin.
        CHECK(stats.relocated == 0);
        // Budget ladder: 243 > 100 at 128; 98 nodes at 160 fit.
        placement.clearance = 8;
        placement.budget = 100;
        CHECK(resolve_light_probes(s.scene, placement, lattice, stats, nullptr, error));
        CHECK(lattice.spacing == 160 && stats.probes == 98);
        CHECK(stats.over_budget.size() == 1 &&
              stats.over_budget[0] == (std::pair<std::uint32_t, std::uint64_t>{128, 101}));
        placement.budget = 1;
        CHECK(!resolve_light_probes(s.scene, placement, lattice, stats, nullptr, error));
        CHECK(error.find("budget of 1") != std::string::npos);
        // Invalid placements are refused before any work.
        placement.budget = 16384;
        placement.spacing = 100;
        CHECK(!resolve_light_probes(s.scene, placement, lattice, stats, nullptr, error));
        return true;
    }

    bool world_models()
    {
        Scene s;
        CHECK(s.build(true));
        CHECK(s.scene.classify({512, 128, 512}, 8) == GiNodeRejection::WorldModel);
        CHECK(s.scene.classify({380, 128, 512}, 8) == GiNodeRejection::WorldModel); // Within the clearance.
        CHECK(s.scene.classify({370, 128, 512}, 8) == GiNodeRejection::None);
        CHECK(s.scene.classify({512, 4, 200}, 8) == GiNodeRejection::Clearance);
        CHECK(s.scene.classify({512, -10, 512}, 8) == GiNodeRejection::Solid);
        CHECK(s.scene.classify({float(NAN), 0, 0}, 8) == GiNodeRejection::Unknown);
        auto placement = placement_for_test();
        GiLightLattice lattice;
        GiPlacementStats stats;
        std::vector<GiRejectedNode> rejected;
        std::string error;
        CHECK(resolve_light_probes(s.scene, placement, lattice, stats, &rejected, error));
        // The node in the door's middle cannot leave it; the floor and ceiling nodes under and over it
        // fail clearance first. Nodes on the door faces move out of it.
        CHECK(stats.world_model == 1 && stats.clearance == 2 && accounted(stats) == stats.nodes);
        CHECK(rejected.size() == 3 && rejected[1].reason == GiNodeRejection::WorldModel);
        CHECK(rejected[1].position.x == 512 && rejected[1].position.y == 128 && rejected[1].position.z == 512);
        CHECK(has_probe_at(lattice, {352, 128, 512}, true)); // Off the door's -x face.
        // Containers and sky never block; a door does.
        s.world.objects[0].class_name = "Water";
        CHECK(s.scene.build(s.world, s.collision, error));
        CHECK(s.scene.classify({512, 128, 512}, 8) == GiNodeRejection::None);
        s.world.objects[0].class_name = "DemoSkyWorldModel";
        CHECK(s.scene.build(s.world, s.collision, error));
        CHECK(s.scene.classify({512, 128, 512}, 8) == GiNodeRejection::None);
        s.world.objects[0].class_name = "Tchernotronic"; // Binds nothing: the model is Unbound and blocks.
        CHECK(s.scene.build(s.world, s.collision, error));
        CHECK(s.scene.classify({512, 128, 512}, 8) == GiNodeRejection::WorldModel);
        // A world without a primary tree is refused.
        World empty;
        CHECK(!GiPlacementScene{}.build(empty, s.collision, error));
        return true;
    }

    bool reflections()
    {
        Scene s;
        CHECK(s.build(false));
        GiBox box;
        bool open = true;
        CHECK(s.scene.fit_room({100, 50, 900}, box, open) && !open);
        CHECK(box.min.x == 0 && box.max.x == 1024 && box.min.y == 0 && box.max.y == 256 && box.max.z == 1024);
        auto placement = placement_for_test();
        GiLightLattice lattice;
        GiPlacementStats stats;
        std::string error;
        CHECK(resolve_light_probes(s.scene, placement, lattice, stats, nullptr, error));
        CHECK(place_reflection_probes(s.scene, lattice, placement, stats, error));
        // Every candidate fits the same room: one probe at its centre, the rest overlap it.
        CHECK(placement.reflections.size() == 1 && stats.automatic == 1 && stats.manual == 0 && stats.open == 0);
        CHECK(stats.room_candidates == 243 && stats.overlapping == 242 && stats.small_rooms == 0);
        const auto& probe = placement.reflections[0];
        CHECK(probe.automatic && probe.box_projection && probe.resolution == 64 && probe.priority == 0);
        CHECK(probe.capture.x == 512 && probe.capture.y == 128 && probe.capture.z == 512);
        CHECK(probe.influence.min.x == 0 && probe.influence.max.y == 256 && probe.blend_distance == 32);
        // Manual probes are kept first and suppress automatic ones they cover.
        GiReflectionProbe manual;
        CHECK(fit_reflection_probe(s.scene, {200, 100, 200}, manual, error) && !manual.automatic);
        manual.priority = 5;
        placement.reflections.insert(placement.reflections.begin(), manual);
        CHECK(place_reflection_probes(s.scene, lattice, placement, stats, error));
        CHECK(placement.reflections.size() == 1 && placement.reflections[0].priority == 5 && stats.manual == 1);
        // A zero cap keeps nothing automatic.
        placement.reflections.clear();
        placement.reflection_cap = 0;
        CHECK(place_reflection_probes(s.scene, lattice, placement, stats, error));
        CHECK(placement.reflections.empty() && stats.capped == 1);
        // Doors end a room; a narrow room gets no automatic probe.
        Scene d;
        CHECK(d.build(true));
        CHECK(d.scene.fit_room({128, 128, 512}, box, open) && box.max.x == 384 && box.min.x == 0);
        Scene narrow;
        narrow.world.models.push_back(box_model({0, 0, 0}, {128, 256, 1024}, true, "PhysicsBSP"));
        CHECK(narrow.collision.build(narrow.world.models[0], error) &&
              narrow.scene.build(narrow.world, narrow.collision, error));
        placement.reflection_cap = 42;
        CHECK(resolve_light_probes(narrow.scene, placement, lattice, stats, nullptr, error));
        CHECK(place_reflection_probes(narrow.scene, lattice, placement, stats, error));
        CHECK(placement.reflections.empty() && stats.small_rooms == stats.room_candidates);
        // An open side (no wall up to the bounds) disables box projection.
        Scene roofless;
        roofless.world.models.push_back(box_model({0, 0, 0}, {1024, 256, 1024}, true, "PhysicsBSP"));
        roofless.world.models[0].surfaces[3].flags = 0; // The ceiling is not collision geometry.
        CHECK(roofless.collision.build(roofless.world.models[0], error) &&
              roofless.scene.build(roofless.world, roofless.collision, error));
        CHECK(roofless.scene.fit_room({512, 128, 512}, box, open) && open && box.max.y == 256);
        CHECK(!fit_reflection_probe(s.scene, {512, -50, 512}, manual, error)); // In the void: no room.
        return true;
    }

    std::string sample_text()
    {
        auto p = placement_for_test();
        p.volumes = {{true, {{-1.5f, 0, 0}, {10, 20.25f, 30}}}, {false, {{1, 2, 3}, {4, 5, 6}}}};
        p.excluded_models = {"Door01", "Glass_B"};
        GiReflectionProbe r;
        r.automatic = false;
        r.capture = {5, 5, 5};
        r.influence = {{0, 0, 0}, {10, 10, 10}};
        r.projection = {{1, 1, 1}, {9, 9, 9}};
        r.blend_distance = 2.5f;
        r.priority = -3;
        r.box_projection = false;
        r.resolution = 128;
        p.reflections = {r, r};
        p.reflections[1].automatic = true;
        std::string text, error;
        write_gi_placement(p, text, error);
        return text;
    }
    bool json()
    {
        std::string text, again, error;
        auto p = placement_for_test();
        CHECK(write_gi_placement(p, text, error));
        GiPlacement read;
        CHECK(read_gi_placement(text, read, error) && write_gi_placement(read, again, error) && again == text);
        CHECK(text.find("\"volumes\": []") != std::string::npos && text.find("\"probes\": []") != std::string::npos);
        text = sample_text();
        CHECK(!text.empty() && read_gi_placement(text, read, error));
        CHECK(read.volumes.size() == 2 && read.volumes[0].include && read.volumes[0].box.min.x == -1.5f);
        CHECK(read.reflections.size() == 2 && !read.reflections[0].automatic && read.reflections[1].automatic);
        CHECK(read.reflections[0].resolution == 128 && read.reflections[0].priority == -3);
        CHECK(read.excluded_models == (std::vector<std::string>{"Door01", "Glass_B"}));
        CHECK(write_gi_placement(read, again, error) && again == text);
        CHECK(text.find("\"version\": 2") != std::string::npos);
        CHECK(text.find("\"excluded_models\": [\"Door01\", \"Glass_B\"],") != std::string::npos);
        // Version 1 (lot G4) has no excluded_models and reads as an empty list; saving writes version 2.
        {
            auto v1 = text;
            v1.replace(v1.find("\"version\": 2"), 12, "\"version\": 1");
            const std::string line = "  \"excluded_models\": [\"Door01\", \"Glass_B\"],\n";
            v1.erase(v1.find(line), line.size());
            GiPlacement old;
            CHECK(read_gi_placement(v1, old, error) && old.excluded_models.empty() && old.volumes.size() == 2);
            auto with_key = v1;
            with_key.replace(with_key.find("\"version\": 1"), 12, "\"version\": 2");
            CHECK(!read_gi_placement(with_key, old, error) &&
                  error.find("missing key 'excluded_models'") != std::string::npos);
            CHECK(!read_gi_placement(text.substr(0, text.find("\"version\": 2")) + "\"version\": 1" +
                                         text.substr(text.find("\"version\": 2") + 12),
                                     old, error) &&
                  error.find("unknown key 'excluded_models'") != std::string::npos);
        }
        // Formatting does not matter to the reader or the hash; values do.
        std::string compact;
        for (const char c : text)
            if (c != '\n' && c != ' ')
                compact += c;
        Sha256Digest h1{}, h2{}, h3{};
        CHECK(read_gi_placement(compact, p, error) && gi_placement_hash(read, h1, error) &&
              gi_placement_hash(p, h2, error) && h1 == h2);
        p.spacing = 136;
        CHECK(gi_placement_hash(p, h3, error) && h3 != h1);
        // Refusals, each named.
        const auto refuse = [&](std::string_view from, std::string_view to, std::string_view why)
        {
            auto bad = text;
            const auto at = bad.find(from);
            if (at == std::string::npos)
                return false;
            bad.replace(at, from.size(), to);
            GiPlacement untouched = read;
            std::string reason;
            if (read_gi_placement(bad, untouched, reason) || reason.find(why) == std::string::npos)
            {
                std::cerr << "accepted or wrong reason for " << to << ": " << reason << '\n';
                return false;
            }
            return untouched.spacing == read.spacing && untouched.reflections.size() == read.reflections.size();
        };
        CHECK(refuse("\"version\": 2", "\"version\": 3", "unsupported version"));
        CHECK(refuse("[\"Door01\", \"Glass_B\"]", "[\"Door01\", \"door01\"]", "listed twice"));
        CHECK(refuse("[\"Door01\", \"Glass_B\"]", "[\"Door01\", \"\"]", "excluded model name"));
        CHECK(refuse("[\"Door01\", \"Glass_B\"]", "[\"Door01\", 3]", "'excluded_models'"));
        CHECK(refuse("[\"Door01\", \"Glass_B\"]", "{}", "'excluded_models'"));
        CHECK(refuse("openblood2-gi-placement", "other", "'format'"));
        CHECK(refuse("\"world\": \"worlds/test.dat\"", "\"world\": \"Worlds/Test.dat\"", "'world'"));
        CHECK(refuse("\"world\": \"worlds/test.dat\"", "\"world\": \"worlds/test.txt\"", "'world'"));
        CHECK(refuse("\"dat_sha256\": \"ab", "\"dat_sha256\": \"AB", "'dat_sha256'"));
        CHECK(refuse("\"spacing\": 128", "\"spacing\": 100", "spacing"));
        CHECK(refuse("\"spacing\": 128", "\"spacing\": 16", "'spacing'"));
        CHECK(refuse("\"spacing\": 128", "\"spacing\": 128.5", "'spacing'"));
        CHECK(refuse("\"budget\": 16384", "\"budget\": 0", "'budget'"));
        CHECK(refuse("\"budget\": 16384", "\"budget\": 262145", "'budget'"));
        CHECK(refuse("\"clearance\": 8", "\"clearance\": 65", "'clearance'"));
        CHECK(refuse("\"mode\": \"include\"", "\"mode\": \"both\"", "'mode'"));
        CHECK(refuse("\"max\": [10, 20.25, 30]", "\"max\": [-10, 20.25, 30]", "volume is empty"));
        CHECK(refuse("\"max\": [10, 20.25, 30]", "\"max\": [10, 20.25, 3e6]", "'max'"));
        CHECK(refuse("\"cap\": 42", "\"cap\": 43", "'cap'"));
        CHECK(refuse("\"cap\": 42", "\"cap\": 1", "more reflection probes than the cap"));
        CHECK(refuse("\"capture\": [5, 5, 5]", "\"capture\": [0.5, 5, 5]", "outside its projection box"));
        CHECK(refuse("\"resolution\": 128", "\"resolution\": 48", "resolution"));
        CHECK(refuse("\"resolution\": 128", "\"resolution\": 512", "'resolution'"));
        CHECK(refuse("\"priority\": -3", "\"priority\": 1001", "'priority'"));
        CHECK(refuse("\"blend_distance\": 2.5", "\"blend_distance\": -1", "'blend_distance'"));
        CHECK(refuse("\"auto\": false", "\"auto\": 0", "'auto'"));
        CHECK(refuse("\"budget\": 16384", "\"budget\": 16384, \"extra\": 1", "unknown key 'extra'"));
        CHECK(refuse("\"budget\": 16384,", "", "missing key 'budget'"));
        CHECK(refuse("\"budget\": 16384", "\"budget\": 16384, \"budget\": 1", "duplicate key"));
        CHECK(refuse("\"capture\": [5, 5, 5]", "\"capture\": [5, 5]", "'capture'"));
        CHECK(!read_gi_placement(text + "x", read, error) && error.find("trailing data") != std::string::npos);
        CHECK(!read_gi_placement(std::string(300 * 1024, ' '), read, error));
        // The writer refuses what the reader refuses.
        auto bad = placement_for_test();
        bad.reflections.resize(1);
        CHECK(!write_gi_placement(bad, text, error) && error.find("Invalid GI placement") == 0);
        // Fuzz: every mutation is refused or rewrites canonically.
        const auto base = sample_text();
        std::mt19937 rng(12345);
        for (int i = 0; i < 3000; ++i)
        {
            auto mutated = base;
            const int edits = 1 + int(rng() % 3);
            for (int e = 0; e < edits; ++e)
            {
                const auto at = rng() % mutated.size();
                switch (rng() % 3)
                {
                case 0: mutated[at] = char(rng() % 128); break;
                case 1: mutated.erase(at, 1); break;
                default: mutated.insert(at, 1, "0123456789.,-[]{}\"e"[rng() % 19]); break;
                }
            }
            GiPlacement parsed;
            std::string reason, canonical, reparsed_text;
            if (!read_gi_placement(mutated, parsed, reason))
                continue;
            GiPlacement reparsed;
            CHECK(write_gi_placement(parsed, canonical, reason) && read_gi_placement(canonical, reparsed, reason) &&
                  write_gi_placement(reparsed, reparsed_text, reason) && canonical == reparsed_text);
        }
        return true;
    }

    bool files(const std::filesystem::path& scratch)
    {
        CHECK(Testing::reset_scratch_dir(scratch));
        std::string name, error, detail;
        CHECK(gi_placement_name("Worlds\\01_SubwayA.dat", name) && name == "worlds/01_subwaya.json");
        CHECK(!gi_placement_name("worlds/readme.txt", name) && !gi_placement_name("", name));
        std::filesystem::path directory;
        CHECK(gi_placement_directory(scratch, directory, error) && directory == scratch);
        CHECK(gi_placement_directory({}, directory, error) && directory.filename() == "placement" &&
              directory.parent_path().filename() == "gi");
        const auto placement = placement_for_test();
        GiPlacement loaded;
        CHECK(load_gi_placement(scratch, "worlds/test.dat", placement.world, loaded, detail) ==
              GiPlacementStatus::Missing);
        CHECK(save_gi_placement(scratch, placement, error));
        CHECK(load_gi_placement(scratch, "Worlds\\TEST.dat", placement.world, loaded, detail) ==
              GiPlacementStatus::Ready);
        CHECK(loaded.world_path == "worlds/test.dat" && loaded.spacing == 128);
        auto other = placement.world;
        other[1] = 1;
        CHECK(load_gi_placement(scratch, "worlds/test.dat", other, loaded, detail) == GiPlacementStatus::Foreign);
        CHECK(detail.starts_with("Made for DAT ab00"));
        // A file whose content names another world.
        auto moved = placement;
        moved.world_path = "worlds/other.dat";
        std::string text;
        CHECK(write_gi_placement(moved, text, error));
        {
            std::ofstream(scratch / "worlds" / "test.json", std::ios::binary) << text;
        }
        CHECK(load_gi_placement(scratch, "worlds/test.dat", placement.world, loaded, detail) ==
              GiPlacementStatus::Foreign);
        CHECK(detail == "Made for worlds/other.dat");
        {
            std::ofstream(scratch / "worlds" / "test.json", std::ios::binary) << "{";
        }
        CHECK(load_gi_placement(scratch, "worlds/test.dat", placement.world, loaded, detail) ==
              GiPlacementStatus::Failed);
        // A refused save keeps the previous file.
        CHECK(save_gi_placement(scratch, placement, error));
        auto invalid = placement;
        invalid.spacing = 7;
        CHECK(!save_gi_placement(scratch, invalid, error));
        CHECK(load_gi_placement(scratch, "worlds/test.dat", placement.world, loaded, detail) ==
              GiPlacementStatus::Ready);
        CHECK(std::string_view(gi_placement_status_name(GiPlacementStatus::Foreign)) == "foreign");
        return Testing::remove_scratch_dir(scratch);
    }

    bool section()
    {
        Scene s;
        CHECK(s.build(true));
        auto placement = placement_for_test();
        GiLightLattice lattice;
        GiPlacementStats stats;
        std::string error;
        CHECK(resolve_light_probes(s.scene, placement, lattice, stats, nullptr, error));
        CHECK(place_reflection_probes(s.scene, lattice, placement, stats, error) && !placement.reflections.empty());
        std::vector<std::uint8_t> bytes, again;
        CHECK(encode_gi_placement_section(lattice, placement.reflections, bytes, error));
        CHECK(bytes.size() == 4 + 4 + 12 + 12 + 4 + 20 * lattice.probes.size() + 4 + 76 * placement.reflections.size());
        GiLightLattice decoded;
        std::vector<GiReflectionProbe> reflections;
        CHECK(decode_gi_placement_section(bytes, decoded, reflections, error));
        CHECK(decoded.probes.size() == lattice.probes.size() && reflections.size() == placement.reflections.size());
        CHECK(encode_gi_placement_section(decoded, reflections, again, error) && again == bytes);
        // Every truncation and a trailing byte are refused, leaving the outputs untouched.
        for (std::size_t n = 0; n < bytes.size(); n += (n < 64 ? 1 : 97))
        {
            CHECK(!decode_gi_placement_section(std::span(bytes).first(n), decoded, reflections, error));
            CHECK(decoded.probes.size() == lattice.probes.size());
        }
        auto longer = bytes;
        longer.push_back(0);
        CHECK(!decode_gi_placement_section(longer, decoded, reflections, error) &&
              error.find("trailing") != std::string::npos);
        const auto patched = [&](std::size_t at, std::uint32_t value)
        {
            auto b = bytes;
            for (int i = 0; i < 4; ++i)
                b[at + std::size_t(i)] = std::uint8_t(value >> (8 * i));
            return b;
        };
        const std::size_t first = 36; // First probe record.
        CHECK(!decode_gi_placement_section(patched(0, 2), decoded, reflections, error));            // Version.
        CHECK(!decode_gi_placement_section(patched(4, 12), decoded, reflections, error));           // Spacing.
        CHECK(!decode_gi_placement_section(patched(first + 16, 2), decoded, reflections, error));   // Flags.
        CHECK(!decode_gi_placement_section(patched(first + 20, lattice.probes[0].node), decoded, reflections,
                                           error)); // Nodes not ascending.
        // An unrelocated probe off its node, and a relocated probe beyond 3/8 of the spacing.
        auto off = lattice;
        off.probes[0].relocated = false;
        off.probes[0].position.x += 1;
        CHECK(!encode_gi_placement_section(off, {}, again, error));
        off.probes[0].relocated = true;
        off.probes[0].position.x += 60;
        CHECK(!encode_gi_placement_section(off, {}, again, error));
        auto bad = placement.reflections;
        bad[0].resolution = 100;
        CHECK(!encode_gi_placement_section(lattice, bad, again, error));
        std::vector<GiReflectionProbe> many(43, placement.reflections[0]);
        CHECK(!encode_gi_placement_section(lattice, many, again, error));
        return true;
    }

    // Private: places probes in every mounted world with the defaults and prints the audit table.
    bool retail(const std::filesystem::path& root)
    {
        Config config;
        config.data_dir = root;
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        std::size_t worlds = 0, single = 0, total_probes = 0, total_reflections = 0;
        double total_ms = 0;
        for (const auto& [path, resource] : resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            const bool is_single = path.find('/', 7) == std::string::npos;
            std::vector<std::uint8_t> bytes;
            World world;
            CollisionWorld collision;
            GiPlacementScene scene;
            if (!resources.read(path, bytes, error) || !read_world(bytes, world, error) ||
                !collision.build(world.models.at(0), error) || !scene.build(world, collision, error))
            {
                std::cerr << path << ": " << error << '\n';
                CHECK(!is_single);
                continue;
            }
            auto placement = default_gi_placement(path, sha256(bytes));
            // Unbounded count at the base spacing, for the Gate A table.
            auto unbounded = placement;
            unbounded.budget = gi_placement_limits::max_budget;
            GiLightLattice lattice;
            GiPlacementStats base, stats;
            CHECK(resolve_light_probes(scene, unbounded, lattice, base, nullptr, error));
            const auto start = std::chrono::steady_clock::now();
            const bool ok = resolve_light_probes(scene, placement, lattice, stats, nullptr, error) &&
                            place_reflection_probes(scene, lattice, placement, stats, error);
            const double ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (!ok)
            {
                std::cerr << path << ": " << error << '\n';
                CHECK(!is_single);
                continue;
            }
            CHECK(accounted(stats) == stats.nodes && stats.probes <= placement.budget);
            CHECK(placement.reflections.size() <= 42);
            std::vector<std::uint8_t> section, again;
            CHECK(encode_gi_placement_section(lattice, placement.reflections, section, error));
            if (path == "worlds/01_subwaya.dat" || path == "worlds/02_cityhub.dat")
            {
                // Deterministic bytes for the same inputs.
                auto replay = default_gi_placement(path, placement.world);
                GiLightLattice l2;
                GiPlacementStats s2;
                CHECK(resolve_light_probes(scene, replay, l2, s2, nullptr, error) &&
                      place_reflection_probes(scene, l2, replay, s2, error) &&
                      encode_gi_placement_section(l2, replay.reflections, again, error) && again == section);
            }
            ++worlds;
            single += is_single;
            if (is_single)
            {
                CHECK(stats.probes > 0);
                total_probes += stats.probes;
                total_reflections += placement.reflections.size();
                total_ms += ms;
            }
            std::cout << "PLACE " << path << " base_probes=" << base.probes << " spacing=" << lattice.spacing
                      << " probes=" << stats.probes << " relocated=" << stats.relocated << " nodes=" << stats.nodes
                      << " solid=" << stats.solid << " unknown=" << stats.unknown << " clearance=" << stats.clearance
                      << " world_model=" << stats.world_model << " reflections=" << placement.reflections.size()
                      << " open=" << stats.open << " rooms=" << stats.room_candidates << " small=" << stats.small_rooms
                      << " overlapping=" << stats.overlapping << " capped=" << stats.capped << " ms=" << ms
                      << " section_bytes=" << section.size() << '\n';
        }
        std::cout << "TOTAL worlds=" << worlds << " single=" << single << " single_probes=" << total_probes
                  << " single_reflections=" << total_reflections << " single_ms=" << total_ms << '\n';
        CHECK(single == 36);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view name(argv[1]);
    bool ok = false;
    if (name == "lattice")
        ok = lattice();
    else if (name == "world_models")
        ok = world_models();
    else if (name == "reflections")
        ok = reflections();
    else if (name == "json")
        ok = json();
    else if (name == "files" && argc == 3)
        ok = files(path_from_utf8(argv[2]));
    else if (name == "section")
        ok = section();
    else if (name == "retail" && argc == 3)
        ok = retail(path_from_utf8(argv[2]));
    else
        return 2;
    return ok ? 0 : 1;
}
