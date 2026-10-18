#include "app/model_scene.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CharacterNodes.h"
#include "player_fixture.h"
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
    HOBJECT model_object(GameSession& g, std::shared_ptr<Model>& m)
    {
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Flags = FLAG_VISIBLE;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
        m = std::make_shared<Model>();
        for (const char* name :
             {"head", "head_extra1", "head_extra2", "head_extra4", "other", "neck_extra1", "neck_extra2"})
        {
            ModelNode n;
            n.name = name;
            n.index = m->nodes.size();
            n.parent = n.index ? 0 : -1; // Hierarchy must NOT imply visibility inheritance.
            m->nodes.push_back(n);
        }
        g.objects.get(h)->model = m;
        return h;
    }
    bool services(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        std::shared_ptr<Model> m;
        auto h = model_object(g, m);
        DBOOL hidden = 99;
        CHECK(g.server.GetModelNodeHideStatus(h, "HEAD", &hidden) == DE_OK && !hidden);
        CHECK(g.objects.get(h)->hidden_nodes.empty());
        CHECK(g.server.SetModelNodeHideStatus(h, "head", true) == DE_OK);
        CHECK(g.server.SetModelNodeHideStatus(h, "HEAD", true) == DE_OK); // Idempotent success.
        CHECK(g.server.GetModelNodeHideStatus(h, "head", &hidden) == DE_OK && hidden);
        CHECK(g.server.GetModelNodeHideStatus(h, "other", &hidden) == DE_OK && !hidden);
        hidden = 99;
        auto before = g.objects.get(h)->hidden_nodes;
        CHECK(g.server.GetModelNodeHideStatus(h, "absent", &hidden) == DE_NOTFOUND && hidden == 99);
        CHECK(g.server.SetModelNodeHideStatus(h, "absent", true) == DE_NOTFOUND &&
              before == g.objects.get(h)->hidden_nodes);
        CHECK(g.server.GetModelNodeHideStatus(h, "", &hidden) != DE_OK && hidden == 99);
        CHECK(g.server.GetModelNodeHideStatus(h, "head", nullptr) != DE_OK);
        // LithTech Model::FindNode: a duplicate name resolves to the first node in model order.
        std::size_t first = 0;
        while (m->nodes[first].name != "head")
            ++first;
        CHECK(first < 4);
        m->nodes[4].name = "HEAD";
        CHECK(g.server.GetModelNodeHideStatus(h, "head", &hidden) == DE_OK && hidden);
        CHECK(g.server.SetModelNodeHideStatus(h, "head", false) == DE_OK && !g.objects.get(h)->hidden_nodes[first] &&
              g.objects.get(h)->hidden_nodes[4] == before[4]);
        CHECK(g.server.SetModelNodeHideStatus(h, "head", true) == DE_OK && before == g.objects.get(h)->hidden_nodes);
        m->nodes[4].name = "other";
        g.server.SetObjectState(h, OBJSTATE_INACTIVE);
        CHECK(g.server.GetModelNodeHideStatus(h, "head", &hidden) == DE_OK && hidden);
        CHECK(g.objects.remove(h));
        hidden = 99;
        CHECK(g.server.GetModelNodeHideStatus(h, "head", &hidden) != DE_OK && hidden == 99);
        CHECK(g.objects.finish_frame());
        CHECK(g.server.SetModelNodeHideStatus(h, "head", false) != DE_OK);
        // No 64-bit shift limit: final node of the reader's full 256-node budget.
        h = model_object(g, m);
        m->nodes.resize(256);
        m->nodes[255].name = "last";
        CHECK(g.server.SetModelNodeHideStatus(h, "last", true) == DE_OK);
        CHECK(g.server.GetModelNodeHideStatus(h, "last", &hidden) == DE_OK && hidden);
        CHECK(g.objects.get(h)->hidden_nodes.size() == 256);
        m->nodes.resize(257);
        CHECK(g.server.SetModelNodeHideStatus(h, "last", false) != DE_OK);
        return true;
    }
    bool names(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        std::shared_ptr<Model> m;
        auto h = model_object(g, m);
        std::string e;
        CHECK(hide_character_node(h, 0, true, e));
        auto& mask = g.objects.get(h)->hidden_nodes;
        CHECK(mask == std::vector<std::uint8_t>({1, 1, 1, 0, 0, 0, 0}));
        CHECK(hide_character_node(h, 1, true, e)); // Missing base neck; extras still visited.
        CHECK(mask == std::vector<std::uint8_t>({1, 1, 1, 0, 0, 1, 1}));
        CHECK(g.server.SetModelNodeHideStatus(h, "head_extra4", true) == DE_OK);
        CHECK(g.server.SetModelNodeHideStatus(h, "other", true) == DE_OK);
        CHECK(reset_character_nodes(h, e));
        CHECK(mask == std::vector<std::uint8_t>({0, 0, 0, 1, 1, 0, 0})); // Gap and nonstandard names preserved.
        CHECK(hide_character_node(h, 0, true, e));
        auto before = mask;
        m->nodes[4].name = "HEAD"; // Duplicate: resets reach the first "head" only.
        CHECK(reset_character_nodes(h, e) && mask[4] == before[4]);
        CHECK(!hide_character_node(h, -1, false, e) && !hide_character_node(h, 21, false, e));
        CHECK(g.objects.clear());
        return true;
    }
    bool rendering(const char*)
    {
        Model m;
        std::string e;
        CHECK(read_model(test_model::abc().b, m, e));
        Resources resources;
        ModelView scene;
        AnimationTrack track;
        auto add = [&](std::span<const std::uint8_t> mask)
        {
            scene.begin_frame();
            return scene.add(&m, track, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "probe", resources, e, mask);
        };
        CHECK(add({}) && scene.mesh.vertices.size() == 3);
        auto revision = scene.revision;
        CHECK(add(std::array<std::uint8_t, 2>{1, 0})); // Mixed-node face removed if any corner is hidden.
        CHECK(scene.mesh.vertices.empty() && scene.world_draws.empty() && scene.entries.size() == 1);
        CHECK(scene.entries[0].hidden_nodes == 1 && scene.entries[0].hidden_triangles == 1);
        CHECK(scene.revision == revision && track.time == 0 && track.first);
        CHECK(add(std::array<std::uint8_t, 2>{0, 0}) && scene.mesh.vertices.size() == 3);
        CHECK(!add(std::array<std::uint8_t, 1>{1}) && scene.entries.empty());
        // Vertex node IDs are not preorder indices. Sparse/permuted IDs must map correctly.
        m.nodes[0].index = 250;
        for (auto& v : m.vertices)
            if (!v.node)
                v.node = 250;
        CHECK(add(std::array<std::uint8_t, 2>{1, 0}) && scene.mesh.vertices.empty());
        CHECK(add({}) && scene.mesh.vertices.size() == 3);
        m.vertices[0].node = 249;
        CHECK(!add({}) && scene.entries.empty());
        return true;
    }
    bool restore(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = g.objects.create(g.objects.find_class("CPlayerObj"), {}, Creation::Restore);
        auto* p = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
        CHECK(p && p->initialized);
        std::string e;
        CHECK(!p->RestoreModelState(e));
        PlayerState fields;
        fields.air = 42;
        fields.strength = 6;
        CHECK(p->LoadFields(fields, e));
        auto* r = g.objects.get(h);
        r->velocity = {1, 2, 3};
        r->acceleration = {4, 5, 6};
        r->state.m_Flags = FLAG_SHADOW;
        r->animation.time = 17;
        r->dimensions = {9, 10, 11};
        p->m_damage.SetHitPoints(37);
        p->m_InventoryMgr.SetAmmoCount(1, 17);
        const auto children = g.objects.snapshot().size();
        CHECK(g.server.SetModelNodeHideStatus(h, "root", true) == DE_OK);
        CHECK(p->RestoreModelState(e) && p->restore_model_loaded && p->restore_model_error.empty());
        const DDWORD flags = FLAG_VISIBLE | FLAG_MODELKEYS | FLAG_YROTATION | FLAG_ANIMTRANSITION | FLAG_TOUCH_NOTIFY |
                             FLAG_SOLID | FLAG_STAIRSTEP;
        CHECK(r->state.m_Flags == flags && vector_dot(r->velocity, r->velocity) == 0 && r->acceleration.y == 5);
        CHECK(r->dimensions.y == 10 && r->animation.time == 17 && !r->scheduled);
        CHECK(p->m_damage.m_fHitPoints == 37 && p->m_InventoryMgr.GetAmmoCount(1) == 17 && p->m_nAttribStrength == 6);
        CHECK(p->m_fAirLevel == 42 && p->respawns == 0 && !p->GetClient() && !p->attributes_received);
        CHECK(g.objects.snapshot().size() == children && p->m_Anim_Sound.GetSoundRoot() == "sounds\\chosen\\caleb");
        DBOOL hidden = false;
        CHECK(g.server.GetModelNodeHideStatus(h, "root", &hidden) == DE_OK &&
              hidden); // Source ResetNodes is named, not global.
        // Same resource retains visibility/clock; successful model replacement resets both.
        CHECK(g.server.SetModelFilenames(h, "missing.abc", "missing.dtx") != DE_OK && r->hidden_nodes[0]);
        CHECK(g.server.SetModelFilenames(h, "models/weapons/beretta_pv.abc", "skin.dtx") == DE_OK);
        CHECK(r->hidden_nodes.empty() && r->animation.time == 0);
        CHECK(g.server.SetModelNodeHideStatus(h, "root", true) == DE_OK);
        CHECK(p->RestoreModelState(e) && r->hidden_nodes.empty());
        auto client = g.server.connect_client();
        p->SetClient(client);
        r->velocity = {1, 2, 3};
        CHECK(!p->RestoreModelState(e) && !p->restore_model_loaded && r->velocity.y == 2);
        p->SetClient(nullptr);
        CHECK(g.server.disconnect_client(client));
        p->m_bPowerupActivated[PU_TRIPLEDAMAGE] = true;
        CHECK(!p->RestoreModelState(e));
        p->m_bPowerupActivated[PU_TRIPLEDAMAGE] = false;
        // Model failure is reported, not converted to a completed tail.
        r->animation.animation = UINT32_MAX;
        CHECK(!p->RestoreModelState(e) && !p->restore_model_error.empty() && r->velocity.y == 2);
        CHECK(g.unload_world() && g.server.string_count() == 0 && CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool private_restore(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string e;
        CHECK(g.resources.mount_game(config, e));
        ModelView scene;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", e) && g.enter_player(e) && g.step(1. / 60));
            auto* source = g.server_shell.Player(g.local_client);
            CHECK(source);
            PlayerState fields;
            InventoryState inventory;
            DestructableState damage;
            CHECK(source->SaveFields(fields, e) && source->m_InventoryMgr.Save(inventory, e) &&
                  source->m_damage.Save(damage, e));
            const auto before = g.objects.snapshot().size();
            auto h = g.objects.create(g.objects.find_class("CPlayerObj"), {}, Creation::Restore);
            auto* p = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
            CHECK(p && p->LoadFields(fields, e) && p->m_InventoryMgr.Load(inventory, InventoryLoadType::KeepAlive, e) &&
                  p->m_damage.Load(damage, e));
            auto* r = g.objects.get(h);
            CHECK(r->model && hide_character_node(h, 0, true, e));
            r->state.m_Flags = FLAG_VISIBLE;
            auto triangles = [&]() -> std::size_t
            {
                for (const auto& entry : scene.entries)
                    if (entry.object == r->id)
                        return entry.triangles;
                return 0;
            };
            CHECK(update_model_scene(g, scene, e));
            const auto hidden_triangles = triangles();
            CHECK(hidden_triangles > 0 && hidden_triangles < r->model->faces.size());
            r->velocity = {3, 4, 5};
            CHECK(p->RestoreModelState(e) && update_model_scene(g, scene, e));
            CHECK(triangles() == r->model->faces.size() && r->model->faces.size() == 941);
            CHECK(p->m_Anim_Sound.hit_spheres_valid && !p->GetClient() && !r->scheduled && r->dimensions.y == 0);
            CHECK(p->m_damage.m_fHitPoints == damage.health && p->m_InventoryMgr.GetAmmoCount(1) == inventory.ammo[1]);
            CHECK(p->m_InventoryMgr.UpdateHandModels());
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
            CHECK(g.objects.snapshot().size() == before && !g.objects.get(h));
            std::cout << "cycle=" << cycle << " Caleb triangles hidden=" << hidden_triangles
                      << " reset=941; isolated Load tail, no restored client\n";
            CHECK(g.unload_world() && g.server.string_count() == 0 && g.objects.attachments().empty());
            CHECK(CDestructable::m_dwNumDestructables == 0 && AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0);
            scene.clear();
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const char*)>> cases{{"services", services},
                                                                        {"names", names},
                                                                        {"rendering", rendering},
                                                                        {"restore", restore},
                                                                        {"private", private_restore}};
    if (argc < 2)
        return 2;
    auto test = cases.find(argv[1]);
    return test != cases.end() && test->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
