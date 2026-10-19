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
    CPlayerObj* detached(GameSession& g)
    {
        auto h = g.objects.create(g.objects.find_class("CPlayerObj"), {});
        return dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
    }
    bool seed(CInventoryMgr& inv)
    {
        CHECK(inv.ObtainWeapon(WEAP_MELEE, 0) == InventoryResult::Ok);
        CHECK(inv.ObtainWeapon(WEAP_BERETTA, 7) == InventoryResult::Ok);
        CHECK(inv.ObtainWeapon(WEAP_BERETTA, 7) == InventoryResult::Ok);
        CHECK(inv.ChangeWeaponSlot(7) == InventoryResult::Ok);
        inv.m_nAttribStrength = 2;
        inv.m_nAttribMagic = 3;
        inv.damage_multiplier = 1.75f;
        for (unsigned i = 0; i < 13; ++i)
            inv.m_fAmmo[i] = 700.5f + i;
        inv.m_bInfiniteAmmo = true;
        CHECK(inv.AddKey({"Laboratory", "Lab access", "icon.pcx", "selected.pcx", 3}) == InventoryResult::Ok);
        CHECK(inv.AddItem(1, 1) == InventoryResult::Ok);
        inv.medkit->count = 37;
        inv.CurrentWeapon()->shots = 42;
        inv.CurrentWeapon()->m_fLastShotTime = 99;
        return true;
    }
    bool matches(CInventoryMgr& inv, const InventoryState& s, bool keys)
    {
        CHECK(inv.m_fAmmo == s.ammo && inv.m_nCurWeaponSlot == s.current_slot);
        CHECK(inv.m_nAttribStrength == s.strength && inv.m_nAttribMagic == s.magic);
        CHECK(inv.m_bInfiniteAmmo == s.infinite_ammo && inv.damage_multiplier == s.damage_multiplier);
        CHECK(inv.keys.size() == (keys ? s.keys.size() : 0));
        CHECK(bool(inv.medkit) == bool(s.medkit_charge));
        if (inv.medkit)
            CHECK(inv.medkit->count == *s.medkit_charge);
        for (unsigned i = 0; i < 10; ++i)
            for (bool left : {false, true})
            {
                auto* w = inv.GetWeapon(i, left);
                CHECK((w ? w->data.m_nType : 0) == (left ? s.left[i] : s.right[i]));
                if (w)
                    CHECK(w->m_pInventoryMgr == &inv && w->m_hOwner == inv.m_hOwner && w->m_bLeftHand == left);
            }
        return true;
    }
    bool roundtrip(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        CHECK(p);
        auto& inv = p->m_InventoryMgr;
        CHECK(seed(inv));
        InventoryState state;
        std::string error;
        CHECK(inv.Save(state, error));
        const auto old_flash = inv.m_hMuzzleFlash;
        CHECK(inv.UpdateHandModels());
        const auto old_hand = inv.CurrentWeapon()->m_hHandModel;
        CHECK(old_hand);
        CHECK(inv.Load(state, InventoryLoadType::Restore, error) && matches(inv, state, true));
        CHECK(inv.QueryKey("LABORATORY") == InventoryResult::Ok && inv.keys[0].uses == 2 && state.keys[0].uses == 3);
        CHECK(inv.CurrentWeapon()->shots == 0 && inv.CurrentWeapon()->m_fLastShotTime == 0);
        CHECK(inv.CurrentWeapon()->m_hHandModel == nullptr && inv.m_hMuzzleFlash != old_flash);
        CHECK(!g.server.IsObjectAlive(old_flash) && !g.server.IsObjectAlive(old_hand));
        CHECK(g.objects.finish_frame() && g.server.IsObjectAlive(inv.m_hMuzzleFlash));
        CHECK(inv.Load(state, InventoryLoadType::KeepAlive, error) && matches(inv, state, false));
        CHECK(state.keys.size() == 1 && state.keys[0].uses == 3);
        CHECK(inv.UpdateHandModels() && inv.CurrentWeapon()->m_hHandModel && inv.CurrentWeapon(true)->m_hHandModel);
        CHECK(g.unload_world() && g.objects.snapshot().empty() && g.objects.attachments().empty());
        CHECK(g.server.string_count() == 0 && CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool malformed(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        CHECK(p);
        auto& inv = p->m_InventoryMgr;
        CHECK(seed(inv));
        InventoryState state;
        std::string error;
        CHECK(inv.Save(state, error));
        const auto ids = g.objects.snapshot();
        const auto revision = inv.revision;
        const auto* weapon = inv.CurrentWeapon();
        std::vector<std::function<void(InventoryState&)>> corrupt{
            [](auto& s) { s.strength = 0; },
            [](auto& s) { s.magic = 7; },
            [](auto& s) { s.current_slot = 10; },
            [](auto& s) { s.right[2] = 11; }, // Minigun: not ported.
            [](auto& s) { s.left[2] = WEAP_BERETTA; },
            [](auto& s) { s.left[0] = WEAP_MELEE; },
            [](auto& s) { s.left[7] = WEAP_MELEE; },
            [](auto& s) { s.ammo[0] = -1; },
            [](auto& s) { s.ammo[12] = std::numeric_limits<float>::infinity(); },
            [](auto& s) { s.damage_multiplier = std::numeric_limits<float>::quiet_NaN(); },
            [](auto& s) { s.damage_multiplier = 1001; },
            [](auto& s) { s.keys.resize(257); },
            [](auto& s) { s.keys[0].uses = 0; },
            [](auto& s) { s.keys[0].name.clear(); },
            [](auto& s) { s.keys[0].display_name = std::string(101, 'x'); },
            [](auto& s) { s.keys[0].icon = std::string("x\0y", 3); },
            [](auto& s)
            {
                auto k = s.keys[0];
                k.name = "LABORATORY";
                s.keys.push_back(k);
            }};
        for (auto& change : corrupt)
        {
            auto bad = state;
            change(bad);
            CHECK(!inv.Load(bad, InventoryLoadType::KeepAlive, error) && !error.empty());
            CHECK(g.objects.snapshot() == ids && inv.revision == revision && inv.CurrentWeapon() == weapon);
            CHECK(matches(inv, state, true));
        }
        CHECK(!inv.Load(state, static_cast<InventoryLoadType>(99), error));
        InventoryState output;
        output.ammo[1] = 123;
        inv.CurrentWeapon()->firing = true;
        CHECK(!inv.Save(output, error) && output.ammo[1] == 123);
        CHECK(!inv.Load(state, InventoryLoadType::KeepAlive, error));
        inv.CurrentWeapon()->firing = false;
        inv.m_fAmmo[1] = -1;
        CHECK(!inv.Save(output, error) && output.ammo[1] == 123);
        inv.m_fAmmo = state.ammo;
        CHECK(g.objects.remove(p->m_hObject));
        CHECK(!inv.Save(output, error) && !inv.Load(state, InventoryLoadType::KeepAlive, error));
        CHECK(g.objects.finish_frame() && g.unload_world());
        return true;
    }
    bool rollback(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        CHECK(p);
        auto& inv = p->m_InventoryMgr;
        CHECK(seed(inv));
        InventoryState state;
        std::string error;
        CHECK(inv.Save(state, error));
        const auto ids = g.objects.snapshot();
        const auto revision = inv.revision;
        const auto flash = inv.m_hMuzzleFlash;
        const auto* weapon = inv.CurrentWeapon();
        // Test-only fault injection of the already typed service, restored before assertions.
        const auto create = g.server.CreateObject;
        g.server.CreateObject = [](HCLASS, ObjectCreateStruct*) -> LPBASECLASS { return nullptr; };
        const bool created = inv.Load(state, InventoryLoadType::KeepAlive, error);
        g.server.CreateObject = create;
        CHECK(!created && !error.empty() && g.objects.snapshot() == ids);
        const auto link = g.server.CreateInterObjectLink;
        g.server.CreateInterObjectLink = [](HOBJECT, HOBJECT) -> DRESULT { return DE_ERROR; };
        const bool linked = inv.Load(state, InventoryLoadType::KeepAlive, error);
        g.server.CreateInterObjectLink = link;
        CHECK(!linked && !error.empty() && g.objects.finish_frame());
        CHECK(g.objects.snapshot() == ids && inv.revision == revision && inv.m_hMuzzleFlash == flash &&
              inv.CurrentWeapon() == weapon);
        CHECK(matches(inv, state, true));
        CHECK(inv.Load(state, InventoryLoadType::KeepAlive, error));
        CHECK(g.objects.finish_frame() && g.server.IsObjectAlive(inv.m_hMuzzleFlash));
        return true;
    }
    bool capture(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(!g.server_shell.CapturePlayerInventory(nullptr));
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        auto& inv = p->m_InventoryMgr;
        CHECK(inv.AddKey({"key", "Key", "icon", "selected", 2}) == InventoryResult::Ok);
        CHECK(g.server.SendWorldExit({"next", WorldExitType::World}, "arrival") == DE_OK &&
              g.pump_player_messages(error));
        auto& capture = g.server_shell.inventory_capture;
        CHECK(capture.valid && capture.captures == 1 && capture.attempts == 2 && capture.state.keys.size() == 1);
        CHECK(g.client_shell.world_exit.queued == 1 && inv.keys.size() == 1);
        auto saved = capture.state;
        inv.SetAmmoCount(1, 17);
        CHECK(capture.state.ammo[1] == 50 && !inv.Load(saved, InventoryLoadType::KeepAlive, error));
        CHECK(inv.GetAmmoCount(1) == 17 && g.client_shell.m_bInWorld);
        g.server.SetObjectState(p->m_hObject, OBJSTATE_INACTIVE);
        CHECK(!g.server_shell.CapturePlayerInventory(g.local_client) && !capture.valid && !capture.error.empty());
        CHECK(capture.state.keys.empty() && capture.captures == 1);
        CHECK(g.unload_world() && capture.attempts == 0 && !capture.valid);
        CHECK(saved.keys[0].name == "key" && saved.ammo[1] == 50);
        auto* restored = detached(g);
        CHECK(restored);
        CHECK(restored->m_InventoryMgr.Load(saved, InventoryLoadType::KeepAlive, error));
        CHECK(matches(restored->m_InventoryMgr, saved, false));
        CHECK(g.unload_world() && g.objects.snapshot().empty());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        InventoryState saved;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* source = g.server_shell.Player(g.local_client);
            CHECK(source);
            auto& inv = source->m_InventoryMgr;
            CHECK(inv.ObtainWeapon(WEAP_BERETTA) == InventoryResult::Ok);
            inv.SetAmmoCount(1, 123.5f + cycle);
            CHECK(inv.AddKey({"transfer-probe", "Diagnostic key", "icon", "selected", 2}) == InventoryResult::Ok);
            CHECK(g.server_shell.CapturePlayerInventory(g.local_client));
            saved = g.server_shell.inventory_capture.state;
            auto* destination = detached(g);
            CHECK(destination && destination->SetCharacter(0));
            auto& restored = destination->m_InventoryMgr;
            CHECK(restored.Load(saved, InventoryLoadType::KeepAlive, error) && matches(restored, saved, false));
            CHECK(restored.UpdateHandModels());
            CHECK(g.server.IsObjectAlive(restored.CurrentWeapon()->m_hHandModel));
            CHECK(g.server.IsObjectAlive(restored.CurrentWeapon(true)->m_hHandModel));
            CHECK(inv.keys.size() == 1 && inv.GetAmmoCount(1) == 123.5f + cycle);
            std::cout << "cycle " << cycle << ": detached inventory restored; raw bullets " << restored.GetAmmoCount(1)
                      << "; keys " << restored.keys.size() << "; both retail hand models loaded\n";
            CHECK(g.objects.remove(source->m_hObject) && g.objects.finish_frame());
            CHECK(matches(restored, saved, false) && saved.keys.size() == 1);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && g.objects.attachments().empty());
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0 &&
                  CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view name = argv[1];
    const auto root = path_from_utf8(argv[2]);
    bool ok = false;
    if (name == "roundtrip")
        ok = roundtrip(root);
    else if (name == "malformed")
        ok = malformed(root);
    else if (name == "rollback")
        ok = rollback(root);
    else if (name == "capture")
        ok = capture(root);
    else if (name == "private")
        ok = private_data(root);
    return ok ? 0 : 1;
}
