#include "audio_fixture.h"
#include "camera_fixture.h"
#include "engine/model_commands.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CameraObj.h"
#include <functional>
#include <set>
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
    std::shared_ptr<CAnim_Sound> table(std::string_view root = "ai")
    {
        auto a = std::make_shared<CAnim_Sound>();
        if (!a->SetSoundRoot(root))
            return {};
        return a;
    }
    AI_Mgr* actor(GameSession& g, std::shared_ptr<CAnim_Sound> a)
    {
        static const ClassInfo type{"AIKeyProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AI_Mgr>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Flags = FLAG_MODELKEYS;
        const auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* ai = r ? dynamic_cast<AI_Mgr*>(r->game.get()) : nullptr;
        if (ai && a && !ai->InitStatics(a))
            return nullptr;
        return ai;
    }
    bool key(GameSession& g, AI_Mgr* ai, std::string text)
    {
        std::vector<std::vector<std::string>> parsed;
        CHECK(model_commands(text, parsed));
        for (auto& c : parsed)
        {
            std::vector<char*> args;
            for (auto& v : c)
                args.push_back(v.data());
            ArgList a{args.data(), int(args.size())};
            CHECK(g.objects.engine_message(ai->m_hObject, MID_MODELSTRINGKEY, &a));
        }
        return true;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_audio::setup(g, root));
        test_player::Directory d;
        for (auto name : {"cue1.wav", "cue2.wav", "cue3.wav", "cue.wav.wav"})
            d.children["ai"].files[name] = test_audio::wave();
        d.children["ai"].files["cue.wav"] = test_audio::wave();
        d.children["ai"].files["long.wav"] = test_audio::wave(1, 16, 8000, 8000);
        auto w = test_model::Writer{};
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, d);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream out(root / "ai.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        CHECK(g.resources.mount(root / "ai.rez", error));
        return true;
    }
    bool sound(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto a = table();
        auto* ai = actor(g, a);
        CHECK(ai);
        CHECK(key(g, ai, "[play_sound:cue][sound_radius:500][sound_volume:70]"));
        CHECK(ai->audio_starts == 1 && ai->key_outcome == AIKeyOutcome::Played && ai->chance_roll >= 0);
        CHECK(g.server.audio.voices().size() == 1);
        const auto& v = g.server.audio.voices().begin()->second;
        CHECK(v.owner == ai->m_hObject && v.auto_remove && !v.world_owned && v.request.outer_radius == 500 &&
              v.request.inner_radius == 50 && v.request.volume == .7f && v.request.priority == 3);
        CHECK(g.server.advance_audio(.1, {}, {}));
        CHECK(g.server.audio.voices().empty());
        CHECK(key(g, ai, "[play_sound:cue.wav]"));
        CHECK(ai->audio_starts == 2 && ai->ai_sound_resource == "ai\\cue.wav.wav");
        CHECK(key(g, ai, "[play_sound:missing]"));
        CHECK(ai->key_outcome == AIKeyOutcome::PlaybackFailed && ai->key_failures == 1 && !ai->ai_key_error.empty());
        return true;
    }
    bool priority(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* ai = actor(g, table());
        CHECK(ai);
        auto* r = g.objects.get(ai->m_hObject);
        CHECK(key(g, ai, "[play_sound:cue][fire_key:head][RotY:1]"));
        CHECK(ai->no_target == 1 && ai->audio_keys == 0 && ai->motion_keys == 0);
        CHECK(ai->SetNewTarget(g.objects.create(g.objects.find_class("BaseClass"), {})));
        CHECK(ai->GetTarget());
        CHECK(key(g, ai, "[extra_key:x][play_sound:cue][fire_key:head][RotY:1]"));
        CHECK(ai->key_outcome == AIKeyOutcome::Unavailable && ai->unavailable_keys == 1 && ai->motion_keys == 1 &&
              ai->audio_starts == 0);
        CHECK(key(g, ai, "[play_sound:cue][extra_key:x][show_weapon:1]"));
        CHECK(ai->ai_key_error.find("uninitialized") != std::string::npos);
        CHECK(key(g, ai, "[extra_key:powerup][play_sound:cue]"));
        CHECK(ai->unavailable_keys == 3 && ai->audio_keys == 0);
        CHECK(key(g, ai, "[PLAY_SOUND:cue]"));
        CHECK(ai->pending_model_commands.size() == 1);
        CHECK(key(g, ai, "[UpVel:2]"));
        CHECK(r->velocity.y == 2 && ai->key_outcome == AIKeyOutcome::None);
        CHECK(g.objects.remove(ai->GetTarget()));
        CHECK(key(g, ai, "[fire_key:head][UpVel:10]"));
        CHECK(ai->key_outcome == AIKeyOutcome::Invalid && r->velocity.y == 2);
        CHECK(g.objects.finish_frame() && !ai->GetTarget() && ai->target_link_breaks == 1);
        CHECK(key(g, ai, "[fire_key:head][UpVel:10]"));
        CHECK(ai->key_outcome == AIKeyOutcome::NoTarget && r->velocity.y == 2);
        return true;
    }
    bool gates(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* ai = actor(g, table());
        CHECK(ai);
        g.server.trace_enabled = true;
        auto camera = g.objects.create(g.objects.find_class("CameraObj"), {}, Creation::World,
                                       {{"StartActive", true}, {"AllowPlayerMovement", false}});
        CHECK(camera && g_hActiveCamera == camera);
        CHECK(key(g, ai, "[play_sound:cue][ForwVel:9]"));
        CHECK(ai->camera_suppressed == 1 && ai->motion_keys == 0 && g.server.service_calls["RandomInt"] == 0);
        ai->m_nState = AI_Mgr::StateScript;
        CHECK(key(g, ai, "[play_sound:cue][ForwVel:9]"));
        CHECK(ai->script_suppressed == 1 && ai->motion_keys == 1 && g.server.service_calls["RandomInt"] == 0);
        ai->m_nState = AI_Mgr::StateIdle;
        auto* c = dynamic_cast<CameraObj*>(g.objects.get(camera)->game.get());
        CHECK(c);
        c->m_bPlayerMovement = true;
        CHECK(key(g, ai, "[play_sound:cue]"));
        CHECK(ai->audio_starts == 1); // Live source flag, not published client descriptor.
        CHECK(g.objects.remove(camera));
        CHECK(key(g, ai, "[play_sound:cue]"));
        CHECK(ai->audio_starts == 2); // Pending global ignored.
        CHECK(g.objects.finish_frame() && !g_hActiveCamera);
        return true;
    }
    bool random_cases(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* ai = actor(g, table());
        CHECK(ai);
        g.server.trace_enabled = true;
        std::set<int> variants;
        for (unsigned i = 0; i < 100; ++i)
        {
            CHECK(key(g, ai, "[play_sound:cue][sound_random:11][sound_chance:0]"));
            variants.insert(ai->selected_variant);
            CHECK(ai->selected_variant >= 1 && ai->selected_variant <= 3 &&
                  ai->key_outcome == AIKeyOutcome::ChanceSkipped);
        }
        CHECK(variants.size() == 3 && g.server.service_calls["RandomInt"] == 200 && ai->audio_starts == 0);
        for (auto max : {"0", "-10", "garbage", "9999999999999999999999999999"})
        {
            CHECK(key(g, ai, "[play_sound:cue][sound_random:" + std::string(max) + "][sound_chance:101]"));
            CHECK(ai->key_outcome == AIKeyOutcome::Played && ai->selected_variant <= 3);
        }
        CHECK(key(g, ai, "[play_sound:cue][sound_chance:-1][ForwVel:12]"));
        CHECK(ai->chance_suppressed == 101 && ai->motion_keys == 1 && ai->audio_starts == 4);
        CHECK(g.server.service_calls["RandomInt"] == 209);
        return true;
    }
    bool shared(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto a = table();
        std::weak_ptr<CAnim_Sound> weak = a;
        auto* one = actor(g, a);
        auto* two = actor(g, a);
        CHECK(one && two);
        a.reset();
        CHECK(!weak.expired());
        CHECK(key(g, one, "[play_sound:long][sound_voice:1]"));
        const auto id = one->AnimSound()->m_hVoxSound;
        CHECK(id);
        CHECK(key(g, two, "[play_sound:cue][sound_voice:-1][ForwVel:7]"));
        CHECK(two->key_outcome == AIKeyOutcome::VoiceBusy && two->busy_voices == 1 && two->motion_keys == 1);
        CHECK(one->AnimSound() == two->AnimSound() && g.server.audio.get(id)->owner == one->m_hObject);
        CHECK(!two->InitStatics(nullptr));
        CHECK(key(g, two, "[play_sound:cue]"));
        CHECK(two->audio_starts == 1); // Nonvoice is not serialized.
        CHECK(g.objects.remove(one->m_hObject) && g.objects.finish_frame());
        CHECK(!weak.expired());
        CHECK(g.server.advance_audio(0, {}, {}));
        CHECK(!g.server.audio.get(id));
        CHECK(key(g, two, "[play_sound:cue][sound_voice:1]"));
        CHECK(two->audio_starts == 2);
        CHECK(g.objects.remove(two->m_hObject) && g.objects.finish_frame() && weak.expired());
        CHECK(g.server.advance_audio(0, {}, {}));
        CHECK(g.server.audio.voices().empty());
        return true;
    }
    bool rejection(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* ai = actor(g, {});
        CHECK(ai);
        CHECK(key(g, ai, "[play_sound:cue]"));
        CHECK(ai->key_outcome == AIKeyOutcome::Invalid);
        CHECK(ai->InitStatics(table()));
        for (auto text : {"[play_sound:]", "[play_sound:cue][sound_radius:nan]", "[play_sound:cue][sound_volume:1x]",
                          "[play_sound:cue][sound_chance:2147483648]", "[play_sound:cue][sound_voice:]",
                          "[play_sound:cue][sound_random:]"})
        {
            CHECK(key(g, ai, text));
            CHECK(ai->key_outcome == AIKeyOutcome::Invalid && ai->audio_starts == 0);
        }
        for (auto text :
             {"[play_sound:cue][sound_radius:-1]", "[play_sound:cue][sound_volume:101]", "[play_sound:../cue]"})
        {
            CHECK(key(g, ai, text));
            CHECK(ai->key_outcome == AIKeyOutcome::PlaybackFailed);
        }
        CHECK(key(g, ai, "[play_sound:" + std::string(32, 'x') + "]"));
        CHECK(ai->key_outcome == AIKeyOutcome::Invalid);
        CHECK(key(g, ai, "[play_sound:cue][sound_radius:1e99]"));
        CHECK(ai->key_outcome == AIKeyOutcome::Invalid);
        CHECK(key(g, ai, std::string(4097, 'x')));
        CHECK(ai->key_outcome == AIKeyOutcome::Invalid);
        return true;
    }
    bool callbacks(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* ai = actor(g, table());
        CHECK(ai);
        auto* r = g.objects.get(ai->m_hObject);
        auto m = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *m, error));
        for (auto& k : m->animations[0].keys)
            k.text.clear();
        m->animations[0].keys[0].text = "[play_sound:cue][ForwVel:3];[play_sound:long][sound_voice:1]";
        r->model = m;
        r->activity = OBJSTATE_INACTIVE;
        CHECK(g.server.advance_models(0) && ai->model_keys == 0);
        r->activity = OBJSTATE_ACTIVE;
        CHECK(g.server.advance_models(0) && ai->model_keys == 2 && ai->audio_starts == 2 && ai->motion_keys == 1);
        CHECK(g.server.advance_audio(.05, {}, {}));
        CHECK(g.server.audio.get(ai->AnimSound()->m_hVoxSound)->cursor > 0);
        CHECK(m->animations[0].keys[0].text.ends_with("[sound_voice:1]"));
        const auto h = ai->m_hObject;
        CHECK(g.objects.remove(h));
        CHECK(!g.objects.engine_message(h, MID_MODELSTRINGKEY, nullptr));
        CHECK(g.server.advance_audio(0, {}, {}));
        CHECK(g.server.audio.voices().empty());
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            auto* ai = actor(g, table());
            CHECK(ai && ai->m_hObject != old);
            CHECK(key(g, ai, "[play_sound:long][sound_voice:1]"));
            CHECK(ai->audio_starts == 1 && ai->AnimSound()->m_hVoxSound);
            old = ai->m_hObject;
            CHECK(g.unload_world() && !g.objects.get(old) && g.server.audio.voices().empty());
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
    struct Profile
    {
        const char* model;
        const char* root;
    };
    const Profile profiles[]{{"m_civilian1", "civilian"}, {"f_civilian1", "f_civ"},    {"labtech", "civilian"},
                             {"m_cultist1", "m_cultist"}, {"f_cultist1", "f_cultist"}, {"gideon", "Gideon"}};
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(cfg, error));
        size_t keys = 0, starts = 0, failures = 0, sounds = 0, absent_targets = 0, unavailable = 0;
        for (auto profile : profiles)
        {
            auto* ai = actor(g, table("sounds/enemies/" + std::string(profile.root)));
            CHECK(ai);
            auto* r = g.objects.get(ai->m_hObject);
            CHECK(g.models.load("models/enemies/" + std::string(profile.model) + ".abc", r->model, error));
            for (unsigned clip = 0; clip < r->model->animations.size(); ++clip)
            {
                r->animation.animation = clip;
                r->animation.reset();
                r->animation.looping = false;
                CHECK(g.server.advance_models(r->model->animations[clip].duration / 1000.));
                if (ai->key_outcome == AIKeyOutcome::PlaybackFailed)
                    std::cout << profile.model << " missing " << ai->ai_sound_resource << " " << ai->ai_key_error
                              << "\n";
                CHECK(g.server.advance_audio(10, {}, {}));
            }
            std::cout << profile.model << " keys=" << ai->model_keys << " sound_keys=" << ai->audio_keys
                      << " starts=" << ai->audio_starts << " busy=" << ai->busy_voices << " no_target=" << ai->no_target
                      << " unavailable=" << ai->unavailable_keys << " failures=" << ai->key_failures << "\n";
            CHECK(ai->audio_keys == ai->audio_starts && ai->busy_voices == 0 && ai->camera_suppressed == 0 &&
                  ai->chance_suppressed == 0);
            CHECK(ai->AnimSound()->sound_failures == 0);
            sounds += ai->audio_keys;
            absent_targets += ai->no_target;
            unavailable += ai->unavailable_keys;
            keys += ai->model_keys;
            starts += ai->audio_starts;
            failures += ai->key_failures;
            const auto h = ai->m_hObject;
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
            CHECK(g.server.advance_audio(0, {}, {}));
        }
        std::cout << "TOTAL keys=" << keys << " starts=" << starts << " failures=" << failures << "\n";
        CHECK(keys == 147 && sounds == 140 && starts == 140 && failures == 1 && absent_targets == 6 &&
              unavailable == 1);
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool subway(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(cfg, error));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* ai = actor(g, table("sounds/enemies/m_cultist"));
            CHECK(ai);
            CHECK(key(g, ai, "[play_sound:idle][sound_random:4][sound_voice:1]"));
            CHECK(ai->key_outcome == AIKeyOutcome::CameraBlocked);
            CHECK(test_camera::complete_subway_intro(g));
            CHECK(key(g, ai, "[play_sound:idle][sound_random:4][sound_voice:1]"));
            if (ai->audio_starts != 1)
                std::cerr << ai->ai_sound_resource << " " << ai->ai_key_error << "\n";
            CHECK(ai->audio_starts == 1 && g.server.advance_audio(.1, {}, {}));
            CHECK(g.server.audio.get(ai->AnimSound()->m_hVoxSound)->cursor > 0);
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"sound", sound},     {"priority", priority},   {"gates", gates},         {"random", random_cases},
        {"shared", shared},   {"rejection", rejection}, {"callbacks", callbacks}, {"lifetime", lifetime},
        {"catalog", catalog}, {"subway", subway}};
    auto i = cases.find(argv[1]);
    return i != cases.end() && i->second(argv[2]) ? 0 : 1;
}
