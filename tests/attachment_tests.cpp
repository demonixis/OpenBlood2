#include "brush_fixture.h"
#include "game/original/HandWeaponModel.h"
#include "player_fixture.h"
#include <cmath>
#include <iostream>
#include <limits>
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
    bool near(DVector a, DVector b)
    {
        return std::abs(a.x - b.x) < .001f && std::abs(a.y - b.y) < .001f && std::abs(a.z - b.z) < .001f;
    }
    HOBJECT body(GameSession& g, bool model = false)
    {
        ObjectCreateStruct s;
        s.m_ObjectType = model ? OT_MODEL : OT_NORMAL;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), s);
        if (model)
        {
            auto m = std::make_shared<Model>();
            std::string error;
            if (!read_model(test_model::abc().b, *m, error))
                return nullptr;
            g.objects.get(h)->model = m;
        }
        return h;
    }
    bool root(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto p = body(g), c = body(g);
        CHECK(p && c);
        g.objects.get(p)->state.m_Pos = {10, 20, 30};
        DRotation q = rotation_from_euler({0, 1.5707963f, 0});
        CHECK(g.server.SetObjectRotation(p, &q) == DE_OK);
        DVector offset{0, 0, 2};
        DRotation local = rotation_from_euler({0, 0, .5f});
        HATTACHMENT id = 0;
        CHECK(g.server.CreateAttachment(p, c, nullptr, &offset, &local, &id) == DE_OK && id);
        CHECK(near(g.objects.get(c)->state.m_Pos, {12, 20, 30}));
        g.objects.get(p)->state.m_Pos = {40, 50, 60};
        DVector pos;
        CHECK(g.objects.refresh_attachments());
        g.server.GetObjectPos(c, &pos);
        CHECK(near(pos, {42, 50, 60}));
        DRotation out;
        CHECK(g.server.GetObjectRotation(c, &out) == DE_OK);
        CHECK(near(rotate_vector(out, {1, 0, 0}), rotate_vector(compose_rotation(q, local), {1, 0, 0})));
        HATTACHMENT found = 0;
        CHECK(g.server.FindAttachment(p, c, &found) == DE_OK && found == id);
        auto floor = test_brush::box("floor", {-100, -20, -100}, {100, -10, 100});
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.objects.get(c)->dimensions = {1, 1, 1};
        g.objects.get(p)->state.m_Pos = {60, 50, 60};
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({62, 50, 50}, {62, 50, 70}, p, hit, false, SegmentObjects::All) ==
              SegmentResult::Hit);
        CHECK(hit.object == c && near(g.objects.get(c)->state.m_Pos, {62, 50, 60}));
        CHECK(g.server.IntersectSegment({62, 50, 50}, {62, 50, 70}, p, hit) == SegmentResult::Miss);
        CHECK(g.server.RemoveAttachment(id) == DE_OK && !g.objects.get(c)->attachment);
        CHECK(g.server.RemoveAttachment(id) == DE_NOTFOUND && g.server.IsObjectAlive(c));
        CHECK(g.server.FindAttachment(p, c, &found) == DE_NOTFOUND && !found);
        return true;
    }
    bool nodes(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto p = body(g, true), c = body(g, true);
        CHECK(p && c);
        auto* r = g.objects.get(p);
        r->state.m_Pos = {10, 20, 30};
        r->state.m_Scale = {2, 3, 4};
        r->animation.time = 25;
        DVector offset{3, 4, 5};
        DRotation q;
        HATTACHMENT id;
        CHECK(g.server.CreateAttachment(p, c, "HAND", &offset, &q, &id) == DE_OK);
        DVector expected;
        DRotation base;
        CHECK(g.server.GetModelNodeTransform(p, "hand", &expected, &base));
        CHECK(near(g.objects.get(c)->state.m_Pos, vector_add(expected, rotate_vector(base, offset))));
        auto revision = r->animation.revision;
        r->activity = OBJSTATE_INACTIVE;
        r->animation.time = 50;
        g.objects.get(c)->activity = OBJSTATE_INACTIVE;
        g.server.SetObjectFlags(c, 0);
        DVector child_node, regular_node;
        DRotation a, b;
        CHECK(g.server.GetAttachedModelNodeTransform(id, "hand", &child_node, &a) == DE_OK);
        CHECK(g.server.GetModelNodeTransform(c, "hand", &regular_node, &b) && near(child_node, regular_node));
        CHECK(r->animation.time == 50 && r->animation.revision == revision);
        auto last = g.objects.get(c)->state.m_Pos;
        auto original = r->model;
        r->model.reset();
        CHECK(!g.objects.refresh_attachments() && !g.objects.attachments().at(id).resolved &&
              near(last, g.objects.get(c)->state.m_Pos));
        DVector unchanged{9, 8, 7};
        CHECK(!g.server.GetModelNodeTransform(c, "hand", &unchanged, &a) && near(unchanged, {9, 8, 7}));
        r->model = original;
        CHECK(g.objects.refresh_attachments());
        return true;
    }
    bool rejects(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto p = body(g, true), c = body(g), extra = body(g);
        DVector offset;
        DRotation q;
        HATTACHMENT id = 123;
        CHECK(g.server.CreateAttachment(p, p, nullptr, &offset, &q, &id) == DE_INVALIDPARAMS && id == 0);
        CHECK(g.server.CreateAttachment(p, c, "missing", &offset, &q, &id) == DE_NODENOTFOUND && !id);
        CHECK(g.server.CreateAttachment(p, c, nullptr, nullptr, &q, &id) == DE_INVALIDPARAMS);
        q.m_Spin = 0;
        CHECK(g.server.CreateAttachment(p, c, nullptr, &offset, &q, &id) == DE_INVALIDPARAMS);
        q = {};
        g.server.SetObjectFlags(c, FLAG_SOLID);
        CHECK(g.server.CreateAttachment(p, c, nullptr, &offset, &q, &id) == DE_ERROR);
        g.server.SetObjectFlags(c, 0);
        CHECK(g.server.CreateAttachment(p, c, nullptr, &offset, &q, &id) == DE_OK);
        HATTACHMENT another = 0;
        CHECK(g.server.CreateAttachment(extra, c, nullptr, &offset, &q, &another) == DE_ERROR);
        CHECK(g.server.CreateAttachment(c, extra, nullptr, &offset, &q, &another) == DE_ERROR);
        CHECK(g.server.CreateAttachment(extra, p, nullptr, &offset, &q, &another) == DE_ERROR);
        CHECK(g.server.TeleportObject(c, &offset) == DE_INVALIDPARAMS &&
              g.server.MoveObject(c, &offset) == DE_INVALIDPARAMS);
        CHECK(g.server.SetVelocity(c, &offset) == DE_INVALIDPARAMS &&
              g.server.SetAcceleration(c, &offset) == DE_INVALIDPARAMS);
        CHECK(g.server.SetObjectRotation(c, &q) == DE_INVALIDPARAMS);
        g.server.SetObjectFlags(c, FLAG_GRAVITY);
        CHECK(!(g.server.GetObjectFlags(c) & FLAG_GRAVITY));
        CHECK(g.objects.attachments().size() == 1);
        CHECK(g.server.MoveKeyframedObject(c, offset, nullptr) == MoverResult::Unavailable);
        CHECK(g.server.RemoveAttachment(id) == DE_OK);
        offset.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(g.server.CreateAttachment(p, c, nullptr, &offset, &q, &id) == DE_INVALIDPARAMS && !id);
        offset = {};
        std::string long_node(101, 'x');
        CHECK(g.server.CreateAttachment(p, c, long_node.c_str(), &offset, &q, &id) == DE_INVALIDPARAMS);
        // Allocation IDs cannot be recycled even when each relation is immediately removed.
        for (unsigned i = 1; i < 65536; ++i)
        {
            CHECK(g.server.CreateAttachment(p, c, nullptr, &offset, &q, &id) == DE_OK);
            CHECK(g.server.RemoveAttachment(id) == DE_OK);
        }
        CHECK(g.server.CreateAttachment(p, c, nullptr, &offset, &q, &id) == DE_ERROR && !id &&
              g.objects.attachments().empty());
        return true;
    }
    class RemoveAttachedParent : public BaseClass
    {
      public:
        HOBJECT parent = nullptr;
        HATTACHMENT attachment = 0;
        bool observed = false;

      protected:
        DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT reason) override
        {
            if (message == MID_UPDATE)
            {
                auto* s = GetServerDE();
                s->RemoveObject(parent);
                DVector p;
                DRotation q;
                observed = !s->IsObjectAlive(parent) &&
                           s->GetAttachedModelNodeTransform(attachment, "hand", &p, &q) == DE_INVALIDPARAMS;
            }
            return BaseClass::EngineMessageFn(message, data, reason);
        }
    };
    bool lifetime(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        HATTACHMENT previous = 0;
        static const ClassInfo removal{"AttachmentRemovalProbe",
                                       g.objects.find_class("BaseClass"),
                                       []() -> std::unique_ptr<BaseClass>
                                       { return std::make_unique<RemoveAttachedParent>(); },
                                       {}};
        CHECK(g.objects.register_class(removal));
        auto parent = body(g);
        auto child = g.objects.create(&removal);
        auto* remover = dynamic_cast<RemoveAttachedParent*>(g.objects.get(child)->game.get());
        CHECK(remover);
        remover->parent = parent;
        DVector offset;
        DRotation rotation;
        CHECK(g.server.CreateAttachment(parent, child, nullptr, &offset, &rotation, &remover->attachment) == DE_OK);
        CHECK(g.objects.set_next_update(child, .01f) && g.objects.advance(.02));
        CHECK(remover->observed && !g.objects.get(parent) && g.server.IsObjectAlive(child) &&
              g.objects.attachments().empty());
        CHECK(g.unload_world());
        for (int i = 0; i < 3; ++i)
        {
            auto p = body(g), c = body(g);
            DVector o;
            DRotation q;
            HATTACHMENT id;
            CHECK(g.server.CreateAttachment(p, c, nullptr, &o, &q, &id) == DE_OK && id != previous);
            CHECK(g.objects.remove(p) && g.objects.attachments().empty() && !g.objects.get(c)->attachment);
            CHECK(g.server.RemoveAttachment(id) == DE_NOTFOUND && g.server.IsObjectAlive(c));
            CHECK(g.objects.finish_frame());
            p = body(g);
            CHECK(g.server.CreateAttachment(p, c, nullptr, &o, &q, &id) == DE_OK);
            CHECK(g.objects.remove(c) && g.objects.attachments().empty());
            CHECK(g.objects.finish_frame());
            c = body(g);
            CHECK(g.server.CreateAttachment(p, c, nullptr, &o, &q, &id) == DE_OK);
            previous = id;
            CHECK(g.unload_world() && g.objects.attachments().empty() && g.objects.snapshot().empty());
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
    bool held(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(test_player::setup(g, path));
        std::string error;
        CHECK(g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        auto& inv = player->m_InventoryMgr;
        CHECK(g.step(.02));
        auto* w = inv.CurrentWeapon();
        auto h = w->m_hHandModel;
        CHECK(h && w->m_hHandAttachment && g.objects.get(h)->model);
        // Synthetic Caleb has no r_gun: source origin fallback, then scheduled hidden state.
        CHECK(g.step(.02));
        CHECK(!(g.server.GetObjectFlags(h) & FLAG_VISIBLE));
        CHECK(g.objects.attachments().at(w->m_hHandAttachment).node.empty());
        CHECK(g.select_weapon(0, error));
        CHECK(!g.server.IsObjectAlive(h));
        CHECK(g.step(.02));
        CHECK(inv.CurrentWeapon()->data.m_nType == WEAP_MELEE && inv.CurrentWeapon()->m_hHandModel);
        CHECK(g.select_weapon(1, error) && g.step(.02));
        CHECK(inv.ObtainWeapon(WEAP_BERETTA, 1) == InventoryResult::Ok && g.step(.02));
        CHECK(inv.CurrentWeapon(true)->m_hHandModel && g.objects.attachments().size() == 2);
        auto removed = inv.CurrentWeapon()->m_hHandModel;
        CHECK(g.objects.remove(removed) && g.objects.finish_frame());
        CHECK(g.step(.02) && inv.CurrentWeapon()->m_hHandModel != removed);
        auto detached = inv.CurrentWeapon()->m_hHandModel;
        CHECK(g.server.RemoveAttachment(inv.CurrentWeapon()->m_hHandAttachment) == DE_OK);
        CHECK(g.step(.02) && inv.CurrentWeapon()->m_hHandModel != detached && !g.server.IsObjectAlive(detached));
        player->m_damage.m_bDead = true;
        CHECK(g.step(.02));
        CHECK(g.objects.attachments().empty() && !inv.CurrentWeapon()->m_hHandModel &&
              !inv.CurrentWeapon(true)->m_hHandModel);
        CHECK(g.leave_player() && g.objects.attachments().empty());
        return true;
    }
    bool missing(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(test_player::setup(g, path));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto* w = p->m_InventoryMgr.CurrentWeapon();
        // Removing the resource mount forces the real model loader to fail.
        auto saved = std::move(g.resources);
        g.resources = Resources{};
        CHECK(!w->ShowHandModel(true) && !w->hand_error.empty() && !w->m_hHandModel);
        CHECK(g.objects.finish_frame() && g.objects.attachments().empty());
        g.resources = std::move(saved);
        CHECK(g.step(.02));
        const auto old = w->m_hHandModel;
        saved = std::move(g.resources);
        g.resources = Resources{};
        CHECK(!g.select_weapon(0, error) && !error.empty());
        CHECK(p->m_InventoryMgr.m_nCurWeaponSlot == 1 && g.client_shell.weapons[0].type == WEAP_BERETTA);
        g.resources = std::move(saved);
        CHECK(g.step(.02));
        CHECK(w->m_hHandModel && w->m_hHandModel != old && !g.objects.get(old) && g.objects.attachments().size() == 1);
        CHECK(g.unload_world() && g.objects.attachments().empty());
        return true;
    }
    bool catalog(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        Config config;
        config.data_dir = path;
        CHECK(g.resources.mount_game(config, error));
        unsigned poses = 0, fallback = 0;
        for (auto file :
             {"models/characters/caleb.abc", "models/enemies/m_civilian1.abc", "models/enemies/f_civilian1.abc",
              "models/enemies/labtech.abc", "models/enemies/m_cultist1.abc", "models/enemies/f_cultist1.abc",
              "models/enemies/gideon.abc"})
        {
            ObjectCreateStruct state;
            state.m_ObjectType = OT_MODEL;
            SAFE_STRCPY(state.m_Filename, file);
            auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
            auto* r = g.objects.get(h);
            CHECK(r && r->model);
            for (bool left : {false, true})
            {
                CBeretta gun;
                gun.Init(h, nullptr, left);
                CHECK(gun.ShowHandModel(true));
                CHECK(g.objects.advance(.02));
                std::cout << file << " / " << (left ? "left" : "right") << " / "
                          << (g.objects.attachments().at(gun.m_hHandAttachment).node.empty() ? "origin fallback"
                                                                                             : "gun node")
                          << "\n";
                CHECK(g.server.GetModelAnimation(gun.m_hHandModel) ==
                      g.server.GetAnimIndex(gun.m_hHandModel, "handheld"));
                for (unsigned clip = 0; clip < r->model->animations.size(); ++clip)
                {
                    r->animation.animation = clip;
                    r->animation.time = r->model->animations[clip].duration * .5;
                    CHECK(g.objects.refresh_attachments());
                    DVector node;
                    DRotation q;
                    const bool found = g.server.GetModelNodeTransform(h, left ? "l_gun" : "r_gun", &node, &q);
                    CHECK(g.objects.attachments().at(gun.m_hHandAttachment).node.empty() == !found);
                    if (!found)
                    {
                        node = r->state.m_Pos;
                        ++fallback;
                    }
                    CHECK(near(node, g.objects.get(gun.m_hHandModel)->state.m_Pos));
                    ++poses;
                }
            }
            CHECK(g.objects.remove(h) && g.objects.finish_frame() && g.objects.attachments().empty());
        }
        std::cout << "Private held-model poses: " << poses << ", source origin fallbacks: " << fallback << "\n";
        CHECK(poses == 652 && fallback > 0);
        return true;
    }
    bool subway(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        Config config;
        config.data_dir = path;
        CHECK(g.resources.mount_game(config, error));
        HATTACHMENT previous = 0;
        for (int i = 0; i < 2; ++i)
        {
            CHECK(g.request_world("01_subwaya", error) && g.enter_player(error) && g.step(.02));
            auto* p = g.server_shell.Player(g.local_client);
            auto* w = p->m_InventoryMgr.CurrentWeapon();
            CHECK(w && w->m_hHandModel);
            CHECK(w->m_hHandAttachment != previous);
            previous = w->m_hHandAttachment;
            for (int j = 0; j < 10; ++j)
                CHECK(g.step(.02));
            DVector node;
            DRotation q;
            CHECK(g.server.GetModelNodeTransform(p->m_hObject, "r_gun", &node, &q));
            CHECK(near(node, g.objects.get(w->m_hHandModel)->state.m_Pos));
            CHECK(g.select_weapon(0, error) && g.step(.02));
            w = p->m_InventoryMgr.CurrentWeapon();
            CHECK(g.objects.get(w->m_hHandModel)->model && w->data.m_nType == WEAP_MELEE);
            CHECK(g.unload_world() && g.objects.attachments().empty() && g.objects.snapshot().empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string name = argv[1];
    std::filesystem::path path = argc > 2 ? argv[2] : "";
    if (name == "root")
        return !root(path);
    if (name == "nodes")
        return !nodes(path);
    if (name == "rejects")
        return !rejects(path);
    if (name == "lifetime")
        return !lifetime(path);
    if (name == "held")
        return !held(path);
    if (name == "missing")
        return !missing(path);
    if (name == "catalog")
        return !catalog(path);
    if (name == "subway")
        return !subway(path);
    return 2;
}
