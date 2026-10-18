#include "app/movement_controls.h"
#include "game/original/ItemPickups.h"
#include "player_fixture.h"
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
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(owner);
        CInvFlashlight f(owner);
        CHECK(f.count == 100 && f.remaining == 120 && !f.active && !f.light && !f.AddItem(255));
        CHECK(f.ActivateItem() && f.active && f.light);
        CHECK(g.server.GetObjectFlags(f.light) == FLAG_FORCECLIENTUPDATE && g.server.GetObjectUserFlags(f.light) == 1);
        CHECK(g.objects.advance(.25) && f.Update() && f.count == 100 && f.remaining == 119.75 && !f.AddItem(0));
        CHECK(g.objects.advance(.5) && f.Update() && f.count == 99 && f.AddItem(0) && f.remaining == 120);
        CHECK(g.objects.advance(60) && f.Update() && f.count == 50 && f.remaining == 60);
        CHECK(f.ActivateItem() && !f.active && !g.server.GetObjectUserFlags(f.light));
        CHECK(g.objects.advance(20) && f.Update() && f.remaining == 60);
        CHECK(f.ActivateItem() && g.objects.advance(60) && f.Update() && f.count == 0 && f.remaining == 0 && !f.active);
        CHECK(!f.ActivateItem() && f.AddItem(255) && f.ActivateItem());
        f.remaining = .01f;
        CHECK(g.objects.advance(.1) && f.Update() && f.remaining < 0 && f.count == 0 && !f.active);
        CInvFlashlight empty(owner);
        empty.remaining = 0;
        empty.count = 0;
        CHECK(!empty.ActivateItem() && empty.light && !empty.active && !g.server.GetObjectUserFlags(empty.light));
        CHECK(g.objects.advance(61) && !f.Update() && !f.error.empty());
        return true;
    }
    bool pickup(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        for (unsigned i = 0; i < 3; ++i)
        {
            auto h = g.objects.create(g.objects.find_class("FlashlightPU"), {}, Creation::World,
                                      {{"Pos", DVector{300, 100, 300}}});
            auto* r = g.objects.get(h);
            CHECK(r && r->model && r->model_error.empty());
            auto* item = dynamic_cast<FlashlightPU*>(r->game.get());
            CHECK(item && item->resource_id == 301 && item->value.type == 2 && item->value.amount == 1);
            if (i)
            {
                CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && !item->picked_up);
                p.m_InventoryMgr.flashlight->count = 99;
                p.m_bDead = true;
                CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && !item->picked_up);
                p.m_bDead = false;
            }
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && item->picked_up);
            CHECK(p.m_InventoryMgr.flashlight->count == 100 && p.m_InventoryMgr.flashlight->remaining == 120);
            CHECK(!g.server.IsObjectAlive(h) && g.objects.finish_frame() && !g.objects.get(h));
        }
        auto spawned = g.objects.create(g.objects.find_class("FlashlightPU"), {}, Creation::String, {});
        auto* bounced = spawned ? dynamic_cast<FlashlightPU*>(g.objects.get(spawned)->game.get()) : nullptr;
        CHECK(bounced && bounced->bouncing); // Original string-created SpawnItem bounce.
        CHECK(!g.server.message_count() && g.unload_world() && !g.server.string_count());
        return true;
    }
    bool commands(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        CHECK(p.m_InventoryMgr.AddItem(2, 0) == InventoryResult::Ok);
        auto& f = *p.m_InventoryMgr.flashlight;
        p.m_bDead = true;
        CHECK(!p.OnCommandItem(2) && !f.light);
        p.m_bDead = false;
        for (bool* gate : {&p.movement_blocked, &p.camera_movement_blocked, &p.command_run})
        {
            *gate = true;
            CHECK(!p.OnCommandItem(2) && !f.light);
            *gate = false;
        }
        g.client_shell.flashlight_down = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell) && f.active && f.actions == 1);
        CHECK(g.client_shell.UpdateUse(g.server_shell) && f.active && f.actions == 1);
        g.client_shell.flashlight_down = false;
        CHECK(g.client_shell.UpdateUse(g.server_shell));
        g.client_shell.flashlight_down = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell) && !f.active && f.actions == 2);
        std::string e;
        CHECK(g.pump_player_messages(e) && g.client_shell.item_actions == 2 && g.client_shell.item_sounds == 0 &&
              !g.client_shell.item_audio_error.empty());
        for (unsigned i = 0; i < 256; ++i)
            CHECK(g.server.SendToClient(g.local_client, PlayerStats{}) == DE_OK);
        CHECK(f.ActivateItem() && f.active && f.actions == 2 && !f.error.empty());
        return true;
    }
    bool ray(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        CHECK(p.m_InventoryMgr.AddItem(2, 1) == InventoryResult::Ok);
        auto& f = *p.m_InventoryMgr.flashlight;
        auto& fx = g.client_shell.flashlight;
        CHECK(f.ActivateItem());
        DVector pos{0, 100, 0};
        CHECK(g.server.TeleportObject(p.m_hObject, &pos) == DE_OK);
        p.m_fPitch = 1.57079632679f;
        p.m_fYaw = 0;
        CHECK(f.Update() && fx.Update(g.server, f.light, p.m_hObject));
        CHECK(fx.visible && fx.result == SegmentResult::Hit && !fx.hit.object && std::abs(fx.distance - 100) < .01f);
        CHECK(std::abs(fx.light.radius - 40) < .01f && std::abs(fx.light.color.x - .95f) < .001f);
        CHECK(std::abs(fx.shade_position.y - 100) < .01f && !fx.shade_available && fx.Current(g.server).radius > 0);
        // Current player pitch, source body origin; the authoritative body is excluded from the client ray.
        CHECK(std::abs(g.objects.get(f.light)->state.m_Pos.y - 100) < .01f);
        p.m_fPitch = -1.57079632679f;
        CHECK(f.Update() && fx.Update(g.server, f.light, p.m_hObject) && !fx.visible);
        p.m_fPitch = 1.57079632679f;
        DVector near{0, 2, 0};
        CHECK(g.server.TeleportObject(p.m_hObject, &near) == DE_OK && f.Update() &&
              fx.Update(g.server, f.light, p.m_hObject));
        CHECK(fx.visible && fx.distance == 10 && fx.light.radius == 4);
        DVector far{0, 2000, 0};
        CHECK(g.server.TeleportObject(p.m_hObject, &far) == DE_OK && f.Update() &&
              fx.Update(g.server, f.light, p.m_hObject));
        CHECK(fx.visible && std::abs(fx.light.color.x - 132.f / 255) < .0001f &&
              std::abs(fx.light.radius - 800) < .01f);
        CHECK(f.ActivateItem() && fx.Current(g.server).radius == 0 && fx.Update(g.server, f.light, p.m_hObject) &&
              !fx.visible);
        CHECK(f.ActivateItem());
        g.server.collision_ready = false;
        CHECK(!fx.Update(g.server, f.light, p.m_hObject) && fx.result == SegmentResult::Unavailable &&
              fx.Current(g.server).radius == 0 && !fx.error.empty());
        g.server.collision_ready = true;
        auto* child = g.objects.get(f.light);
        child->state.m_Pos.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(!fx.Update(g.server, f.light, p.m_hObject) && fx.result == SegmentResult::Invalid);
        return true;
    }
    bool restore(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& inv = player(g).m_InventoryMgr;
        CHECK(inv.AddItem(2, 1) == InventoryResult::Ok && inv.flashlight->ActivateItem());
        inv.flashlight->remaining = 32;
        inv.flashlight->count = 27;
        auto old = inv.flashlight->light;
        PlayerRestoreState s;
        std::string e;
        CHECK(g.capture_player(s, e) && s.inventory.flashlight->remaining == 32 && s.inventory.flashlight->count == 27);
        CHECK(g.leave_player() && !g.server.IsObjectAlive(old));
        auto bad = s;
        bad.inventory.flashlight->remaining = 121;
        CHECK(!g.restore_player(bad, e) && !g.client_shell.player);
        CHECK(g.restore_player(s, e));
        auto& f = *player(g).m_InventoryMgr.flashlight;
        CHECK(f.light && f.light != old && !f.active && !g.server.GetObjectUserFlags(f.light) && f.remaining == 32 &&
              f.count == 27);
        CHECK(g.leave_player());
        auto h = g.objects.create(g.objects.find_class("CPlayerObj"), {}, Creation::World, {});
        auto* p = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
        CHECK(p);
        for (auto type : {InventoryLoadType::KeepAlive, InventoryLoadType::Restore})
        {
            CHECK(p->m_InventoryMgr.Load(s.inventory, type, e));
            auto child = p->m_InventoryMgr.flashlight->light;
            CHECK(!p->m_InventoryMgr.flashlight->active && p->m_InventoryMgr.flashlight->remaining == 32);
            for (float invalid : {121.f, -61.f, std::numeric_limits<float>::quiet_NaN()})
            {
                auto b = s.inventory;
                b.flashlight->remaining = invalid;
                CHECK(!p->m_InventoryMgr.Load(b, type, e) && p->m_InventoryMgr.flashlight->light == child);
            }
            auto b = s.inventory;
            b.flashlight->count = 101;
            CHECK(!p->m_InventoryMgr.Load(b, type, e) && p->m_InventoryMgr.flashlight->light == child);
            p->m_InventoryMgr.flashlight->remaining = std::numeric_limits<float>::quiet_NaN();
            InventoryState output;
            output.medkit_charge = 42;
            CHECK(!p->m_InventoryMgr.Save(output, e) && output.medkit_charge == 42 && !output.flashlight);
            p->m_InventoryMgr.flashlight->remaining = 32;
            auto absent = s.inventory;
            absent.flashlight.reset();
            CHECK(p->m_InventoryMgr.Load(absent, type, e) && !p->m_InventoryMgr.flashlight &&
                  !g.server.IsObjectAlive(child));
            CHECK(g.objects.finish_frame());
        }
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        {
            GameSession g;
            CHECK(setup(g, root));
            auto& p = player(g);
            CHECK(p.m_InventoryMgr.AddItem(2, 1) == InventoryResult::Ok);
            auto& f = *p.m_InventoryMgr.flashlight;
            auto& fx = g.client_shell.flashlight;
            CHECK(f.ActivateItem());
            p.m_fPitch = 1.57079632679f;
            CHECK(f.Update() && fx.Update(g.server, f.light, p.m_hObject) && fx.visible);
            auto child = f.light;
            CHECK(g.objects.remove(child) && fx.Current(g.server).radius == 0 && !f.Update() && !f.error.empty());
            CHECK(!f.ActivateItem() && g.objects.finish_frame());
            CHECK(g.leave_player() && !g.objects.get(child) && g.unload_world());
            CHECK(g.server.audio.voices().empty() && !g.server.message_count() && !g.server.string_count());
        }
        GameSession next_game;
        CHECK(setup(next_game, root));
        auto& next = player(next_game);
        CHECK(next.m_InventoryMgr.AddItem(2, 1) == InventoryResult::Ok);
        CHECK(next.m_InventoryMgr.flashlight->ActivateItem());
        auto child = next.m_InventoryMgr.flashlight->light;
        CHECK(next_game.objects.remove(next.m_hObject) && next_game.objects.finish_frame() &&
              !next_game.server.IsObjectAlive(child));
        CHECK(next_game.leave_player() && next_game.unload_world());
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        CHECK(p.m_InventoryMgr.AddItem(2, 1) == InventoryResult::Ok);
        auto& f = *p.m_InventoryMgr.flashlight;
        g.client_shell.flashlight_down = true;
        CHECK(g.step(.01) && f.active);
        g.client_shell.flashlight_down = false;
        const auto before = f.remaining;
        for (unsigned i = 0; i < 100; ++i)
            CHECK(g.step(.01));
        CHECK(f.remaining < before - .9f && f.remaining > before - 1.1f && f.updates >= 100);
        const auto time = f.remaining;
        g.client_shell.flashlight.Current(g.server);
        CHECK(f.remaining == time);
        CHECK(g.objects.remove(f.light) && !g.step(.01) && !f.update_ok && !f.error.empty());
        return true;
    }
    bool input(const std::filesystem::path&)
    {
        MovementControls c;
        c.focused = true;
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = 7;
        e.key.scancode = SDL_SCANCODE_L;
        CHECK(c.consumes(e, false));
        c.event(e, 7);
        CHECK(c.sample_flashlight(false));
        CHECK(!c.sample_flashlight(true) && !c.sample_flashlight(false));
        e.type = SDL_EVENT_KEY_UP;
        c.event(e, 7);
        CHECK(!c.sample_flashlight(false));
        e.type = SDL_EVENT_KEY_DOWN;
        c.event(e, 7);
        CHECK(c.sample_flashlight(false));
        e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        e.window.windowID = 7;
        c.event(e, 7);
        CHECK(!c.sample_flashlight(false));
        e.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        c.event(e, 7);
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = 7;
        e.key.scancode = SDL_SCANCODE_L;
        e.key.repeat = true;
        c.event(e, 7);
        CHECK(!c.sample_flashlight(false));
        e.key.repeat = false;
        c.event(e, 8);
        CHECK(!c.sample_flashlight(false));
        c.event(e, 7);
        CHECK(c.sample_flashlight(false));
        c.player_changed();
        CHECK(!c.sample_flashlight(false));
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
            unsigned made = 0, unknown = 0, failed = 0;
            unsigned omitted = 0;
            HOBJECT h = nullptr;
            for (const auto& entry : g.entries)
            {
                made += entry.state == EntryState::Created;
                unknown += entry.state == EntryState::Unsupported;
                failed += entry.state == EntryState::Failed;
                omitted += entry.state == EntryState::MissingWorldModel;
            }
            CHECK(made == 453 && unknown == 0 && failed == 0 && omitted == 1);
            for (auto id : g.objects.snapshot())
                if (dynamic_cast<FlashlightPU*>(g.objects.get(id)->game.get()))
                    h = g.objects.get(id)->handle;
            CHECK(h && g.objects.get(h)->model && g.objects.get(h)->model_error.empty() && g.enter_player(e));
            auto& p = player(g);
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && p.m_InventoryMgr.flashlight);
            for (unsigned i = 0; i < 2; ++i)
            {
                CHECK(p.OnCommandItem(2) && g.pump_player_messages(e));
                auto voice = g.client_shell.item_voice;
                auto* v = g.server.audio.get(voice);
                CHECK(v && v->clip && v->request.space == SoundSpace::Local && v->request.priority == 4 &&
                      v->world_owned);
                CHECK(g.server.audio.advance(.1, {}, {}));
                CHECK(g.server.audio.peak > 0);
                std::cout << "Flashlight cycle=" << cycle << " active=" << p.m_InventoryMgr.flashlight->active
                          << " frames=" << v->clip->frames() << " peak=" << g.server.audio.peak << '\n';
            }
            CHECK(g.client_shell.item_actions == 2 && g.client_shell.item_sounds == 2);
            auto child = p.m_InventoryMgr.flashlight->light;
            CHECK(g.leave_player() && !g.server.IsObjectAlive(child) && g.unload_world());
            CHECK(!g.server.string_count() && !g.server.message_count() && g.server.audio.voices().empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"rules", rules}, {"pickup", pickup},   {"commands", commands},
        {"ray", ray},     {"restore", restore}, {"lifetime", lifetime},
        {"input", input}, {"session", session}, {"private", private_game}};
    auto i = tests.find(argv[1]);
    return i != tests.end() && i->second(argv[2]) ? 0 : 1;
}
