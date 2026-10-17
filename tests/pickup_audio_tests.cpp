#include "audio_fixture.h"
#include "game/original/KeyPickup.h"
#include "game/original/PickupObject.h"
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
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(g, root, true, true, true));
        test_player::Directory dir;
        auto& sounds = dir.children["sounds"].children["powerups"].files;
        sounds["ammo1.wav"] = sounds["inventory1.wav"] = test_audio::wave(1, 16, 8000, 8000);
        sounds["bad.wav"] = {1, 2, 3};
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream out(root / "audio.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        CHECK(g.resources.mount(root / "audio.rez", error) && g.enter_player(error));
        g.server.audio.inspect_pcm = true;
        return true;
    }
    HOBJECT create(GameSession& g, const char* type = "BulletAmmoPU", Properties props = {})
    {
        props.try_emplace("Name", std::string("collection"));
        props.try_emplace("Pos", DVector{200, 10000, 200});
        if (std::string_view(type) == "KeyPickup")
        {
            props["ObjectName"] = std::string("RedKey");
            props["ModelFile"] = std::string("models/characters/caleb.abc");
        }
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
    }
    bool touch(GameSession& g, HOBJECT h)
    {
        return g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player);
    }
    bool voice(GameSession& g, const PickupReport& report)
    {
        const auto* v = g.server.audio.get(report.voice);
        CHECK(report.sound_error.empty() && report.voice && v && v->clip->frames());
        CHECK(v->request.resource == report.sound && v->request.space == SoundSpace::Spatial && !v->request.speaker &&
              !v->request.loop && !v->request.streaming && v->request.volume == 1 && v->request.inner_radius == 30 &&
              v->request.outer_radius == 300 && v->request.priority == 2 && v->request.reverb_requested &&
              v->world_owned && v->auto_remove);
        CHECK(v->position.x == report.position.x && v->position.y == report.position.y &&
              v->position.z == report.position.z);
        return true;
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        auto h = create(g);
        CHECK(h);
        p->m_InventoryMgr.SetAmmoCount(1, p->m_InventoryMgr.MaxAmmo(1));
        CHECK(touch(g, h) && g.server.pickup_reports.empty() && g.server.audio.voices().empty());
        p->m_InventoryMgr.SetAmmoCount(1, 0);
        p->m_damage.m_bDead = true;
        CHECK(touch(g, h) && g.server.pickup_reports.empty());
        p->m_damage.m_bDead = false;
        const auto pos = g.objects.get(h)->state.m_Pos;
        CHECK(touch(g, h) && g.server.pickup_reports.size() == 1 && !g.server.IsObjectAlive(h));
        const auto report = g.server.pickup_reports.back();
        CHECK(report.name == "collection" && report.class_name == "BulletAmmoPU" && report.value &&
              report.value->amount == 100 && report.key.empty() && report.position.y == pos.y && voice(g, report));
        CHECK(!touch(g, h) && g.server.pickup_reports.size() == 1 && g.server.audio.voices().size() == 1);
        CHECK(g.objects.finish_frame() && !g.objects.get(h) && g.server.audio.get(report.voice));
        CHECK(g.server.advance_audio(.01, pos, {}) && g.server.audio.peak > 0);
        CHECK(g.server.audio.get(report.voice)->cursor > 0);
        auto key = create(g, "KeyPickup");
        CHECK(key && touch(g, key) && !g.server.IsObjectAlive(key) && g.server.pickup_reports.size() == 2);
        const auto& kr = g.server.pickup_reports.back();
        CHECK(!kr.value && kr.key == "RedKey" && kr.sound == "sounds\\powerups\\inventory1.wav" && voice(g, kr));
        CHECK(g.objects.remove(p->m_hObject) && g.objects.finish_frame());
        CHECK(g.server.advance_audio(.02, pos, {}) && g.server.audio.get(report.voice));
        CHECK(g.server.advance_audio(1, pos, {}) && g.server.audio.voices().empty());
        CHECK(g.server.pickup_reports.size() == 2 && g.unload_world() && g.server.pickup_reports.empty() &&
              g.server.audio.cached_clips() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        for (const char* path : {"missing.wav", "sounds/powerups/bad.wav", ""})
        {
            auto h = create(g);
            CHECK(h);
            auto* item = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
            CHECK(item);
            item->pickup_sound = path;
            p->m_InventoryMgr.SetAmmoCount(1, 0);
            CHECK(touch(g, h) && !g.server.IsObjectAlive(h) && p->m_InventoryMgr.GetAmmoCount(1) == 100);
            const auto& r = g.server.pickup_reports.back();
            CHECK(!r.voice && r.sound_error.empty() == (r.sound.empty()) && g.server.audio.voices().empty());
        }
        SoundRequest request;
        request.resource = "sounds/powerups/ammo1.wav";
        std::string error;
        for (unsigned i = 0; i < 128; ++i)
            CHECK(g.server.PlayDetachedSound(p->m_hObject, request, {}, error));
        auto h = create(g);
        p->m_InventoryMgr.SetAmmoCount(1, 0);
        CHECK(h && touch(g, h) && !g.server.IsObjectAlive(h));
        CHECK(!g.server.pickup_reports.back().voice && !g.server.pickup_reports.back().sound_error.empty());
        CHECK(g.server.audio.voices().size() == 128 && g.server.advance_audio(1, {}, {}));
        CHECK(g.server.audio.voices().empty());
        for (unsigned i = 0; i < 35; ++i)
        {
            p->m_InventoryMgr.SetAmmoCount(1, 0);
            h = create(g);
            CHECK(h && touch(g, h) && voice(g, g.server.pickup_reports.back()) && g.objects.finish_frame());
            CHECK(g.server.advance_audio(1, {}, {}) && g.server.audio.voices().empty());
        }
        CHECK(g.server.pickup_reports.size() == 32 && g.unload_world() && g.server.pickup_reports.empty());
        return true;
    }
    struct Probe : BaseClass
    {
        std::function<void()> callback;
        DDWORD ObjectMessageFn(HOBJECT, DDWORD id, HMESSAGEREAD) override
        {
            if (id == 1000 && callback)
                callback();
            return 1;
        }
    };
    bool callbacks(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        const ClassInfo type{
            "AudioPickupProbe", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); }, {}};
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct state;
        SAFE_STRCPY(state.m_Name, "target");
        auto ph = g.objects.create(&type, state);
        CHECK(ph);
        auto* probe = dynamic_cast<Probe*>(g.objects.get(ph)->game.get());
        unsigned calls = 0;
        bool ordered = false;
        auto h = create(g, "BulletAmmoPU",
                        {{"PickupTriggerTarget", std::string("target")}, {"PickupTriggerMessage", std::string("GET")}});
        CHECK(h);
        probe->callback = [&]
        {
            ++calls;
            ordered = g.server.pickup_reports.size() == 1 && voice(g, g.server.pickup_reports.back()) &&
                      !(g.objects.get(h)->state.m_Flags & (FLAG_VISIBLE | FLAG_TOUCH_NOTIFY));
            g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player);
            g.objects.remove(h);
            g.objects.remove(g.client_shell.player);
        };
        CHECK(touch(g, h) && calls == 1 && ordered && g.server.pickup_reports.size() == 1);
        const auto id = g.server.pickup_reports.back().voice;
        CHECK(g.objects.finish_frame() && g.server.advance_audio(.05, {200, 0, 200}, {}) && g.server.audio.get(id));
        CHECK(g.unload_world() && !g.server.audio.get(id) && !g.server.string_count() && !g.server.message_count());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(Config{root, "private"}, error));
        CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
        g.server.audio.inspect_pcm = true;
        auto* p = g.server_shell.Player(g.local_client);
        std::set<std::string> sounds;
        for (const char* type :
             {"BulletAmmoPU",  "ShellAmmoPU",     "BMGAmmoPU",      "FlareAmmoPU",  "DieBugDieAmmoPU", "HowitzerAmmoPU",
              "FuelAmmoPU",    "BatteryAmmoPU",   "HealthPU",       "MegaHealthPU", "WardPU",          "NecroWardPU",
              "BerettaPU",     "SubMachineGunPU", "AssaultRiflePU", "FlareGunPU",   "MedKitPU",        "FlashlightPU",
              "ProximitiesPU", "RemotesPU",       "KeyPickup"})
        {
            for (unsigned i = 1; i <= 8; ++i)
                p->m_InventoryMgr.SetAmmoCount(i, 0);
            p->m_damage.SetHitPoints(10);
            p->m_damage.m_fArmorPoints = 0;
            const auto count = g.server.pickup_reports.size();
            auto h = create(g, type);
            CHECK(h && g.objects.get(h)->model && touch(g, h));
            CHECK(g.server.pickup_reports.size() == count + 1 && !g.server.IsObjectAlive(h));
            const auto r = g.server.pickup_reports.back();
            CHECK(voice(g, r));
            std::string normalized;
            CHECK(resource_path(r.sound, normalized));
            sounds.insert(normalized);
            CHECK(g.objects.finish_frame() && g.server.advance_audio(.02, r.position, {}) && g.server.audio.peak > 0);
            const auto* v = g.server.audio.get(r.voice);
            CHECK(v && v->cursor > 0);
            std::cout << type << " -> " << r.sound << " frames=" << v->clip->frames() << " cursor=" << v->cursor
                      << '\n';
        }
        CHECK(g.server.pickup_reports.size() == 21 && sounds.size() == 8);
        CHECK(g.unload_world() && g.server.audio.voices().empty() && g.server.pickup_reports.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"rules", rules}, {"failures", failures}, {"callbacks", callbacks}, {"private", private_data}};
    const auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
