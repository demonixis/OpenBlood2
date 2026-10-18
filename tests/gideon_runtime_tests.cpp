#include "game/game_settings.h"
#include "game/original/CameraObj.h"
#include "game/original/DestructableBrush.h"
#include "gideon_fixture.h"
#include <functional>
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
    // Synthetic node table for precise packet/order tests. Private tests below
    // use the production factory and external ABC, without injected initialization.
    test_gideon::Actor* probe(GameSession& g)
    {
        auto* a = test_gideon::actor(g, false, {0, 100, 0});
        if (!a)
            return nullptr;
        auto model = std::make_shared<Model>();
        ModelAnimation clip;
        clip.name = "idle";
        clip.dimensions = {20, 20, 20};
        clip.duration = 100;
        clip.keys.resize(2);
        clip.keys[1].time = 100;
        for (unsigned i = 0; i < 18; ++i)
        {
            ModelNode n;
            n.name = szNodes[i];
            n.index = i;
            model->nodes.push_back(n);
            NodeAnimation node;
            node.frames = {{{float(i) * 4, 0, 0}, {}}, {{float(i) * 4, 0, 0}, {}}};
            clip.nodes.push_back(node);
        }
        model->animations.push_back(clip);
        auto* r = g.objects.get(a->m_hObject);
        r->model = model;
        r->dimensions = clip.dimensions;
        auto table = std::make_shared<CAnim_Sound>();
        table->hit_spheres_valid = true;
        table->m_fHitSpheres.fill(1);
        if (!a->InitStatics(table))
            return nullptr;
        a->ConfigureRuntime(AI_Mgr::NPCType::Gideon);
        a->runtime_initialized = true;
        a->m_nState = AI_Mgr::StateInactive;
        a->m_damage.SetHitPoints(100);
        a->m_damage.SetArmorPoints(0);
        a->m_damage.m_fMaxHitPoints = 100;
        a->m_damage.ai_hit_localization = true;
        a->m_fFullHealth = 100;
        a->m_vDims = clip.dimensions;
        return a;
    }
    bool defaults(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* type = g.objects.find_class("Gideon");
        CHECK(type);
        {
            auto object = type->construct();
            auto* a = dynamic_cast<Gideon*>(object.get());
            CHECK(a && a->npc_type == AI_Mgr::NPCType::Gideon && !a->m_bCabal);
            CHECK(a->npc_hp == 4000 && a->npc_armor == 200 && a->npc_random_hp == 0);
            CHECK(a->m_fHearingDist == 0 && a->m_fSensingDist == 10000 && a->m_fSeeingDist == 10000);
            CHECK(a->m_fWalkSpeed == 75 && a->m_fRunSpeed == 180 && a->m_fRollSpeed == 3.5f && a->m_dwFlags == 0x20);
            CHECK(a->npc_weapons[0] == "GIDEON_SHIELD" && a->npc_weapons[3] == "GIDEON_WIND");
            CHECK(!a->m_damage.apply_damage_physics && a->m_fFullHealth == 0 && a->m_bCreateHealth);
        }
        const auto count = g.objects.snapshot().size();
        CHECK(!g.server.IsObjectAlive(
            g.objects.create(type, {}, Creation::World))); // Missing ABC must fail, never substitute.
        CHECK(g.objects.finish_frame() && g.objects.snapshot().size() == count);
        CHECK(!g.server.IsObjectAlive(g.objects.create(type, {}, Creation::Restore)));
        CHECK(g.objects.finish_frame() && g.objects.snapshot().size() == count);
        return true;
    }
    bool health(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* a = probe(g);
        CHECK(a);
        a->m_damage.SetHitPoints(75);
        CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr));
        CHECK(a->health_no_client == 1 && a->health_deliveries == 0 && a->boss_health_fraction == .75f &&
              a->m_bCreateHealth);
        CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr) && a->health_requests == 2);
        a->m_fFullHealth = 0;
        CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr));
        CHECK(!a->m_bCreateHealth && a->m_fFullHealth == 75 && a->boss_health_fraction == 1);
        CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr) && a->health_requests == 3);
        a->m_bCreateHealth = true;
        a->m_fFullHealth = std::numeric_limits<float>::quiet_NaN();
        g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr);
        CHECK(a->runtime_failed && a->health_result == DE_INVALIDPARAMS && !a->health_error.empty());
        CHECK(!g.objects.get(a->m_hObject)->scheduled);
        for (bool denominator : {false, true})
        {
            auto* invalid = probe(g);
            CHECK(invalid);
            if (denominator)
                invalid->m_fFullHealth = std::numeric_limits<float>::infinity();
            else
            {
                invalid->m_fFullHealth = 0; // The initial full-health packet must not hide invalid HP.
                invalid->m_damage.m_fHitPoints = std::numeric_limits<float>::infinity();
            }
            g.objects.engine_message(invalid->m_hObject, MID_UPDATE, nullptr);
            CHECK(invalid->runtime_failed && invalid->health_result == DE_INVALIDPARAMS &&
                  invalid->health_deliveries == 0 && invalid->health_no_client == 0);
            CHECK(!g.objects.get(invalid->m_hObject)->scheduled);
        }
        return true;
    }
    bool client(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.server.SendBossHealth({.5f}) == DE_NOTFOUND);
        CHECK(g.enter_player(error));
        for (float f : {1.f, .5f, 0.f, -1.f, 4001.f})
        {
            const auto before = g.client_shell.boss_health_messages;
            CHECK(g.server.SendBossHealth({f}) == DE_OK && g.pump_player_messages(error));
            CHECK(g.client_shell.boss_health_messages == before + 1 && g.client_shell.boss_health.fraction == f);
            CHECK(g.client_shell.boss_health_visible == (f > 0 && f < 1));
        }
        const float bad = std::numeric_limits<float>::quiet_NaN();
        CHECK(g.server.SendBossHealth({bad}) == DE_INVALIDPARAMS);
        CHECK(!g.client_shell.OnMessage(BossHealth{bad}, g.server_shell));
        auto* a = probe(g);
        CHECK(a);
        a->m_damage.SetHitPoints(25);
        CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr) && a->health_deliveries == 1);
        CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame() && g.pump_player_messages(error));
        CHECK(g.client_shell.boss_health.fraction == .25f &&
              g.client_shell.boss_health_visible); // Owned packet survives sender.
        for (unsigned i = 0; i < 256; ++i)
            CHECK(g.server.SendBossHealth({.1f}) == DE_OK);
        CHECK(g.server.SendBossHealth({.1f}) == DE_ERROR);
        auto* blocked = probe(g);
        CHECK(blocked);
        g.objects.engine_message(blocked->m_hObject, MID_UPDATE, nullptr);
        CHECK(blocked->runtime_failed && blocked->health_result == DE_ERROR && !blocked->health_error.empty());
        CHECK(g.unload_world());
        CHECK(!g.client_shell.boss_health_visible && g.client_shell.boss_health_messages == 0);
        CHECK(g.server.SendBossHealth({.5f}) == DE_NOTFOUND);
        return true;
    }
    bool send(GameSession& g, HOBJECT target, HOBJECT sender, float amount)
    {
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(sender), target, damage_message);
        return m && g.server.WriteDamage(m, {{0, 0, 1}, {8, 100, -1}, amount, DamageType::Normal, sender}) == DE_OK &&
               g.server.EndMessage(m) == DE_OK;
    }
    bool damage(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* a = probe(g);
        CHECK(a);
        auto sender = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(sender);
        CHECK(send(g, a->m_hObject, sender, 20));
        CHECK(a->m_damage.m_fHitPoints == 80 && a->m_damage.accepted_damage == 1 && a->damage_specials == 0);
        CHECK(a->m_nState == AI_Mgr::StateRecoil && a->health_requests == 1 && a->boss_health_fraction == .8f);
        CHECK(send(g, a->m_hObject, sender, 1));
        CHECK(a->m_damage.m_fHitPoints == 79 && a->m_damage.accepted_damage == 2 && a->damage_specials == 1);
        CHECK(a->m_nState == AI_Mgr::StateRecoil &&
              a->damage_reactions == 2); // Gideon Special1 precedes base ALWAYSRECOIL.
        CHECK(a->AI_STATE_Recoil() && a->m_nState == AI_Mgr::StateSpecial1);
        a->m_damage.can_damage = false;
        CHECK(send(g, a->m_hObject, sender, 100) && a->health_requests == 2 && a->damage_specials == 1);
        CHECK(a->m_damage.m_fHitPoints == 79 && !a->runtime_failed && g.server.message_count() == 0);
        a->m_damage.can_damage = true;
        a->m_fFullHealth = 0;
        CHECK(send(g, a->m_hObject, sender, 1));
        CHECK(a->m_damage.m_fHitPoints == 78 && a->m_damage.accepted_damage == 3 && a->runtime_failed);
        CHECK(a->health_result == DE_INVALIDPARAMS && !a->health_error.empty() && a->damage_reactions == 2);
        return true;
    }
    class Callback : public BaseClass
    {
      public:
        std::function<void()> action;

      protected:
        DDWORD ObjectMessageFn(HOBJECT, DDWORD message, HMESSAGEREAD) override
        {
            if (message == 1000 && action)
                action();
            return 1;
        }
    };
    bool lifetime(const char* root)
    {
        for (bool remove : {false, true})
        {
            GameSession g;
            CHECK(test_player::setup(g, root));
            auto* a = probe(g);
            CHECK(a);
            static ClassInfo type{"BossDamageCallback",
                                  nullptr,
                                  []() -> std::unique_ptr<BaseClass> { return std::make_unique<Callback>(); },
                                  {}};
            type.parent = g.objects.find_class("BaseClass");
            CHECK(g.objects.register_class(type));
            ObjectCreateStruct s;
            SAFE_STRCPY(s.m_Name, "boss_callback");
            auto h = g.objects.create(&type, s);
            auto* cb = dynamic_cast<Callback*>(g.server.HandleToObject(h));
            CHECK(cb);
            auto target = a->m_hObject;
            bool nested = false;
            const auto strings = g.server.string_count();
            cb->action = [&]
            {
                nested = send(g, target, h, 100);
                if (remove)
                    g.server.RemoveObject(target);
            };
            a->m_damage.damage_target = "boss_callback";
            a->m_damage.damage_message = "TRIGGER";
            CHECK(send(g, target, h, 25) && nested && a->m_damage.accepted_damage == 1 &&
                  a->m_damage.m_fHitPoints == 75);
            CHECK(a->health_requests == (remove ? 0 : 1) && a->damage_specials == (remove ? 0 : 1));
            CHECK(g.server.IsObjectAlive(target) != remove && g.server.message_count() == 0 &&
                  g.server.string_count() == strings);
            CHECK(g.unload_world() && g.objects.snapshot().empty());
        }
        return true;
    }
    bool mount(GameSession& g, const char* root)
    {
        CHECK(g.initialize() && test_gideon::floor(g));
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        return true;
    }
    Gideon* production(GameSession& g, DVector pos = {0, 100, 500}, const char* state = "INACTIVE")
    {
        auto h = g.objects.create(g.objects.find_class("Gideon"), {}, Creation::World,
                                  {{"Pos", pos}, {"AIState", std::string(state)}, {"RandomHitPoints", 1.f}});
        return g.server.IsObjectAlive(h) ? dynamic_cast<Gideon*>(g.server.HandleToObject(h)) : nullptr;
    }
    bool profile(const char* root)
    {
        for (int difficulty = 0; difficulty < 3; ++difficulty)
        {
            GameSession g;
            CHECK(mount(g, root));
            g.server.set_game_settings(difficulty, 0);
            auto* a = production(g);
            CHECK(a);
            auto* r = g.objects.get(a->m_hObject);
            const float factor = difficulty == 0 ? .25f : difficulty == 2 ? 1.5f : 1.f;
            DVector initial;
            CHECK(g.server.GetModelAnimUserDims(a->m_hObject, &initial, 0) == DE_OK);
            CHECK(a->RuntimeReady() && r->model &&
                  std::abs(r->state.m_Pos.y - (100 + initial.y * (a->m_vScale.y - 1))) < .003f &&
                  !(r->state.m_Flags & FLAG_GRAVITY));
            CHECK(a->m_damage.m_fHitPoints == 4001 * factor && a->m_damage.m_fArmorPoints == 200 * factor);
            CHECK(a->m_fFullHealth == 1 && a->m_bCreateHealth && a->m_InventoryMgr.damage_multiplier == factor);
            CHECK(dynamic_cast<CGideonShield*>(a->m_InventoryMgr.CurrentWeapon()) != nullptr);
            CHECK(a->AnimSound()->GetSoundRoot() == "sounds/enemies/Gideon" && a->m_MoveObj.initialized);
            CHECK(a->m_damage.ai_hit_localization && !a->m_damage.apply_damage_physics &&
                  a->m_InventoryMgr.m_nAttribStrength == 10);
            constexpr DBYTE weapon_ids[]{WEAP_GIDEON_SHIELD, WEAP_ZEALOT_ENERGYBLAST, WEAP_NAGA_EYEBEAM,
                                         WEAP_GIDEON_WIND};
            for (unsigned slot = 0; slot < 4; ++slot)
                CHECK(a->m_InventoryMgr.GetWeapon(slot) &&
                      a->m_InventoryMgr.GetWeapon(slot)->data.m_nType == weapon_ids[slot]);
            auto* second = production(g, {500, 300, 500});
            CHECK(second && second->AnimSound() == a->AnimSound());
            CHECK(g.objects.advance(.016) && a->runtime_updates == 1 && a->boss_health_fraction == 4001 * factor);
            std::cout << "Gideon difficulty=" << difficulty << " hp=" << a->m_damage.m_fHitPoints
                      << " captured=" << a->m_fFullHealth << " ratio=" << a->boss_health_fraction << '\n';
            CHECK(g.unload_world());
            AI_Mgr::RuntimeTables tables;
            AI_Mgr::ExchangeRuntimeTables(tables);
            for (const auto& t : tables)
                CHECK(!t);
        }
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(mount(g, root));
        std::string error;
        ObjectId previous{};
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && !g.objects.get(previous));
            Gideon* boss = nullptr;
            unsigned created = 0, failed = 0, unknown = 0, npcs = 0;
            for (const auto& e : g.entries)
            {
                created += e.state == EntryState::Created;
                failed += e.state == EntryState::Failed;
                unknown += e.state == EntryState::Unsupported;
            }
            for (auto id : g.objects.snapshot())
            {
                auto* base = g.objects.get(id)->game.get();
                npcs += dynamic_cast<AI_Mgr*>(base) != nullptr;
                if (auto* a = dynamic_cast<Gideon*>(base))
                {
                    CHECK(!boss);
                    boss = a;
                    previous = id;
                }
            }
            std::cout << "SubwayA created=" << created << " unknown=" << unknown << " failed=" << failed
                      << " npcs=" << npcs << " objects=" << g.objects.snapshot().size() << '\n';
            CHECK(boss && boss->RuntimeReady() && created == 204 && !failed && !unknown && npcs == 16);
            CHECK(g.server.GetAnimIndex(boss->m_hObject, "TALKING_02") == 19);
            CHECK(g.enter_player(error));
            std::cout << "Entered objects=" << g.objects.snapshot().size() << '\n';
            bool visible_through_glass = false, passive_before_camera_change = false;
            for (unsigned tick = 0; tick < 780; ++tick)
            {
                CHECK(g.step(1. / 60));
                // Allow the source's initial desynchronization and half-second
                // player pings to run before inspecting the still-active camera1.
                if (tick == 120)
                {
                    const auto* r = g.objects.get(previous);
                    const auto* observer = g.objects.get(g_hActiveCamera);
                    CHECK(observer && std::string_view(observer->state.m_Name) == "camera1");
                    SegmentHit hit;
                    CHECK(g.server.IntersectSegment(observer->state.m_Pos, r->state.m_Pos, observer->handle, hit) ==
                          SegmentResult::Hit);
                    const auto* obstruction = g.objects.get(hit.object);
                    const auto* glass =
                        obstruction ? dynamic_cast<const CDestructableBrush*>(obstruction->game.get()) : nullptr;
                    CHECK(glass && glass->destructable && glass->GetSurfaceType() == SURFTYPE_GLASS &&
                          std::string_view(obstruction->state.m_Name) == "nglass1");
                    std::cout << "Glass probe observer=" << (r->activation_probe.observer == observer->id)
                              << " visibility=" << int(r->activation_probe.visibility)
                              << " refreshed=" << r->activation_probe.refreshed << " pings=" << r->activation_pings
                              << " script updates=" << boss->script_updates << '\n';
                    CHECK(r->activation_probe.observer == observer->id &&
                          r->activation_probe.visibility == ActivationVisibility::Visible &&
                          r->activation_probe.refreshed && r->activation_pings > 0 && boss->script_updates > 10);
                    visible_through_glass = true;
                }
                if (tick == 390)
                    passive_before_camera_change =
                        boss->m_nState == AI_Mgr::StatePassive && g_hActiveCamera &&
                        std::string_view(g.objects.get(g_hActiveCamera)->state.m_Name) == "camera1";
                if (!tick)
                    std::cout << "First tick objects=" << g.objects.snapshot().size()
                              << " attachments=" << g.objects.attachments().size() << '\n';
            }
            const auto* boss_record = g.objects.get(previous);
            CHECK(boss_record && boss_record->life == Life::Alive && boss_record->game.get() == boss);
            std::cout << "Boss activity=" << boss_record->activity << " pings=" << boss_record->activation_pings
                      << " animation=" << boss_record->animation.animation << '\n';
            std::cout << "Boss updates=" << boss->runtime_updates << " state=" << boss->m_nState
                      << " health messages=" << g.client_shell.boss_health_messages << " error=" << boss->runtime_error
                      << '\n';
            std::cout << "Intro glass visibility=" << visible_through_glass
                      << " passive before camera switch=" << passive_before_camera_change
                      << " selections=" << boss->script_selections << " script updates=" << boss->script_updates
                      << " current=" << boss->CurrentScriptCommand().argument << '\n';
            if (boss->runtime_failed)
            {
                auto* r = g.objects.get(boss->m_hObject);
                std::cerr << "Script arg=" << boss->CurrentScriptCommand().argument
                          << " State error=" << boss->state_error << " requested clip=" << boss->requested_animation
                          << " pos=" << r->state.m_Pos.x << "," << r->state.m_Pos.y << "," << r->state.m_Pos.z
                          << " dims=" << r->dimensions.x << "," << r->dimensions.y << "," << r->dimensions.z
                          << " flags=" << r->state.m_Flags << '\n';
                for (unsigned i = 0; i < r->model->animations.size(); ++i)
                    std::cerr << i << ":" << r->model->animations[i].name << " ";
                std::cerr << '\n';
            }
            CHECK(boss->RuntimeReady() && boss->runtime_updates > 0 && g.client_shell.boss_health_messages > 0);
            CHECK(visible_through_glass && passive_before_camera_change && boss->m_nState == AI_Mgr::StatePassive &&
                  boss->CurrentScriptCommand().command == AI_Mgr::ScriptCommand::SetState &&
                  boss->CurrentScriptCommand().argument == "PASSIVE" && boss->script_selections == 2 &&
                  boss->script_failures == 0 && !g_hActiveCamera && !g.client_shell.view.object &&
                  !g.server_shell.Player(g.local_client)->activation_ping_failures);
            CHECK(g.run_world(error) && g.state == SessionState::Ready && error.empty());
            CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.objects.get(previous));
            CHECK(!AI_Mgr::m_dwNumMonster && !AI_Mgr::m_dwNumCabal && !CDestructable::m_dwNumDestructables);
        }
        return true;
    }
    bool arena(const char* root)
    {
        GameSession g;
        CHECK(mount(g, root));
        std::string error;
        g.server.set_game_settings(1, 0);
        g.world_path = "synthetic Gideon arena";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        auto* a = production(g, {0, 100, 500}, "IDLE");
        CHECK(a);
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld() && g.enter_player(error));
        g.server_shell.Player(g.local_client)->m_damage.m_bGodMode =
            true; // Observe the boss without ending this bounded arena.
        for (unsigned i = 0; i < 1800 && !a->fire_shots; ++i)
            CHECK(g.step(1. / 60));
        std::cout << "Arena updates=" << a->runtime_updates << " attack=" << a->attack_updates
                  << " shots=" << a->fire_shots << " state=" << a->m_nState << " error=" << a->runtime_error << '\n';
        CHECK(a->RuntimeReady() && a->runtime_updates > 0 && a->attack_updates > 0 && a->fire_shots > 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"defaults", defaults}, {"health", health},   {"client", client}, {"damage", damage},
        {"lifetime", lifetime}, {"profile", profile}, {"subway", subway}, {"arena", arena}};
    if (argc != 3 || !tests.contains(argv[1]))
        return 2;
    return tests.at(argv[1])(argv[2]) ? 0 : 1;
}
