#include "audio_fixture.h"
#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
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
    template <class T = AI_Mgr> T* actor(GameSession& g, DVector pos = {})
    {
        static const ClassInfo type{std::is_same_v<T, AI_Mgr> ? "HearingProbe" : "RemovingHearingProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<T>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_Pos = pos;
        SAFE_STRCPY(s.m_Name, type.name.c_str());
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<T*>(r->game.get()) : nullptr;
    }
    bool send(GameSession& g, HOBJECT sender, HOBJECT target, const char* text)
    {
        auto string = g.server.CreateString(text);
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(sender), target, 1000);
        CHECK(string && message && g.server.WriteToMessageHString(message, string) == DE_OK);
        g.server.FreeString(string);
        CHECK(g.server.EndMessage(message) == DE_OK);
        return true;
    }
    HOBJECT source(GameSession& g, DVector pos, const char* name = "source")
    {
        ObjectCreateStruct s;
        s.m_Pos = pos;
        SAFE_STRCPY(s.m_Name, name);
        return g.objects.create(g.objects.find_class("BaseClass"), s);
    }
    bool hearing(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* a = actor(g);
        CHECK(a);
        a->m_fHearingDist = 100;
        auto h = source(g, {49, 0, 0});
        CHECK(send(g, h, a->m_hObject, "SOUND_TRIGGER 2 999 999 999;"));
        CHECK(a->m_nState == AI_Mgr::StateSearchVisual && a->m_vTargetPos.x == 49 && !a->GetTarget());
        CHECK(a->hearing_reactions == 1 && a->heard_distance == 49);
        a->SetNewState(AI_Mgr::StateIdle);
        g.objects.get(h)->state.m_Pos.x = 50;
        CHECK(a->HandleTrigger(h, "sound_trigger 2"));
        CHECK(a->m_nState == AI_Mgr::StateIdle && a->face_commands == 1 && a->Metacmd == 1);
        CHECK(a->trigger_outcome == AI_Mgr::TriggerOutcome::Turned);
        g.objects.get(h)->state.m_Pos.x = 100;
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 2") && a->hearing_reactions == 3);
        g.objects.get(h)->state.m_Pos.x = 100.01f;
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 2") && a->hearing_ignored == 1);
        CHECK(a->HandleTrigger(a->m_hObject, "SOUND_TRIGGER 1") && a->hearing_ignored == 2);
        a->m_bHasTargeted = true;
        a->m_fWaitForIdleTime = 120;
        g.objects.get(h)->state.m_Pos.x = 80;
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 2") && a->m_nState == AI_Mgr::StateSearchVisual);
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool gunfire(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* a = actor(g);
        auto* other = actor(g, {20, 0, 0});
        CHECK(a && other);
        a->m_fHearingDist = 100;
        CHECK(a->HandleTrigger(other->m_hObject, "SOUND_TRIGGER 0") && a->hearing_ignored == 1);
        auto h = source(g, {20, 0, 0});
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 0")); // Original reversed IsKindOf accepts BaseClass.
        CHECK(a->m_nState == AI_Mgr::StateSearchVisual && a->hearing_reactions == 1);
        a->SetNewState(AI_Mgr::StateIdle);
        a->m_fHearingDist = 0;
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 0") && a->hearing_ignored == 2);
        return true;
    }
    bool impact(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* a = actor(g);
        CHECK(a);
        auto h = source(g, {90000, 0, 0});
        a->m_dwFlags = 8;
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 1 1.2 -3.4 500"));
        CHECK(a->m_nState == AI_Mgr::StateDodge && a->m_vTrackObjPos.z == 500 && a->hearing_reactions == 1);
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 1") && a->hearing_ignored == 1); // Guards before coordinate parsing.
        a->SetNewState(AI_Mgr::StateIdle);
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 1 1 2 3") && a->hearing_ignored == 2); // last Dodge
        a->m_nLastState = AI_Mgr::StatePassive;
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 1 1 2 3") && a->hearing_ignored == 3);
        a->m_nLastState = AI_Mgr::StateIdle;
        CHECK(!a->HandleTrigger(h, "SOUND_TRIGGER 1 1 nan 3") && a->m_vTrackObjPos.z == 500);
        return true;
    }
    bool commands(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* a = actor(g);
        CHECK(a);
        auto h = source(g, {20, 0, 0}, "target with spaces");
        CHECK(a->HandleTrigger(h, "setstate passive; TARGET missing") && a->m_nState == AI_Mgr::StatePassive);
        CHECK(a->trigger_commands == 1); // Source command early return.
        CHECK(a->HandleTrigger(h, "TARGET \"target with spaces\"; SETSTATE IDLE"));
        CHECK(a->GetTarget() == h && a->m_nState == AI_Mgr::StateAttackFar && a->Metacmd == 1);
        CHECK(!a->HandleTrigger(h, "TARGET absent") && a->GetTarget() == h);
        const auto state = a->m_nState;
        CHECK(!a->HandleTrigger(h, "SCRIPT INTERRUPTABLE; WAIT 1;") && a->m_nState == state);
        CHECK(!a->HandleTrigger(h, "SETSTATE IDLE; \"unterminated") && a->m_nState == state);
        CHECK(a->HandleTrigger(h, "SETSTATE unknown") && a->m_nState == AI_Mgr::StateInactive);
        a->m_nState = AI_Mgr::StateScript;
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 2; SETSTATE IDLE") && a->m_nState == AI_Mgr::StateScript);
        a->m_dwScriptFlags = 2;
        CHECK(a->HandleTrigger(h, "SOUND_TRIGGER 2; SETSTATE IDLE") && a->m_nState == AI_Mgr::StateIdle);
        CHECK(!a->HandleTrigger(h, "unsupported"));
        CHECK(!a->HandleTrigger(h, std::string(1280, 'a')));
        CHECK(!a->HandleTrigger(h, std::string_view("SETSTATE IDLE\0x", 15)));
        return true;
    }
    bool relay(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* a = actor(g);
        auto* b = actor(g);
        CHECK(a && b);
        SAFE_STRCPY(g.objects.get(b->m_hObject)->state.m_Name, "receiver");
        a->trigger_relay_target = "receiver";
        b->trigger_relay_target = "no matches";
        CHECK(send(g, a->m_hObject, a->m_hObject, "TrIgGeR"));
        CHECK(a->trigger_relays == 1 && b->trigger_calls == 1 && b->trigger_failures == 0);
        a->trigger_relay_target = "HearingProbe";
        CHECK(!a->HandleTrigger(nullptr, "TRIGGER"));
        CHECK(a->trigger_failures == 1 && a->trigger_error.find("Reentrant") != std::string::npos);
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0 && g.server.list_count() == 0);
        CHECK(a->HandleTrigger(nullptr, "SETSTATE IDLE")); // Guard released after rejection.
        return true;
    }
    struct ListenerPlayer : CPlayerObj
    {
        unsigned alerts = 0;
        DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT value) override
        {
            return BaseClass::EngineMessageFn(message, data, value);
        } // Test-only message recipient, no player entry.
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read) override
        {
            if (message == 1000)
                ++alerts;
            return BaseClass::ObjectMessageFn(sender, message, read);
        }
    };
    bool broadcast(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* a = actor(g, {10, 0, 0});
        auto* b = actor(g, {20, 0, 0});
        CHECK(a && b);
        a->m_fHearingDist = b->m_fHearingDist = 100;
        b->m_bCabal = false;
        CHECK(a->InsertMe() && b->InsertMe());
        auto h = source(g, {0, 0, 0});
        SoundAlertReport report;
        CHECK(SendSoundTrigger(h, 2, {}, 10, report));
        CHECK(report.visited == 2 && report.delivered == 1 && a->hearing_reactions == 1 && b->hearing_events == 0);
        CHECK(SendSoundTrigger(h, 2, {}, 20, report) && report.delivered == 2 && b->hearing_reactions == 1);
        CHECK(SendSoundTrigger(a->m_hObject, 2, {10, 0, 0}, 100, report) && report.delivered == 1 &&
              report.skipped == 1);
        a->m_nState = AI_Mgr::StateIdle;
        a->m_nLastState = AI_Mgr::StateIdle;
        a->m_dwFlags = 8;
        CHECK(SendSoundTrigger(h, 1, {1.26f, -2.34f, 3.56f}, 100, report));
        CHECK(std::abs(a->m_vTrackObjPos.x - 1.3f) < .001f && std::abs(a->m_vTrackObjPos.y + 2.3f) < .001f);
        CHECK(!SendSoundTrigger(h, 2, {}, -1, report) && !report.complete && report.delivered == 0);
        CHECK(!SendSoundTrigger(h, 9, {}, 20, report));
        static const ClassInfo listener_type{"HearingPlayerSubclass",
                                             g.objects.find_class("CPlayerObj"),
                                             []() -> std::unique_ptr<BaseClass>
                                             { return std::make_unique<ListenerPlayer>(); },
                                             {}};
        CHECK(g.objects.register_class(listener_type));
        auto listener_handle = g.objects.create(&listener_type);
        auto* listener = dynamic_cast<ListenerPlayer*>(g.server.HandleToObject(listener_handle));
        CHECK(listener);
        AI_Mgr::InsertPlayer(*listener);
        g.objects.get(listener_handle)->activity = OBJSTATE_INACTIVE;
        g.objects.get(a->m_hObject)->activity = OBJSTATE_INACTIVE;
        CHECK(SendSoundTrigger(h, 2, {}, 100, report) && report.delivered == 4 && listener->alerts == 2);
        a->m_nState = AI_Mgr::StateIdle;
        const auto ignored = a->hearing_ignored;
        CHECK(a->HandleTrigger(listener_handle, "SOUND_TRIGGER 0") && a->hearing_ignored == ignored + 1);

        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    struct Removing : AI_Mgr
    {
        HOBJECT victim = nullptr;
        inline static unsigned destroyed = 0;
        ~Removing() override
        {
            ++destroyed;
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read) override
        {
            if (message == 1000 && victim)
                GetServerDE()->RemoveObject(victim);
            return AI_Mgr::ObjectMessageFn(sender, message, read);
        }
    };
    bool lifetime(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        for (unsigned cycle = 0; cycle < 4; ++cycle)
        {
            auto* b = actor(g, {20, 0, 0});
            auto* a = actor<Removing>(g, {10, 0, 0});
            CHECK(a && b);
            CHECK(b->InsertMe() && a->InsertMe()); // Head insertion: remover first.
            auto h = source(g, {});
            a->victim = b->m_hObject;
            SoundAlertReport report;
            CHECK(SendSoundTrigger(h, 2, {}, 100, report));
            CHECK(report.delivered == 1 && report.skipped == 1 && !g.server.IsObjectAlive(b->m_hObject));
            CHECK(g.objects.finish_frame());
            auto* later = actor(g, {30, 0, 0});
            CHECK(later);
            later->m_bCabal = false;
            CHECK(later->InsertMe());
            a->victim = h;
            CHECK(!SendSoundTrigger(h, 2, {}, 100, report) && report.delivered == 1 && !report.complete);
            CHECK(later->hearing_events == 0);
            CHECK(!g.server.IsObjectAlive(h) && g.objects.finish_frame());
            CHECK(!SendSoundTrigger(h, 2, {}, 100, report));
            CHECK(g.objects.clear() && AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0);
        }
        CHECK(Removing::destroyed == 4 && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool audio(const char* root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        CHECK(a->HandleTrigger(nullptr, "PLAYSOUND cue.wav"));
        CHECK(g.server.audio.voices().size() == 1);
        const auto& voice = g.server.audio.voices().begin()->second;
        CHECK(voice.request.outer_radius == 5000 && voice.request.inner_radius == 500 && voice.auto_remove);
        CHECK(a->HandleTrigger(nullptr, "PLAYSOUND cue.wav -4"));
        CHECK(!a->HandleTrigger(nullptr, "PLAYSOUND cue.wav 1.5"));
        CHECK(!a->HandleTrigger(nullptr, "PLAYSOUND cue.wav +-1"));
        CHECK(!a->HandleTrigger(nullptr, "PLAYSOUND missing.wav"));
        CHECK(a->HandleTrigger(nullptr, "PLAYANIMATION animation0") == false); // No model, explicit failure.
        auto model = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *model, error));
        auto* record = g.objects.get(a->m_hObject);
        record->model = model;
        record->state.m_Pos = {0, 100, 0};
        record->dimensions = {1, 1, 1};
        CHECK(a->HandleTrigger(nullptr, "PLAYANIMATION " + model->animations.front().name));
        CHECK(record->animation.animation == 0 && !record->animation.looping);
        CHECK(!a->HandleTrigger(nullptr, "PLAYANIMATION missing") && record->animation.animation == 0);
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("start")}, {"Pos", DVector{40, 100, 0}}}));
        CHECK(g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(player && a->InsertMe());
        a->m_fHearingDist = 400;
        CHECK(player->PlayPlayerSound("cue.wav", 400, 100, false));
        CHECK(a->hearing_reactions == 1 && player->sound_alert.delivered == 1);
        CHECK(a->SetNewState(AI_Mgr::StateIdle));
        CHECK(!player->PlayPlayerSound("missing.wav", 400, 100, false));
        CHECK(a->hearing_reactions == 2 && player->sound_alert.delivered == 1 && player->unavailable_sound_alerts == 0);
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            CHECK(!g.objects.get(old) && g.objects.find_class("CultistAI"));
            auto* player = g.server_shell.Player(g.local_client);
            auto* a = actor(g);
            CHECK(player && a);
            // Keep this single-listener hearing fixture isolated from the new authored NPCs.
            // Production acquisition/update/contacts are exercised by ai_runtime.subway/arena.
            g.objects.get(player->m_hObject)->state.m_Pos = {0, 10000, 0};
            DVector pos;
            CHECK(g.server.GetObjectPos(player->m_hObject, &pos) == DE_OK);
            g.objects.get(a->m_hObject)->state.m_Pos = vector_add(pos, {40, 0, 0});
            a->m_fHearingDist = 400;
            CHECK(a->InsertMe());
            CHECK(player->PlayPlayerSound("sounds/player/footsteps/metal/leftm.wav", 400, 70, false));
            CHECK(player->sound_alert.complete && player->sound_alert.delivered == 1 && a->hearing_reactions == 1);
            CHECK(a->m_nState == AI_Mgr::StateSearchVisual && !a->GetTarget());
            CHECK(a->SetNewState(AI_Mgr::StateIdle));
            CHECK(!player->PlayPlayerSound("missing-preservation-probe.wav", 400, 70, false));
            CHECK(a->hearing_reactions == 2 && player->sound_alert.delivered == 1);
            std::cout << "cycle=" << cycle << " reactions=" << a->hearing_reactions
                      << " delivered=" << player->sound_alert.delivered << " failures=" << a->trigger_failures << '\n';
            CHECK(a->trigger_failures == 0 && player->unavailable_sound_alerts == 0);
            old = a->m_hObject;
            CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"hearing", hearing},     {"gunfire", gunfire},   {"impact", impact}, {"commands", commands}, {"relay", relay},
        {"broadcast", broadcast}, {"lifetime", lifetime}, {"audio", audio},   {"subway", subway}};
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
