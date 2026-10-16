#include "app/model_scene.h"
#include "game/original/AI_Mgr.h"
#include "naga_fixture.h"
#include <cmath>
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
    bool near(float a, float b)
    {
        return std::abs(a - b) < .001f;
    }
    struct Actor : BaseClass
    {
        CDestructable damage;
        CInventoryMgr inventory;
        bool remove_sender = false;
        Actor()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD m, void* d, float f) override
        {
            if (m == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject);
                damage.m_fHitPoints = damage.m_fMaxHitPoints = 1000;
            }
            return BaseClass::EngineMessageFn(m, d, f);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD m, HMESSAGEREAD r) override
        {
            const auto value = BaseClass::ObjectMessageFn(sender, m, r);
            if (m == damage_message && remove_sender)
                GetServerDE()->RemoveObject(sender);
            return value;
        }
    };
    Actor* actor(GameSession& g, DVector pos, unsigned kind = 0, DDWORD flags = FLAG_SOLID)
    {
        static ClassInfo base{
            "NagaProbe", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Actor>(); }, {}};
        static ClassInfo child{
            "NagaProbeChild", &base, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Actor>(); }, {}};
        static ClassInfo other{
            "NagaTarget", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Actor>(); }, {}};
        base.parent = other.parent = g.objects.find_class("BaseClass");
        for (const auto* type : {&base, &child, &other})
            if (!g.objects.find_class(type->name) && !g.objects.register_class(*type))
                return nullptr;
        ObjectCreateStruct state;
        state.m_Pos = pos;
        state.m_Flags = flags;
        auto h = g.objects.create(kind == 0 ? &base : kind == 1 ? &child : &other, state);
        if (auto* record = g.objects.get(h))
            record->dimensions = {2, 2, 2};
        return dynamic_cast<Actor*>(g.server.HandleToObject(h));
    }
    bool setup(GameSession& g, const std::filesystem::path& root, bool assets = true)
    {
        CHECK(test_player::setup(g, root));
        std::string error;
        if (assets)
            CHECK(test_naga::mount(g.resources, root, error));
        return true;
    }
    CNagaEyes* equip(Actor* a)
    {
        if (!a || !a->inventory.Init(a->m_hObject, nullptr) ||
            a->inventory.ObtainWeapon(WEAP_NAGA_EYEBEAM, 0) != InventoryResult::Ok)
            return nullptr;
        return dynamic_cast<CNagaEyes*>(a->inventory.CurrentWeapon());
    }
    WeaponFire shot(bool alternate = false)
    {
        return {{0, 50, 0}, {0, 0, 1}, 42, WEAP_NAGA_EYEBEAM, alternate, false};
    }
    bool weapon(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* owner = actor(g, {0, 50, 0});
        auto* w = equip(owner);
        CHECK(w);
        CHECK(w->data.m_nType == 38 && w->data.m_nAmmoType == AMMO_NONE && w->data.m_fRange == 2000);
        CHECK(w->data.m_fMinDamage == 30 && w->data.m_fMaxDamage == 45);
        CHECK(near(w->data.m_fReloadTime, .2f) && near(w->data.m_fAltReloadTime, .05f));
        CHECK(w->ShowHandModel(true) && !w->m_hHandModel && !w->m_hHandAttachment);
        auto* same = actor(g, {0, 50, 30}, 1);
        auto* different = actor(g, {0, 50, 70}, 2);
        CHECK(same && different);
        CHECK(w->Fire(shot()) == FireResult::Fired);
        CHECK(w->beam_collision == SegmentResult::Hit && w->beam_hit.object == different->m_hObject);
        CHECK(w->last_shot.hit.object == same->m_hObject && near(w->beam_end.z, 68));
        CHECK(near(same->damage.m_fHitPoints, 1000 - w->last_shot.damage) && different->damage.m_fHitPoints == 1000);
        CHECK(w->beam && w->beam_error.empty() && !w->explosion_error.empty() && w->sounds_played == 1);
        CHECK(w->last_shot.damage >= 30 && w->last_shot.damage <= 45 && w->CheckAmmo() == 1);
        CHECK(g.objects.remove(same->m_hObject) && g.objects.remove(different->m_hObject) && g.objects.finish_frame());
        CHECK(w->Fire(shot(true)) == FireResult::Fired && w->beam_collision == SegmentResult::Miss);
        CHECK(near(w->beam_end.z, 2000) && w->sounds_played == 1 && !w->last_fire_sound && w->shots == 2);
        CHECK(g.enter_player(w->beam_error));
        CHECK(g.server_shell.Player(g.local_client)->m_InventoryMgr.ObtainWeapon(38) == InventoryResult::NotAvailable);
        std::cout << "Visual class-family filter passes same-class target; ordinary damage hits it. Unlimited ammo, "
                     "primary WAV and silent alt verified.\n";
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root, false));
        auto* w = equip(actor(g, {0, 50, 0}));
        CHECK(w);
        auto* target = actor(g, {0, 50, 70}, 2, FLAG_RAYHIT);
        CHECK(target);
        g.server.collision_ready = false;
        CHECK(w->Fire(shot()) == FireResult::Unavailable && w->shots == 0 && g.server.laser_beams.reports.empty());
        g.server.collision_ready = true;
        CHECK(w->Fire(shot()) == FireResult::Fired && !w->beam && !w->beam_error.empty());
        CHECK(w->beam_collision == SegmentResult::Miss && w->last_shot.damage_delivered &&
              target->damage.m_fHitPoints < 1000);
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({0, 50, 0}, {0, 50, 100}, nullptr, hit, false, SegmentObjects::SolidOnly,
                                        SegmentFilter::IgnoreOwnerClass) == SegmentResult::Invalid);
        std::string error;
        CHECK(test_naga::mount(g.resources, root, error));
        auto& beams = g.server.laser_beams;
        for (unsigned i = 0; i < 128; ++i)
            CHECK(beams.publish({0, 50, 0}, {0, 50, 100}, "bounded", g.models, g.resources, 0, error));
        CHECK(!beams.publish({0, 50, 0}, {0, 50, 100}, "overflow", g.models, g.resources, 0, error) && !error.empty());
        CHECK(beams.active.size() == 128 && beams.reports.size() == 64);
        CHECK(beams.advance(0) && beams.advance(.5) && beams.active.empty());
        CHECK(!beams.publish({}, {}, "zero", g.models, g.resources, 0, error) && !error.empty());
        CHECK(!beams.publish({}, {0, 0, 1}, std::string(101, 'x'), g.models, g.resources, 0, error));
        return true;
    }
    bool clocks(const std::filesystem::path& root)
    {
        Resources r;
        ModelStore m(r);
        std::string error;
        CHECK(test_naga::mount(r, root, error));
        CLaserBeamFX fx;
        CHECK(fx.CreateObject(m, r, {10, 20, 30}, {110, 20, 30}, 2, error));
        CHECK(fx.Update(10) && fx.start_time == 10 && near(fx.scale.x, .05f) && near(fx.scale.z, 100));
        CHECK(near(fx.position.x, 10));
        const auto f = rotate_vector(fx.rotation, {0, 0, 1});
        CHECK(near(f.x, 1));
        CHECK(fx.Update(10.125) && near(fx.scale.x, .525f) && near(fx.alpha, .5f));
        CHECK(fx.Update(10.25) && near(fx.scale.x, 1) && near(fx.alpha, .5f));
        CHECK(fx.Update(10.375) && near(fx.alpha, .25f));
        CHECK(!fx.Update(10.5) && fx.error.empty() && near(fx.alpha, 0));
        CHECK(!fx.Update(10.4) && !fx.error.empty());
        CLaserBeamFX invalid;
        CHECK(!invalid.CreateObject(m, r, {}, {0, 0, 1}, std::numeric_limits<double>::quiet_NaN(), error));
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* owner = actor(g, {0, 50, 0});
        auto* w = equip(owner);
        CHECK(w);
        auto* t = actor(g, {0, 50, 70}, 2);
        CHECK(t);
        t->remove_sender = true;
        auto h = owner->m_hObject;
        CHECK(w->Fire(shot()) == FireResult::Fired && w->last_shot.interrupted && !g.server.IsObjectAlive(h));
        const auto id = w->beam;
        CHECK(id && g.objects.finish_frame() && !g.objects.get(h));
        auto& beams = g.server.laser_beams;
        CHECK(beams.active.size() == 1);
        CHECK(g.step(.01));
        ModelView view;
        std::string error;
        CHECK(update_explosion_model_scene(g, view, error) && view.entries.size() == 1 &&
              view.entries[0].error.empty());
        CHECK(view.entries[0].laser_effect == id && view.world_draws.size() == 1);
        const auto updates = beams.active[0].updates;
        CHECK(update_explosion_model_scene(g, view, error) && updates == beams.active[0].updates);
        CHECK(g.step(.5) && beams.active.empty());
        CHECK(g.unload_world() && beams.reports.empty() && g.server.audio.voices().empty());
        CHECK(beams.publish({}, {0, 0, 100}, "after unload", g.models, g.resources, g.objects.time(), error) > id);
        CHECK(g.unload_world() && beams.active.empty());
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool ai(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        static ClassInfo type{
            "NagaAIProbe", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<AI_Mgr>(); }, {}};
        type.parent = g.objects.find_class("CBaseCharacter");
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = {0, 50, 0};
        s.m_Flags = FLAG_MODELKEYS;
        auto h = g.objects.create(&type, s);
        auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
        CHECK(a);
        CHECK(a->m_InventoryMgr.Init(h, nullptr) && a->m_InventoryMgr.ObtainWeapon(38, 0) == InventoryResult::Ok);
        auto model = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *model, error));
        for (auto& anim : model->animations)
        {
            for (auto& key : anim.keys)
                key.text.clear();
            for (auto& node : anim.nodes)
                for (auto& frame : node.frames)
                    frame.position = {};
        }
        model->nodes[1].name = "head_gun";
        model->animations[0].keys = {{0, {}, {}, "[fire_key:1]"}, {100, {}, {}, ""}};
        g.objects.get(h)->model = model;
        g.server.SetModelAnimation(h, 0);
        auto* t = actor(g, {0, 50, 70}, 2);
        CHECK(t && a->SetNewTarget(t->m_hObject));
        a->m_vTargetPos = {0, 50, 70};
        CHECK(g.server.advance_models(.01) && a->fire_shots == 1 && a->fire_gun_node);
        CHECK(t->damage.m_fHitPoints < 1000 && g.server.laser_beams.active.size() == 1);
        model->nodes[1].name = "missing";
        CHECK(!a->Fire() && a->fire_result == FireResult::Unavailable && a->fire_shots == 1);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        std::uint64_t previous = 0;
        for (unsigned iteration = 0; iteration < 3; ++iteration)
        {
            WorldMessage message;
            CHECK(original_world_request("01_SubwayA", 0, 1, 1, message));
            CHECK(g.load_world(message, error) && g.enter_player(error));
            auto* owner = actor(g, {0, 5000, 0});
            auto* w = equip(owner);
            CHECK(w);
            CHECK(w->Fire(shot()) == FireResult::Fired && w->beam && w->beam_error.empty() && w->sounds_played == 1);
            auto& beams = g.server.laser_beams;
            CHECK(w->beam > previous);
            previous = w->beam;
            // Isolated emission above the level exercises the real beam model/skin without changing an authored NPC.
            CHECK(beams.publish({0, 5000, 0}, {0, 5000, 100}, "private beam", g.models, g.resources, g.objects.time(),
                                error));
            CHECK(beams.advance(g.objects.time()));
            ModelView view;
            CHECK(update_explosion_model_scene(g, view, error));
            unsigned count = 0;
            for (const auto& row : view.entries)
                if (row.laser_effect)
                {
                    CHECK(row.error.empty() && row.triangles > 0);
                    ++count;
                }
            CHECK(count == 2);
            const auto laser_id = beams.active.front().id;
            const auto laser_updates = beams.active.front().updates;
            const auto laser_time = beams.active.front().last_time;
            CHECK(g.rehearse_destination(message, "", error));
            CHECK(beams.active.size() == 2 && beams.active.front().id == laser_id &&
                  beams.active.front().updates == laser_updates && beams.active.front().last_time == laser_time);
            std::cout << "Private loop " << iteration << ": " << count << " decoded beam models, " << w->sounds_played
                      << " original WAV, shot damage " << w->last_shot.damage << "\n";
            CHECK(g.unload_world() && beams.active.empty() && beams.reports.empty() && g.server.audio.voices().empty());
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string name = argv[1];
    return (name == "weapon"     ? weapon(argv[2])
            : name == "failures" ? failures(argv[2])
            : name == "clocks"   ? clocks(argv[2])
            : name == "lifetime" ? lifetime(argv[2])
            : name == "ai"       ? ai(argv[2])
            : name == "private"  ? private_data(argv[2])
                                 : false)
               ? 0
               : 1;
}
