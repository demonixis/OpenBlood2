#include "audio_fixture.h"
#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include <cmath>
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
    bool setup(GameSession& game, const std::filesystem::path& root, bool audio = true)
    {
        CHECK(test_player::setup(game, root));
        if (audio)
        {
            test_player::Directory dir;
            auto& weapons = dir.children["sounds"].children["weapons"];
            weapons.children["beretta"].files["fire.wav"] = test_audio::wave();
            weapons.children["knife"].files["fire.wav"] = test_audio::wave(1, 16, 8000, 8000);
            weapons.children["knife"].files["alt.wav"] = test_audio::wave(1, 16, 8000, 2400);
            test_model::Writer w;
            w.b.resize(168);
            w.b[0] = w.b[62] = w.b[124] = 13;
            w.b[1] = w.b[63] = w.b[125] = 10;
            w.b[126] = 26;
            w.patch(127, 1);
            auto [offset, size] = test_player::write_dir(w, dir);
            w.patch(131, offset);
            w.patch(135, size);
            std::ofstream out(root / "weapon-audio.rez", std::ios::binary);
            out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
            out.close();
            std::string error;
            CHECK(game.resources.mount(root / "weapon-audio.rez", error));
        }
        std::string error;
        CHECK(game.enter_player(error));
        game.server.audio.inspect_pcm = true;
        return true;
    }
    WeaponFire shot(GameSession& g, DBYTE type = WEAP_BERETTA, bool alt = false)
    {
        auto pos = g.objects.get(g.client_shell.player)->state.m_Pos;
        pos.y += 36;
        return {pos, {0, 0, 1}, 7, type, alt, false};
    }
    struct Listener : AI_Mgr
    {
        unsigned action = 0;
        bool saw_consumed_ammo = false;
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read) override
        {
            const auto result = AI_Mgr::ObjectMessageFn(sender, message, read);
            if (message == 1000)
            {
                auto* player = dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(sender));
                saw_consumed_ammo = player && player->m_InventoryMgr.GetAmmoCount(1) == 49;
                if (action == 1)
                    g_pServerDE->RemoveObject(sender);
                else if (action >= 2)
                {
                    DVector pos{500, 100, 300};
                    g_pServerDE->TeleportObject(m_hObject, &pos);
                    if (action == 3)
                    {
                        // Messages rewind at end: skip the string length, then read text as an invalid length.
                        g_pServerDE->ReadFromMessageDWord(read);
                        if (auto string = g_pServerDE->ReadFromMessageHString(read))
                            g_pServerDE->FreeString(string);
                    }
                }
            }
            return result;
        }
    };
    Listener* listener(GameSession& g, DVector pos)
    {
        static ClassInfo type{"WeaponAudioListener",
                              nullptr,
                              []() -> std::unique_ptr<BaseClass> { return std::make_unique<Listener>(); },
                              {}};
        type.parent = g.objects.find_class("CBaseCharacter");
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct state;
        state.m_Pos = pos;
        auto handle = g.objects.create(&type, state);
        auto* actor = dynamic_cast<Listener*>(g.server.HandleToObject(handle));
        if (!actor || !actor->InsertMe())
            return nullptr;
        actor->m_fHearingDist = 2000;
        actor->m_nState = actor->m_nLastState = AI_Mgr::StateIdle;
        return actor;
    }
    bool voices(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* player = g.server_shell.Player(g.local_client);
        auto* w = player->m_InventoryMgr.CurrentWeapon();
        for (unsigned i = 0; i < 4; ++i)
            CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired);
        CHECK(w->sound_attempts == 4 && w->sounds_played == 4 && g.server.audio.voices().size() == 4);
        const auto previous = w->fire_sounds;
        CHECK(w->fire_sound_ends[0] == w->fire_sound_ends[3]);
        const auto ends = w->fire_sound_ends;
        g.server.client_resources = nullptr;
        w->PlayFireSound(false, shot(g).position);
        g.server.client_resources = &g.resources;
        CHECK(w->sound_failures == 1 && w->fire_sounds == previous && w->fire_sound_ends == ends &&
              g.server.audio.voices().size() == 4 && w->sound_evictions == 0);
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g, WEAP_BERETTA, true)) == FireResult::Fired);
        CHECK(w->sound_evictions == 1 && !g.server.audio.get(previous[0]) && w->fire_sounds[1] == previous[1]);
        const auto* voice = g.server.audio.get(w->fire_sounds[0]);
        CHECK(voice && voice->request.speaker == player->m_hObject && voice->request.priority == 6);
        CHECK(voice->request.outer_radius == 2000 && voice->request.inner_radius == 200 &&
              voice->request.reverb_requested);
        CHECK(!voice->auto_remove && !voice->request.loop && voice->clip->frames() == 800);
        float duration = 9;
        CHECK(g.server.GetSoundDuration(voice->id, duration) && std::abs(duration - .1f) < 1e-6f);
        CHECK(!g.server.GetSoundDuration(0, duration) && duration == 0);
        g.objects.get(player->m_hObject)->state.m_Pos.x += 3;
        CHECK(g.server.advance_audio(.11, g.client_shell.position, {}));
        CHECK(g.server.audio.peak > 0 && g.server.audio.get(w->fire_sounds[0])->done);
        CHECK(g.server.audio.get(w->fire_sounds[0])->position.x == g.objects.get(player->m_hObject)->state.m_Pos.x);
        std::string error;
        CHECK(g.select_weapon(0, error) && g.step(.016)); // Inactive inventory slot still gets original sound cleanup.
        CHECK(g.server.audio.voices().empty() && w->sounds_released == 5);
        auto* knife = player->m_InventoryMgr.CurrentWeapon();
        for (bool alt : {false, true, false, false})
            knife->PlayFireSound(alt, shot(g).position);
        const auto knife_before = knife->fire_sounds;
        knife->PlayFireSound(false, shot(g).position);
        CHECK(knife->sound_evictions == 1 && !g.server.audio.get(knife_before[1]) &&
              knife->fire_sounds[0] == knife_before[0]);
        CHECK(g.unload_world() && g.server.audio.voices().empty() && g.server.string_count() == 0);
        return true;
    }
    bool hearing(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto fire = shot(g);
        auto* a = listener(g, vector_add(fire.position, {100, 0, 0}));
        CHECK(a);
        const auto strings = g.server.string_count();
        auto* w = g.server_shell.Player(g.local_client)->m_InventoryMgr.CurrentWeapon();
        CHECK(g.server_shell.OnMessage(g.local_client, fire) == FireResult::Fired);
        CHECK(a->hearing_reactions == 1 && a->m_nState == AI_Mgr::StateSearchVisual && a->saw_consumed_ammo);
        CHECK(w->fire_sound_alert.complete && w->fire_sound_alert.delivered == 1 && w->sounds_played == 1);
        CBeretta npc;
        npc.Init(a->m_hObject, &a->m_InventoryMgr, false);
        npc.PlayFireSound(false, fire.position);
        CHECK(npc.sounds_played == 1 && g.server.audio.get(npc.last_fire_sound)->request.priority == 4);
        CHECK(g.server.string_count() == strings && g.server.message_count() == 0);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root, false));
        auto fire = shot(g);
        auto* a = listener(g, vector_add(fire.position, {100, 0, 0}));
        CHECK(a);
        auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        auto* w = inv.CurrentWeapon();
        CHECK(g.server_shell.OnMessage(g.local_client, fire) == FireResult::Fired);
        CHECK(w->sound_failures == 1 && !w->fire_sound_error.empty() && !w->sounds_played && a->hearing_reactions == 1);
        CHECK(w->fire_sound_alert.delivered == 1 && g.server.audio.voices().empty() && inv.GetAmmoCount(1) == 49);
        inv.SetAmmoCount(1, 0);
        CHECK(g.server_shell.OnMessage(g.local_client, fire) == FireResult::Dry);
        CHECK(w->sound_attempts == 1 && a->hearing_reactions == 1); // Selected empty-fire paths are empty strings.
        inv.SetAmmoCount(1, 50);
        g.server.collision_ready = false;
        CHECK(g.server_shell.OnMessage(g.local_client, fire) == FireResult::Unavailable);
        CHECK(w->sound_attempts == 1 && inv.GetAmmoCount(1) == 50);
        return true;
    }
    bool callbacks(const std::filesystem::path& root)
    {
        for (unsigned action : {1u, 2u, 3u})
        {
            GameSession g;
            CHECK(setup(g, root));
            const auto fire = shot(g);
            auto* a = listener(g, vector_add(fire.position, {0, 0, 80}));
            CHECK(a);
            a->action = action;
            auto* body = g.objects.get(a->m_hObject);
            body->state.m_Flags |= FLAG_SOLID;
            DVector dims{15, 15, 15};
            CHECK(g.server.SetObjectDims(a->m_hObject, &dims) == DE_OK);
            auto* w = g.server_shell.Player(g.local_client)->m_InventoryMgr.CurrentWeapon();
            CHECK(g.server_shell.OnMessage(g.local_client, fire) == FireResult::Fired);
            CHECK(a->saw_consumed_ammo && !w->last_shot.damage_delivered);
            CHECK(w->fire_sound_alert.eligible == 1);
            if (action == 3)
                CHECK(!w->fire_sound_alert.complete && w->fire_sound_alert.delivered == 0 &&
                      w->sound_alert_failures == 1);
            else
                CHECK(w->fire_sound_alert.delivered == 1);
            if (action == 1)
                CHECK(w->last_shot.interrupted && !g.server.IsObjectAlive(w->m_hOwner) &&
                      g.server.audio.voices().empty());
            else
                CHECK(!w->last_shot.interrupted && w->last_shot.collision == SegmentResult::Miss);
            CHECK(g.objects.finish_frame());
            CHECK(g.unload_world() && !g.server.client_count() && !g.server.message_count() &&
                  !g.server.string_count());
        }
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        auto* w = inv.CurrentWeapon();
        w->PlayFireSound(false, shot(g).position);
        CHECK(w->last_fire_sound && g.server.audio.voices().size() == 1);
        inv.ClearWeapons();
        CHECK(g.server.audio.voices().empty());
        CHECK(g.unload_world() && g.server.string_count() == 0);
        return true;
    }
    bool private_audio(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        SoundId previous = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_subwaya", error) && g.enter_player(error));
            auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
            for (unsigned slot : {1u, 0u})
                for (bool alt : {false, true})
                {
                    auto* w = inv.GetWeapon(slot);
                    w->PlayFireSound(
                        alt,
                        shot(g).position); // Diagnostic direct audio request, not a shot through an active cinematic.
                    CHECK(w->last_fire_sound > previous && w->fire_sound_error.empty() && w->fire_sound_alert.complete);
                    previous = w->last_fire_sound;
                    const auto* voice = g.server.audio.get(previous);
                    CHECK(voice && voice->clip->frames() > 0);
                    std::cout << w->fire_sound_resource << " frames=" << voice->clip->frames()
                              << " rate=" << voice->clip->rate << " priority=" << unsigned(voice->request.priority)
                              << '\n';
                }
            g.server.audio.inspect_pcm = true;
            CHECK(g.server.advance_audio(.1, g.client_shell.position, {}) && g.server.audio.peak > 0);
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.objects.snapshot().empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    const std::filesystem::path root = argv[2];
    if (name == "voices")
        return voices(root) ? 0 : 1;
    if (name == "hearing")
        return hearing(root) ? 0 : 1;
    if (name == "failures")
        return failures(root) ? 0 : 1;
    if (name == "callbacks")
        return callbacks(root) ? 0 : 1;
    if (name == "lifetime")
        return lifetime(root) ? 0 : 1;
    if (name == "private")
        return private_audio(root) ? 0 : 1;
    return 2;
}
