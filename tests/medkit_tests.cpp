#include "app/movement_controls.h"
#include "game/original/ItemPickups.h"
#include "game/original/PowerupPickups.h"
#include "player_fixture.h"
#include <algorithm>
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
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        std::string e;
        CHECK(test_player::setup(g, root, true, true, true) && g.enter_player(e));
        return true;
    }
    CPlayerObj& player(GameSession& g)
    {
        return *g.server_shell.Player(g.local_client);
    }
    HOBJECT create(GameSession& g)
    {
        return g.objects.create(g.objects.find_class("MedKitPU"), {}, Creation::World,
                                {{"Pos", DVector{300, 100, 300}}});
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& inv = p.m_InventoryMgr;
        auto& d = p.m_damage;
        CHECK(inv.AddItem(1, 0) == InventoryResult::Ok && inv.medkit->count == 100);
        CHECK(inv.AddItem(1, 255) == InventoryResult::AlreadyHave && inv.AddItem(3, 1) == InventoryResult::Invalid);
        d.SetHitPoints(35);
        d.m_fMaxHitPoints = 100;
        CHECK(inv.SetActiveItem(1) == InventoryResult::Ok && d.m_fHitPoints == 100 && inv.medkit->count == 35);
        CHECK(inv.medkit->actual_gain == 65 && inv.medkit->action_messages == 1);
        CHECK(inv.SetActiveItem(1) == InventoryResult::NotAvailable && inv.medkit->count == 35);
        d.SetHitPoints(10);
        CHECK(inv.SetActiveItem(1) == InventoryResult::Ok && d.m_fHitPoints == 45 && inv.medkit->count == 0);
        CHECK(inv.SetActiveItem(1) == InventoryResult::NotAvailable);
        CHECK(inv.AddItem(1, 0) == InventoryResult::Ok && inv.medkit->count == 100);
        d.SetHitPoints(99.5f);
        CHECK(inv.SetActiveItem(1) == InventoryResult::NotAvailable && d.m_fHitPoints == 100 &&
              inv.medkit->count == 100);
        CHECK(inv.medkit->actual_gain == .5f && inv.medkit->action_messages == 3);
        d.SetHitPoints(125);
        CHECK(inv.SetActiveItem(1) == InventoryResult::Ok && inv.medkit->activation_result == -25 &&
              d.m_fHitPoints == 125);
        CHECK(inv.medkit->action_messages == 3 && inv.medkit->count == 100);
        d.m_fMaxHitPoints = 50;
        d.SetHitPoints(40);
        CHECK(inv.SetActiveItem(1) == InventoryResult::Ok && d.m_fHitPoints == 50 && inv.medkit->count == 40);
        CHECK(inv.medkit->requested_gain == 60 && inv.medkit->actual_gain == 10);
        d.SetHitPoints(50);
        CHECK(inv.SetActiveItem(1) == InventoryResult::Ok && d.m_fHitPoints == 50 && inv.medkit->count == 0);
        CHECK(inv.medkit->actual_gain == 0 && inv.medkit->action_messages == 5);
        std::string e;
        CHECK(g.pump_player_messages(e));
        CHECK(g.client_shell.item_actions == 5 && g.client_shell.item_sounds == 0 &&
              !g.client_shell.item_audio_error.empty());
        return true;
    }
    bool pickup(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& inv = p.m_InventoryMgr;
        for (unsigned i = 0; i < 3; ++i)
        {
            auto h = create(g);
            auto* r = g.objects.get(h);
            CHECK(r && r->model && r->model_error.empty());
            auto* item = dynamic_cast<MedKitPU*>(r->game.get());
            CHECK(item && item->resource_id == 302 && item->value.amount == 1);
            CHECK(item->pickup_sound == "sounds/powerups/inventory1.wav");
            if (i == 1)
            {
                CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && !item->picked_up);
                inv.medkit->count = 17;
            }
            if (i == 2)
            {
                inv.medkit->count = 0;
                p.m_bDead = true;
                CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && !item->picked_up);
                p.m_bDead = false;
                p.m_damage.m_bDead = true;
                CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && !item->picked_up);
                p.m_damage.m_bDead = false;
            }
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && item->picked_up);
            CHECK(inv.medkit->count == 100 && !g.server.IsObjectAlive(h));
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) == false);
            CHECK(g.objects.finish_frame() && !g.objects.get(h));
        }
        CHECK(g.server.pickup_reports.size() == 3 && !g.server.message_count());
        auto spawned = g.objects.create(g.objects.find_class("MedKitPU"), {}, Creation::String, {});
        auto* bounced = spawned ? dynamic_cast<MedKitPU*>(g.objects.get(spawned)->game.get()) : nullptr;
        CHECK(bounced && bounced->bouncing); // Original string-created SpawnItem bounce.
        CHECK(g.unload_world() && !g.server.string_count());
        return true;
    }
    bool commands(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& inv = p.m_InventoryMgr;
        CHECK(inv.AddItem(1, 1) == InventoryResult::Ok);
        p.m_damage.SetHitPoints(20);
        p.m_bDead = true;
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerItemCommand{1}));
        p.m_bDead = false;
        for (bool* gate : {&p.movement_blocked, &p.camera_movement_blocked, &p.command_run})
        {
            *gate = true;
            CHECK(!g.server_shell.OnMessage(g.local_client, PlayerItemCommand{1}));
            *gate = false;
        }
        CHECK(!g.server_shell.OnMessage(nullptr, PlayerItemCommand{1}) && inv.medkit->attempts == 0);
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerItemCommand{2}));
        g.client_shell.medkit_down = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell) && inv.medkit->count == 20 && p.m_damage.m_fHitPoints == 100);
        p.m_damage.SetHitPoints(90);
        CHECK(g.client_shell.UpdateUse(g.server_shell) && p.m_damage.m_fHitPoints == 90);
        g.client_shell.medkit_down = false;
        CHECK(g.client_shell.UpdateUse(g.server_shell));
        g.client_shell.medkit_down = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell));
        CHECK(p.m_damage.m_fHitPoints == 100 && inv.medkit->count == 10);
        return true;
    }
    bool transport(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto h = create(g);
        CHECK(h);
        auto* item = g.objects.get(h)->game.get();
        for (auto value : {PickupMessage{PickupKind::Item, 3, 1}, PickupMessage{PickupKind::Item, 1, .5},
                           PickupMessage{PickupKind::Item, 1, 256}, PickupMessage{PickupKind::Item, 1, -1}})
        {
            auto m = g.server.StartMessageToObject(item, p.m_hObject, add_item_message);
            CHECK(m && g.server.WritePickupMessage(m, value) != DE_OK);
            g.server.EndHMessageWrite(m);
        }
        auto m = g.server.StartMessageToObject(item, p.m_hObject, add_powerup_message);
        CHECK(m && g.server.WritePickupMessage(m, {PickupKind::Item, 1, 1}) != DE_OK);
        g.server.EndHMessageWrite(m);
        CHECK(g.server.SendToClient(g.local_client, InventoryAction{3}) == DE_INVALIDPARAMS);
        std::string sound_error;
        SoundRequest request;
        request.resource = "sounds/powerups/medikit.wav";
        CHECK(!g.server.PlayLocalSound(p.m_hObject, request, sound_error) && !sound_error.empty());
        request.space = SoundSpace::Local;
        request.loop = true;
        CHECK(!g.server.PlayLocalSound(p.m_hObject, request, sound_error));
        request.loop = false;
        request.streaming = true;
        CHECK(!g.server.PlayLocalSound(p.m_hObject, request, sound_error));
        request.streaming = false;
        CHECK(!g.server.PlayLocalSound(nullptr, request, sound_error));
        CHECK(p.m_InventoryMgr.AddItem(1, 1) == InventoryResult::Ok);
        for (unsigned i = 0; i < 256; ++i)
            CHECK(g.server.SendToClient(g.local_client, PlayerStats{}) == DE_OK);
        p.m_damage.SetHitPoints(90);
        CHECK(p.OnCommandItem(1) && p.m_damage.m_fHitPoints == 100 && p.m_InventoryMgr.medkit->count == 90);
        CHECK(p.m_InventoryMgr.medkit->action_messages == 0 && !p.m_InventoryMgr.medkit->error.empty());
        CHECK(!g.server.message_count());
        return true;
    }
    bool restore(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string e;
        auto& inv = player(g).m_InventoryMgr;
        CHECK(inv.AddItem(1, 1) == InventoryResult::Ok);
        inv.medkit->count = 27;
        PlayerRestoreState s;
        CHECK(g.capture_player(s, e) && s.inventory.medkit_charge == 27);
        auto old = g.client_shell.player;
        CHECK(g.leave_player());
        auto bad = s;
        bad.inventory.medkit_charge = 101;
        CHECK(!g.restore_player(bad, e) && !g.client_shell.player);
        CHECK(g.restore_player(s, e) && g.client_shell.player != old);
        auto& p = player(g);
        CHECK(p.m_InventoryMgr.medkit && p.m_InventoryMgr.medkit->count == 27);
        p.m_damage.SetHitPoints(50);
        CHECK(p.OnCommandItem(1) && p.m_damage.m_fHitPoints == 77);
        CHECK(p.m_InventoryMgr.medkit->count == 0);
        CHECK(g.capture_player(s, e) && s.inventory.medkit_charge == 0);
        CHECK(g.leave_player() && g.restore_player(s, e) && player(g).m_InventoryMgr.medkit->count == 0);
        CHECK(g.leave_player());
        auto h = g.objects.create(g.objects.find_class("CPlayerObj"), {}, Creation::World, {});
        auto* detached = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
        CHECK(detached);
        s.inventory.medkit_charge = 37;
        CHECK(detached->m_InventoryMgr.Load(s.inventory, InventoryLoadType::KeepAlive, e));
        CHECK(detached->m_InventoryMgr.medkit->count == 37);
        auto malformed = s.inventory;
        malformed.medkit_charge = 255;
        CHECK(!detached->m_InventoryMgr.Load(malformed, InventoryLoadType::KeepAlive, e) &&
              detached->m_InventoryMgr.medkit->count == 37);
        s.inventory.medkit_charge.reset();
        CHECK(detached->m_InventoryMgr.Load(s.inventory, InventoryLoadType::Restore, e) &&
              !detached->m_InventoryMgr.medkit);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        CHECK(p.m_InventoryMgr.AddItem(1, 1) == InventoryResult::Ok);
        CInvMedkit probe = *p.m_InventoryMgr.medkit;
        CHECK(g.objects.remove(p.m_hObject) && probe.Activate() == 0 && !probe.error.empty());
        CHECK(g.leave_player() && g.objects.finish_frame());
        CHECK(probe.Activate() == 0 && probe.count == 100 && !g.server.message_count());
        CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.string_count());
        return true;
    }
    bool input(const std::filesystem::path&)
    {
        MovementControls c;
        c.focused = true;
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = 7;
        e.key.scancode = SDL_SCANCODE_H;
        CHECK(c.consumes(e, false));
        c.event(e, 7);
        CHECK(c.sample_medkit(false));
        CHECK(!c.sample_medkit(true) && !c.sample_medkit(false));
        e.type = SDL_EVENT_KEY_UP;
        c.event(e, 7);
        CHECK(!c.sample_medkit(false));
        e.type = SDL_EVENT_KEY_DOWN;
        c.event(e, 7);
        CHECK(c.sample_medkit(false));
        e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        e.window.windowID = 7;
        c.event(e, 7);
        CHECK(!c.sample_medkit(false));
        e.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        c.event(e, 7);
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = 7;
        e.key.scancode = SDL_SCANCODE_H;
        e.key.repeat = true;
        c.event(e, 7);
        CHECK(!c.sample_medkit(false));
        e.key.repeat = false;
        c.event(e, 8);
        CHECK(!c.sample_medkit(false));
        c.event(e, 7);
        CHECK(c.sample_medkit(false));
        c.player_changed();
        CHECK(!c.sample_medkit(false));
        return true;
    }
    bool private_game(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string e;
        CHECK(g.resources.mount_game(cfg, e));
        g.server.audio.inspect_pcm = true;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", e));
            g.server_shell.start_point_name = "01_egressa";
            unsigned created = 0, unknown = 0, failed = 0;
            unsigned omitted = 0;
            HOBJECT h = nullptr;
            for (auto& entry : g.entries)
            {
                created += entry.state == EntryState::Created;
                unknown += entry.state == EntryState::Unsupported;
                failed += entry.state == EntryState::Failed;
                omitted += entry.state == EntryState::MissingWorldModel;
            }
            CHECK(created == 453 && unknown == 0 && failed == 0 && omitted == 1);
            for (auto id : g.objects.snapshot())
                if (dynamic_cast<MedKitPU*>(g.objects.get(id)->game.get()))
                    h = g.objects.get(id)->handle;
            CHECK(h && g.objects.get(h)->model && g.objects.get(h)->model_error.empty());
            std::cout << "CityHub MedKitPU model=" << g.objects.get(h)->state.m_Filename << "\n";
            CHECK(g.enter_player(e));
            auto& p = player(g);
            p.m_damage.SetHitPoints(45);
            // Authored pickup, diagnostic direct contact avoids claiming a level playthrough.
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && p.m_InventoryMgr.medkit->count == 100);
            g.client_shell.medkit_down = true;
            CHECK(g.client_shell.UpdateUse(g.server_shell));
            CHECK(p.m_damage.m_fHitPoints == 100 && p.m_InventoryMgr.medkit->count == 45);
            CHECK(g.pump_player_messages(e) && g.client_shell.item_actions == 1 && g.client_shell.item_sounds == 1);
            const auto voice = g.client_shell.item_voice;
            const auto* v = g.server.audio.get(voice);
            CHECK(v && v->world_owned && v->auto_remove && v->request.space == SoundSpace::Local &&
                  v->request.priority == 4);
            CHECK(v->clip && v->clip->frames() == 45312);
            CHECK(std::any_of(v->clip->samples.begin(), v->clip->samples.end(),
                              [](float sample) { return std::abs(sample) > .01f; }));
            CHECK(g.server.audio.advance(.1, {}, {}));
            std::cout << "Medkit cycle=" << cycle << " charges=" << int(p.m_InventoryMgr.medkit->count)
                      << " frames=" << v->clip->frames() << " peak=" << g.server.audio.peak << "\n";
            CHECK(g.server.audio.peak > 0);
            CHECK(g.leave_player() && g.server.audio.get(voice));
            CHECK(g.server.advance_audio(10, {}, {}));
            CHECK(!g.server.audio.get(voice));
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.message_count() &&
                  !g.server.string_count());
        }
        return true;
    }
    // Original StealthPU and CPlayerObj invisibility timer, blink, sounds and resets.
    bool stealth(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto pickup = g.objects.create(g.objects.find_class("StealthPU"), {}, Creation::World,
                                       {{"Pos", DVector{300, 100, 300}}});
        auto* item = pickup ? dynamic_cast<StealthPU*>(g.objects.get(pickup)->game.get()) : nullptr;
        CHECK(item && item->value.kind == PickupKind::Powerup && item->value.type == 6 && item->value.amount == 30 &&
              item->resource_id == 314 && item->file == "Stealth_pu");
        // Deliver the pickup's original MID_ADDPOWERUP payload.
        auto deliver = [&]
        {
            auto m = g.server.StartMessageToObject(g.server.HandleToObject(pickup), p.m_hObject, add_powerup_message);
            return m && g.server.WritePickupMessage(m, item->value) == DE_OK && g.server.EndMessage(m) == DE_OK;
        };
        const float start = g.objects.time();
        CHECK(deliver() && p.IsPowerupActive(PU_INVISIBLE) && p.powerup_pickups == 1);
        const auto* r = g.objects.get(p.m_hObject);
        CHECK(std::abs(r->alpha - .08f) < 1e-6f && !(r->state.m_Flags & FLAG_SHADOW));
        CHECK(p.m_nPowerupSoundStackPos == 1 && p.m_ePowerupSoundStack[0] == PU_INVISIBLE);
        CHECK(std::abs(p.m_fInvisibleTime - (start + 30)) < 1e-4f);
        // Still invisible 24 s later; the last five seconds blink.
        while (g.objects.time() < start + 24)
            CHECK(g.step(.1));
        CHECK(p.IsPowerupActive(PU_INVISIBLE) && std::abs(r->alpha - .08f) < 1e-6f);
        bool shown = false, hidden = false;
        while (g.objects.time() < start + 26)
            CHECK(g.step(.1));
        for (int i = 0; i < 60; ++i)
        {
            CHECK(g.step(.05));
            shown |= r->alpha == 1;
            hidden |= std::abs(r->alpha - .08f) < 1e-6f;
        }
        CHECK(shown && hidden && p.IsPowerupActive(PU_INVISIBLE));
        while (g.objects.time() < start + 31)
            CHECK(g.step(.1));
        CHECK(!p.IsPowerupActive(PU_INVISIBLE) && p.powerup_expiries == 1 && r->alpha == 1 &&
              (r->state.m_Flags & FLAG_SHADOW) && p.powerup_console_unavailable == 1 && !p.m_nPowerupSoundStackPos);
        // Death resets it (DoDeath -> ResetPowerups).
        CHECK(p.AddPowerup(6, 30) && p.IsPowerupActive(PU_INVISIBLE));
        p.ResetPowerups();
        CHECK(!p.IsPowerupActive(PU_INVISIBLE) && r->alpha == 1 && (r->state.m_Flags & FLAG_SHADOW));
        // The source saves no power state: capture drops it, the engine snapshot keeps the color.
        CHECK(p.AddPowerup(6, 30));
        PlayerRestoreState s;
        std::string e;
        CHECK(g.capture_player(s, e) && std::abs(s.alpha - .08f) < 1e-6f);
        CHECK(g.leave_player() && g.restore_player(s, e));
        auto& restored = player(g);
        CHECK(!restored.IsPowerupActive(PU_INVISIBLE) && std::abs(g.objects.get(restored.m_hObject)->alpha - .08f) < 1e-6f);
        CHECK(!p.AddPowerup(7, 30)); // Other powers stay unported.
        return true;
    }
    // Original InvulnerabilityPU: Willpower nigh invulnerability, glow flag, expiry voice.
    bool willpower(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto pickup = g.objects.create(g.objects.find_class("InvulnerabilityPU"), {}, Creation::World,
                                       {{"Pos", DVector{300, 100, 300}}});
        auto* item = pickup ? dynamic_cast<InvulnerabilityPU*>(g.objects.get(pickup)->game.get()) : nullptr;
        CHECK(item && item->value.type == 5 && item->value.amount == 30 && item->resource_id == 313 &&
              item->file == "Invulnerability_pu" && g.server.ValidateSpawnObject("InvulnerabilityPU"));
        auto m = g.server.StartMessageToObject(item, p.m_hObject, add_powerup_message);
        CHECK(m && g.server.WritePickupMessage(m, item->value) == DE_OK && g.server.EndMessage(m) == DE_OK);
        const float start = g.objects.time();
        CHECK(p.IsPowerupActive(PU_INVULNERABLE) && p.m_damage.m_bNighInvulnerable);
        CHECK(g.server.GetObjectUserFlags(p.m_hObject) & (1u << 9));
        CHECK(std::abs(p.m_fNighInvulnerableTime - (start + 30)) < 1e-4f);
        // Character damage is scaled by 0.05 while nigh invulnerable.
        const float before = p.m_damage.m_fHitPoints;
        auto d = g.server.StartMessageToObject(item, p.m_hObject, damage_message);
        CHECK(d && g.server.WriteDamage(d, {{0, 0, 1}, {}, 40, DamageType::Normal, pickup}) == DE_OK &&
              g.server.EndMessage(d) == DE_OK);
        CHECK(before - p.m_damage.m_fHitPoints <= 2.001f);
        while (g.objects.time() < start + 31)
            CHECK(g.step(.1));
        CHECK(!p.IsPowerupActive(PU_INVULNERABLE) && !p.m_damage.m_bNighInvulnerable &&
              !(g.server.GetObjectUserFlags(p.m_hObject) & (1u << 9)) && p.powerup_expiries == 1);
        // Capture drops the power; the source CDestructable Save keeps nigh invulnerability.
        CHECK(p.AddPowerup(5, 30));
        PlayerRestoreState s;
        std::string e;
        CHECK(g.capture_player(s, e) && s.damage.nigh_invulnerable);
        p.ResetPowerups();
        CHECK(!p.IsPowerupActive(PU_INVULNERABLE) && !p.m_damage.m_bNighInvulnerable);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"rules", rules},     {"pickup", pickup},     {"commands", commands}, {"transport", transport},
        {"restore", restore}, {"lifetime", lifetime}, {"input", input},       {"private", private_game},
        {"stealth", stealth},     {"willpower", willpower}};
    auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
