#include "audio_fixture.h"
#include "game/game_settings.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
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
        return std::abs(a - b) < .003f;
    }
    bool setup(GameSession& g, const std::filesystem::path& path)
    {
        CHECK(test_player::setup(g, path, false));
        test_player::Directory d;
        auto model = test_model::abc(false, true).b;
        const std::vector<std::uint8_t> old{4, 0, 'r', 'o', 'o', 't'}, replacement{4, 0, 'h', 'e', 'a', 'd'};
        auto it = std::search(model.begin(), model.end(), old.begin(), old.end());
        CHECK(it != model.end());
        std::copy(replacement.begin(), replacement.end(), it);
        d.files["corpse.abc"] = model;
        d.files["skin.dtx"] = {1, 2, 3}; // Indexed only, not renderer evidence.
        for (auto name : {"cue.wav", "cue1.wav", "cue2.wav", "cue3.wav"})
            d.children["sounds"].files[name] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, d);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream f(path / "corpse.rez", std::ios::binary);
        f.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        f.close();
        std::string e;
        CHECK(g.resources.mount(path / "corpse.rez", e));
        return true;
    }
    AI_Mgr* actor(GameSession& g)
    {
        static const ClassInfo type{"DeathProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AI_Mgr>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Flags = FLAG_VISIBLE;
        s.m_Pos = {0, 60, 0};
        auto h = g.objects.create(&type, s);
        auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
        if (!a || g.server.SetModelFilenames(h, "corpse.abc", "skin.dtx") != DE_OK)
            return nullptr;
        auto table = std::make_shared<CAnim_Sound>();
        if (!table->SetSoundRoot("sounds") || !a->InitStatics(table) || !a->m_MoveObj.Init(h) || !a->m_MoveObj.Update())
            return nullptr;
        for (unsigned i = 0; i < 12; ++i)
            table->m_nAnim_DEATH[i] = i + 1;
        a->m_vDims = {12, 60, 12};
        a->m_vScale = {2, 2, 2};
        a->m_damage.node_hit = 2;
        a->m_damage.side_hit = 0;
        if (!a->m_damage.Init(h))
            return nullptr;
        a->m_damage.SetHitPoints(0);
        a->m_damage.m_fMaxHitPoints = 100;
        return a;
    }
    bool mappings(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* a = actor(g);
        CHECK(a);
        constexpr int nodes[]{1, 2, 4, 7, 10, 14};
        for (int side : {0, 6})
            for (int i = 0; i < 6; ++i)
            {
                a->m_damage.node_hit = nodes[i];
                a->m_damage.side_hit = side;
                CHECK(a->SetNewState(AI_Mgr::StateDying));
                auto clip = g.server.GetModelAnimation(a->m_hObject);
                CHECK(a->DispatchState() && a->death_slot == i + side && a->m_nCorpseType == unsigned(i + side + 1));
                CHECK(a->Metacmd == 1 && g.server.GetModelAnimation(a->m_hObject) == clip && !a->last_corpse);
            }
        a->m_dwFlags = 0x400;
        bool first = false, second = false;
        for (int i = 0; i < 100; ++i)
        {
            CHECK(a->SetNewState(AI_Mgr::StateDying) && a->DispatchState());
            first |= a->death_slot == 0;
            second |= a->death_slot == 1;
        }
        CHECK(first && second);
        a->Metacmd = 2;
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateInactive && a->Metacmd == 2);
        return true;
    }
    bool transfer(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* a = actor(g);
        CHECK(a);
        auto source = a->m_hObject;
        CHECK(a->SetNewState(AI_Mgr::StateDying) && a->DispatchState());
        DVector shifted{300, 500, 0};
        CHECK(g.server.TeleportObject(source, &shifted) == DE_OK);
        CHECK(a->DispatchState() && a->m_bRemoveMe && a->corpses_created == 1);
        auto h = a->last_corpse;
        auto* c = dynamic_cast<CCorpse*>(g.server.HandleToObject(h));
        auto* r = g.objects.get(h);
        CHECK(c && c->configured && c->initialized && r->model && c->measurements.hit_spheres_valid);
        CHECK(!(g.server.GetObjectFlags(source) & FLAG_VISIBLE) && g.server.GetModelAnimation(h) == 2 &&
              !g.server.GetModelLooping(h));
        CHECK(near(r->state.m_Pos.y, 41.001f) && near(r->state.m_Pos.x, 0) && r->dimensions.y == 41 &&
              r->state.m_Scale.y == 2);
        CHECK(c->m_damage.m_fHitPoints == 50 && c->m_damage.m_fMass == 200 && !(r->state.m_Flags & FLAG_SOLID));
        CHECK(g.objects.remove(source) && g.objects.finish_frame() && g.server.IsObjectAlive(h));
        CHECK(g.server.advance_models(.2) && g.objects.advance(.2) && c->animation_done);
        CHECK(g.server.advance_physics(.3) && g.objects.advance(.3) && g.server.IsObjectAlive(h));
        CHECK(g.objects.clear() && g.objects.attachments().empty() && !g.server.message_count());
        return true;
    }
    bool failures(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        CHECK(a->SetNewState(AI_Mgr::StateDying) && a->DispatchState());
        auto before = g.objects.snapshot().size();
        a->m_vScale = {0, 1, 1};
        CHECK(!a->DispatchState() && !a->m_bRemoveMe && (g.server.GetObjectFlags(h) & FLAG_VISIBLE));
        CHECK(g.objects.finish_frame() && g.objects.snapshot().size() == before && a->corpses_created == 0);
        a->m_vScale = {1, 1, 1};
        a->m_nCorpseType = 9999;
        CHECK(!a->DispatchState() && g.objects.snapshot().size() == before);
        a->m_nCorpseType = 0;
        CHECK(a->CreateCorpse() && a->death_slot >= 0 && a->death_slot < 12);
        std::string model = "unchanged", skin = "unchanged";
        CHECK(!g.server.GetModelFilenames(nullptr, model, skin) && model == "unchanged" && skin == "unchanged");
        a->m_damage.reaction_damage_type = DamageType::Fire;
        CHECK(!a->CreateCorpse() && a->corpses_created == 1);
        CHECK(!g.objects.create(g.objects.find_class("CCorpse"), {}, Creation::World));
        return true;
    }
    CCorpse* corpse(GameSession& g)
    {
        ObjectCreateStruct s;
        SAFE_STRCPY(s.m_Filename, "corpse.abc");
        SAFE_STRCPY(s.m_SkinName, "skin.dtx");
        s.m_Pos = {0, 30, 0};
        auto h = g.objects.create(g.objects.find_class("CCorpse"), s);
        auto* c = dynamic_cast<CCorpse*>(g.server.HandleToObject(h));
        if (!c || !c->Setup("sounds", false))
            return nullptr;
        g.server.SetModelAnimation(h, 0);
        g.server.SetModelLooping(h, false);
        return c;
    }
    bool lifetime(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* c = corpse(g);
        CHECK(c);
        auto h = c->m_hObject;
        CHECK(g.server.advance_models(.2) && g.objects.advance(.2) && c->animation_done && c->pool_requested &&
              !c->pool_error.empty());
        CHECK(g.objects.advance(31) && g.server.IsObjectAlive(h) && !c->remove_time);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        g.server.set_game_settings(DIFFICULTY_MEDIUM, GAMETYPE_BLOODBATH);
        c = corpse(g);
        CHECK(c);
        h = c->m_hObject;
        CHECK(g.server.advance_models(.2) && g.objects.advance(.2) && c->remove_time > g.objects.time());
        CHECK(g.objects.advance(29.8) && g.server.IsObjectAlive(h));
        CHECK(g.objects.advance(.3) && !g.server.IsObjectAlive(h));
        return true;
    }
    bool sounds(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* c = corpse(g);
        CHECK(c);
        auto key = [&](const char* text)
        {
            char* args[]{const_cast<char*>(text)};
            ArgList a{args, 1};
            return c->OnStringKey(&a);
        };
        CHECK(key("[play_sound:cue][sound_radius:500][sound_volume:70]"));
        auto first = c->last_sound;
        auto* voice = g.server.audio.get(first);
        CHECK(voice && voice->auto_remove && voice->request.priority == 3 && near(voice->request.volume, .7f));
        CHECK(key("[play_sound:cue][sound_random:99]") && c->last_sound != first &&
              g.server.audio.voices().size() == 2);
        CHECK(!key("[play_sound:cue][sound_radius:+-1]") && !key("[play_sound:missing]") && !c->sound_error.empty());
        CHECK(g.server.advance_audio(.2, {}, {}) && g.server.audio.voices().empty());
        CHECK(key("[play_sound:cue]"));
        auto id = c->last_sound;
        CHECK(g.objects.remove(c->m_hObject) && g.objects.finish_frame() && g.server.advance_audio(.01, {}, {}) &&
              !g.server.audio.get(id));
        return true;
    }
    bool gates(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* a = actor(g);
        CHECK(a);
        a->m_bCabal = false;
        for (auto type : {DamageType::Fire, DamageType::Explode})
        {
            a->m_damage.reaction_damage_type = type;
            CHECK(!a->HandleDamage() && !a->damage_reaction_error.empty());
        }
        a->m_damage.reaction_damage_type = DamageType::Normal;
        a->m_damage.m_fLastDamagePercent = .9f;
        CHECK(!a->HandleDamage());
        a->m_damage.m_fLastDamagePercent = .899f;
        CHECK(a->HandleDamage() && a->kill_voice_roll >= 0 && a->m_nState == AI_Mgr::StateRecoil);
        a->m_damage.m_fLastDamagePercent = 100;
        a->m_dwFlags = 0x800;
        CHECK(a->HandleDamage());
        auto* c = corpse(g);
        CHECK(c);
        auto m = g.server.StartMessageToObject(a, c->m_hObject, damage_message);
        CHECK(g.server.WriteDamage(m, {{0, 0, 1}, {0, 30, 0}, 100, DamageType::Death, a->m_hObject}) == DE_OK &&
              g.server.EndMessage(m) == DE_OK);
        CHECK(c->m_damage.m_fHitPoints == 50 && c->damage_refusals == 1 && !c->damage_error.empty() &&
              c->error.empty());
        CHECK(g.server.advance_models(.2) && g.objects.advance(.2) && c->animation_done);
        return true;
    }
    bool accumulation(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* a = actor(g);
        CHECK(a);
        a->m_nState = AI_Mgr::StateDying; // Isolate shared damage bookkeeping from AI reactions.
        a->m_damage.SetHitPoints(100);
        a->m_damage.SetArmorPoints(20);
        auto send = [&](float amount, DamageType type)
        {
            auto m = g.server.StartMessageToObject(a, a->m_hObject, damage_message);
            return m && g.server.WriteDamage(m, {{0, 0, 1}, {0, 60, 0}, amount, type, a->m_hObject}) == DE_OK &&
                   g.server.EndMessage(m) == DE_OK;
        };
        CHECK(send(10, DamageType::Normal) && near(a->m_damage.m_fLastDamageAmount, 7) &&
              near(a->m_damage.m_fLastDamagePercent, .07f));
        CHECK(send(10, DamageType::Normal) && near(a->m_damage.m_fLastDamagePercent, .14f));
        CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr) && a->m_damage.m_fLastDamagePercent == 0 &&
              a->m_damage.m_fLastDamageAmount == 0);
        CHECK(send(5, DamageType::Death) && a->m_damage.IsDead() && near(a->m_damage.m_fLastDamageAmount, 105) &&
              near(a->m_damage.m_fLastDamagePercent, 100.05f));
        auto* c = corpse(g);
        CHECK(c);
        DVector position{0, 100, 0};
        CHECK(g.server.TeleportObject(c->m_hObject, &position) == DE_OK);
        CHECK(g.server.advance_models(.2) && g.objects.advance(.2) && c->animation_done && !c->pool_requested &&
              c->pool_result == SegmentResult::Miss);
        return true;
    }
    bool catalog(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = path;
        std::string e;
        CHECK(g.resources.mount_game(cfg, e));
        WorldModel floor;
        floor.points = {{-2000, 0, -2000}, {2000, 0, -2000}, {2000, 0, 2000}, {-2000, 0, 2000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, e));
        g.server.collision_ready = true;
        for (int cycle = 0; cycle < 2; ++cycle)
            for (auto type : {"CultistAI", "CivilianAI"})
            {
                auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World,
                                          {{"Pos", DVector{0, 100, 0}},
                                           {"AIState", std::string("IDLE")},
                                           {"AIWeapon1", std::string("BERETTA")}});
                auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
                CHECK(a && a->RuntimeReady());
                if (cycle == 0 && std::string_view(type) == "CultistAI")
                    for (auto clip : a->AnimSound()->m_nAnim_DEATH)
                        std::cout << "Death clip " << clip << " duration "
                                  << g.objects.get(h)->model->animations[clip].duration << '\n';
                Damage damage{{0, 0, 1}, {0, 100, 0}, 1000, DamageType::Death, h};
                if (cycle == 0)
                {
                    // Diagnostic NEVERGIB tests the source forced-death branch.
                    a->m_dwFlags |= 0x800;
                    a->m_damage.node_hit = 2;
                }
                else
                {
                    // Real Cabal headshot; low-health Civilian torso kill through normal localization.
                    DVector node;
                    DRotation rotation;
                    const bool cultist = std::string_view(type) == "CultistAI";
                    CHECK(g.server.GetModelNodeTransform(h, cultist ? "head" : "torso", &node, &rotation));
                    damage = {{0, 0, 1}, {node.x, node.y, node.z - 1}, 3, DamageType::Normal, h};
                    if (!cultist)
                    {
                        a->m_damage.SetHitPoints(1);
                        a->m_damage.SetArmorPoints(0);
                    }
                }
                auto m = g.server.StartMessageToObject(a, h, damage_message);
                CHECK(g.server.WriteDamage(m, damage) == DE_OK && g.server.EndMessage(m) == DE_OK);
                CHECK(a->m_damage.IsDead() && !a->runtime_failed);
                CHECK(a->AI_Update() && a->death_starts == 1 && a->AI_Update() && a->corpses_created == 1);
                auto child = a->last_corpse;
                auto* c = dynamic_cast<CCorpse*>(g.server.HandleToObject(child));
                CHECK(c);
                CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && g.objects.finish_frame() &&
                      !g.objects.get(h) && g.objects.get(child));
                bool animated = false;
                for (int frame = 0; frame < 600 && !c->animation_done; ++frame)
                {
                    CHECK(g.server.advance_models(.02) && g.server.advance_physics(.02) && g.objects.advance(.02) &&
                          g.server.advance_audio(.02, {}, {}));
                    animated |= g.objects.get(child)->animation.time > 0;
                }
                CHECK(animated && c->animation_done && c->initialized && c->error.empty() && c->sound_starts > 0 &&
                      g.objects.get(child)->model);
                std::cout << type << " cycle " << cycle << ": source removed; corpse clip "
                          << g.server.GetModelAnimation(child) << ", sound starts " << c->sound_starts
                          << ", pool request " << c->pool_requested << '\n';
                CHECK(g.objects.clear() && g.server.advance_audio(.01, {}, {}) && g.server.audio.voices().empty() &&
                      g.objects.attachments().empty() && !g.server.message_count());
            }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"mappings", mappings}, {"transfer", transfer}, {"failures", failures},         {"lifetime", lifetime},
        {"sounds", sounds},     {"gates", gates},       {"accumulation", accumulation}, {"catalog", catalog}};
    if (argc != 3)
        return 2;
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
