#include "game/original/AI_Mgr.h"
#include "game/original/SmellHint.h"
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
    CPlayerObj* detached(GameSession& g, Creation reason = Creation::Runtime)
    {
        return dynamic_cast<CPlayerObj*>(
            g.server.HandleToObject(g.objects.create(g.objects.find_class("CPlayerObj"), {}, reason)));
    }
    PlayerState sample()
    {
        PlayerState s;
        s.smell_time_offset = -2.5f;
        s.falling = true;
        s.server_falling = true;
        s.fall_start_offset = -.75f;
        s.min_fall_y = -40;
        s.max_fall_y = 400;
        s.smell_delay = .75f;
        s.dead = true;
        s.movement_blocked = true;
        s.pitch = -.25f;
        s.yaw = 8.25f;
        s.air = 17.25f;
        s.trigger = "USE_SAVED";
        s.name = "saved player";
        s.last_crouch = true;
        s.forced_crouch = true;
        s.strength = 6;
        s.speed = 5;
        s.resistance = 4;
        s.magic = 3;
        s.move_state = 14;
        return s;
    }
    bool roundtrip(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        CHECK(p);
        std::string error;
        auto s = sample();
        auto strings = g.server.string_count();
        const auto dims = g.objects.get(p->m_hObject)->dimensions;
        p->m_damage.SetHitPoints(73);
        p->m_InventoryMgr.SetAmmoCount(1, 1234);
        p->m_bFirstUpdate = false;
        p->m_fMoveVel = 197;
        for (int i = 0; i < 3; ++i)
        {
            CHECK(p->LoadFields(s, error));
            PlayerState saved;
            CHECK(p->SaveFields(saved, error) && saved == s);
            CHECK(g.server.string_count() == strings + 1); // Previously only TRIGGER was allocated.
            CHECK(p->m_damage.m_fHitPoints == 73 && p->m_InventoryMgr.GetAmmoCount(1) == 1234);
            CHECK(p->m_fMoveVel == 197 && !p->m_bFirstUpdate && !p->attributes_received && p->respawns == 0);
            CHECK(g.objects.get(p->m_hObject)->dimensions.y == dims.y);
        }
        // The restored player dead flag participates in the original pickup
        // guard independently of the still-alive damage aggregate.
        p->m_InventoryMgr.SetAmmoCount(1, 1);
        auto offer = [&]()
        {
            auto message = g.server.StartMessageToObject(p, p->m_hObject, add_ammo_message);
            return message && g.server.WritePickupMessage(message, {PickupKind::Ammo, 1, 10}) == DE_OK &&
                   g.server.EndMessage(message) == DE_OK;
        };
        CHECK(!p->m_damage.IsDead() && offer() && p->m_InventoryMgr.GetAmmoCount(1) == 1);
        s.dead = false;
        CHECK(p->LoadFields(s, error));
        CHECK(offer() && p->m_InventoryMgr.GetAmmoCount(1) == 11);
        p->m_InventoryMgr.SetAmmoCount(1, 1234);
        // Test-only association checks the source cache invalidation without
        // claiming the unavailable restored-shell/client-entry path.
        auto client = g.server.connect_client();
        CHECK(client);
        p->SetClient(client);
        CHECK(g.server.SetClientUserData(client, p->m_hObject) == DE_OK);
        std::vector<PlayerEvent> events;
        CHECK(p->UpdateStats() && g.server.take_client_events(client, events) && events.size() == 1);
        CHECK(p->UpdateStats() && g.server.take_client_events(client, events) && events.empty());
        p->SetClient(nullptr);
        CHECK(g.server.SetClientUserData(client, nullptr) == DE_OK);
        CHECK(p->LoadFields(s, error));
        p->SetClient(client);
        CHECK(g.server.SetClientUserData(client, p->m_hObject) == DE_OK);
        CHECK(p->UpdateStats() && g.server.take_client_events(client, events) && events.size() == 1);
        const auto* stats = std::get_if<PlayerStats>(&events.front());
        CHECK(stats && stats->health == 73 && stats->bullets == 1234);
        p->SetClient(nullptr);
        CHECK(g.server.SetClientUserData(client, nullptr) == DE_OK && g.server.disconnect_client(client));
        s.name = "";
        s.trigger.reset();
        CHECK(p->LoadFields(s, error));
        PlayerState saved;
        CHECK(p->SaveFields(saved, error) && saved == s && saved.name && saved.name->empty() && !saved.trigger);
        CHECK(g.unload_world() && g.server.string_count() == 0 && saved.name && !saved.trigger);
        auto* fresh = detached(g);
        CHECK(fresh && fresh->LoadFields(saved, error));
        PlayerState again;
        CHECK(fresh->SaveFields(again, error) && again == saved);
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0 && AI_Mgr::m_dwNumCabal == 0 &&
              AI_Mgr::m_dwNumMonster == 0);
        return true;
    }
    bool clocks(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        CHECK(p);
        CHECK(g.objects.advance(10));
        p->m_fSmellTime = 7.5f;
        p->m_fSmellDelay = .75f;
        std::string error;
        PlayerState saved;
        CHECK(p->SaveFields(saved, error) && saved.smell_time_offset == -2.5f);
        CHECK(g.objects.advance(20));
        auto* next = detached(g, Creation::Restore);
        CHECK(next);
        CHECK(next->LoadFields(saved, error) && next->m_fSmellTime == 27.5f && next->m_fSmellDelay == .75f);
        CHECK(next->m_bFirstUpdate && !next->GetClient());
        saved.smell_time_offset = -100;
        CHECK(next->LoadFields(saved, error) && next->m_fSmellTime == -70);
        saved.smell_time_offset = 4;
        CHECK(next->LoadFields(saved, error) && next->m_fSmellTime == 34);
        // These detached actors verify relative clocks; player.smell tests periodic spawning.
        return true;
    }
    bool initialization(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto before = g.objects.snapshot().size();
        auto strings = g.server.string_count();
        auto* p = detached(g, Creation::Restore);
        CHECK(p && p->initialized && p->restore_initialization && !p->restore_fields_loaded);
        auto* r = g.objects.get(p->m_hObject);
        CHECK(g.objects.snapshot().size() == before + 2 && !r->scheduled && r->dimensions.y == 0);
        CHECK(p->m_InventoryMgr.GetAmmoCount(1) == 0 && p->m_damage.m_fHitPoints == 1 && p->m_damage.m_fMass == 1);
        CHECK(p->m_fMoveVel == 0 && p->m_fJumpVel == 0 && !p->attributes_received && p->m_bFirstUpdate);
        CHECK(SmellHint::m_dwNumSmells == 0 && g.server.string_count() == strings && p->m_InventoryMgr.m_hMuzzleFlash);
        CHECK(CDestructable::m_dwNumDestructables == 1 && AI_Mgr::m_dwNumCabal == 1 && AI_Mgr::m_dwNumMonster == 1);
        PlayerState values;
        std::string error;
        CHECK(p->SaveFields(values, error) && !values.trigger && !values.name);
        CHECK(g.objects.advance(1) && p->updates == 0); // Engine restoration must provide scheduling.
        CHECK(g.objects.remove(p->m_hObject) && g.objects.finish_frame());
        CHECK(g.objects.snapshot().size() == before && CDestructable::m_dwNumDestructables == 0 &&
              AI_Mgr::m_dwNumCabal == 0);
        auto create = g.server.CreateObject;
        g.server.CreateObject = [](HCLASS, ObjectCreateStruct*) -> LPBASECLASS { return nullptr; };
        auto* rejected = detached(g, Creation::Restore);
        g.server.CreateObject = create;
        CHECK(rejected && g.objects.get(rejected->m_hObject)->life == Life::Pending &&
              !g.server.IsObjectAlive(rejected->m_hObject));
        CHECK(g.objects.finish_frame() && g.objects.snapshot().size() == before);
        auto link = g.server.CreateInterObjectLink;
        g.server.CreateInterObjectLink = [](HOBJECT, HOBJECT) -> DRESULT { return DE_ERROR; };
        rejected = detached(g, Creation::Restore);
        g.server.CreateInterObjectLink = link;
        CHECK(rejected && g.objects.get(rejected->m_hObject)->life == Life::Pending &&
              !g.server.IsObjectAlive(rejected->m_hObject));
        CHECK(g.objects.finish_frame() && g.objects.snapshot().size() == before);
        CHECK(CDestructable::m_dwNumDestructables == 0 && AI_Mgr::m_dwNumCabal == 0 &&
              g.server.string_count() == strings);
        return true;
    }
    struct Receiver : BaseClass
    {
        CPlayerObj* player = nullptr;
        PlayerState state;
        bool rejected = false;
        unsigned calls = 0;
        DDWORD ObjectMessageFn(HOBJECT, DDWORD id, HMESSAGEREAD) override
        {
            if (id == 1000 && player)
            {
                ++calls;
                PlayerState output;
                output.name = "unchanged";
                std::string error;
                rejected = !player->SaveFields(output, error) && output.name == "unchanged" &&
                           !player->LoadFields(state, error);
            }
            return 1;
        }
    };
    std::unique_ptr<BaseClass> make_receiver()
    {
        return std::make_unique<Receiver>();
    }
    HSTRING (*real_string)(const char*) = nullptr;
    unsigned allocations = 0;
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        CHECK(p);
        std::string error;
        auto s = sample();
        CHECK(p->LoadFields(s, error));
        const auto count = g.server.string_count();
        std::vector<std::function<void(PlayerState&)>> corrupt{
            [](auto& s) { s.fall_start_offset = std::numeric_limits<float>::infinity(); },
            [](auto& s) { s.min_fall_y = 401; },
            [](auto& s) { s.max_fall_y = std::numeric_limits<float>::quiet_NaN(); },
            [](auto& s) { s.air = -1; },
            [](auto& s) { s.air = 101; },
            [](auto& s) { s.smell_time_offset = std::numeric_limits<float>::infinity(); },
            [](auto& s) { s.smell_delay = -1; },
            [](auto& s) { s.yaw = std::numeric_limits<float>::quiet_NaN(); },
            [](auto& s) { s.strength = 0; },
            [](auto& s) { s.magic = 7; },
            [](auto& s) { s.move_state = 15; },
            [](auto& s) { s.character = 4; },
            [](auto& s) { s.skin = 1; },
            [](auto& s) { s.trigger = std::string(101, 'x'); },
            [](auto& s) { s.name = std::string("x\0y", 3); }};
        for (auto& change : corrupt)
        {
            auto bad = s;
            change(bad);
            CHECK(!p->LoadFields(bad, error) && !error.empty());
            PlayerState after;
            CHECK(p->SaveFields(after, error) && after == s && g.server.string_count() == count);
        }
        real_string = g.server.CreateString;
        allocations = 0;
        g.server.CreateString = [](const char* v) -> HSTRING { return ++allocations == 2 ? nullptr : real_string(v); };
        auto changed = s;
        changed.name = "new name";
        bool loaded = p->LoadFields(changed, error);
        g.server.CreateString = real_string;
        CHECK(!loaded && allocations == 2 && g.server.string_count() == count);
        PlayerState after;
        CHECK(p->SaveFields(after, error) && after == s);
        // The source saves no power state: invisibility is dropped, other powers stay unported.
        p->m_bPowerupActivated[PU_INVISIBLE] = true;
        CHECK(p->SaveFields(after, error) && after == s);
        p->m_bPowerupActivated[PU_INVISIBLE] = false;
        p->m_bPowerupActivated[PU_TRIPLEDAMAGE] = true;
        PlayerState untouched;
        untouched.name = "unchanged";
        CHECK(!p->SaveFields(untouched, error) && untouched.name == "unchanged" && !p->LoadFields(s, error));
        p->m_bPowerupActivated[PU_TRIPLEDAMAGE] = false;
        p->m_fYaw = std::numeric_limits<float>::infinity();
        CHECK(!p->SaveFields(untouched, error) && untouched.name == "unchanged");
        p->m_fYaw = s.yaw;
        static ClassInfo type{"PlayerStateReceiver", nullptr, make_receiver, {}};
        type.parent = g.objects.find_class("BaseClass");
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct spec;
        SAFE_STRCPY(spec.m_Name, "saved_player_receiver");
        auto* receiver = dynamic_cast<Receiver*>(g.server.HandleToObject(g.objects.create(&type, spec)));
        CHECK(receiver);
        receiver->player = p;
        receiver->state = s;
        p->m_damage.damage_target = "saved_player_receiver";
        p->m_damage.damage_message = "HIT";
        auto message = g.server.StartMessageToObject(p, p->m_hObject, damage_message);
        CHECK(message && g.server.WriteDamage(message, {{0, 0, 1}, {}, 1, DamageType::Normal, p->m_hObject}) == DE_OK &&
              g.server.EndMessage(message) == DE_OK);
        CHECK(receiver->calls == 1 && receiver->rejected);
        p->move_state = static_cast<CPlayerObj::MoveState>(256);
        CHECK(!p->SaveFields(untouched, error));
        p->move_state = CPlayerObj::MoveState::FireCrawl;
        CHECK(g.enter_player(error));
        auto* live = g.server_shell.Player(g.local_client);
        CHECK(live);
        CHECK(!live->LoadFields(s, error) && live->m_fAirLevel == 100);
        CHECK(g.objects.remove(p->m_hObject));
        CHECK(!p->SaveFields(untouched, error) && !p->LoadFields(s, error));
        CHECK(g.objects.finish_frame() && g.unload_world() && g.server.string_count() == 0);
        return true;
    }
    bool capture(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(!g.server_shell.CapturePlayerFields(nullptr));
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        p->m_fAirLevel = 35;
        p->m_fSmellTime = -3;
        CHECK(g.server.SendWorldExit({"next", WorldExitType::World}, "arrival") == DE_OK &&
              g.pump_player_messages(error));
        auto& c = g.server_shell.player_capture;
        CHECK(c.valid && c.captures == 1 && c.attempts == 2 && c.state.air == 35 && c.state.smell_time_offset == -3);
        CHECK(c.state.name && c.state.name->empty() && c.state.trigger == "TRIGGER" &&
              g.client_shell.world_exit.queued == 1);
        p->m_fAirLevel = 25;
        CHECK(c.state.air == 35);
        p->m_bPowerupActivated[PU_SILENT] = true;
        CHECK(!g.server_shell.CapturePlayerFields(g.local_client) && !c.valid && c.captures == 1 && !c.state.name);
        CHECK(g.unload_world() && c.attempts == 0 && !c.valid);
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
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* source = g.server_shell.Player(g.local_client);
            CHECK(source);
            source->m_fAirLevel = 17.25f + cycle;
            source->m_fSmellTime = g.server.GetTime() - 2.5f;
            source->m_damage.SetHitPoints(73);
            source->m_InventoryMgr.SetAmmoCount(1, 31);
            CHECK(g.server_shell.CapturePlayerFields(g.local_client) &&
                  g.server_shell.CapturePlayerInventory(g.local_client) &&
                  g.server_shell.CapturePlayerDamage(g.local_client));
            auto saved = g.server_shell.player_capture.state;
            auto* target = detached(g, Creation::Restore);
            CHECK(target);
            CHECK(target->LoadFields(saved, error) &&
                  target->m_damage.Load(g.server_shell.damage_capture.state, error) &&
                  target->m_InventoryMgr.Load(g.server_shell.inventory_capture.state, InventoryLoadType::KeepAlive,
                                              error));
            CHECK(target->SetCharacter(0) && target->m_InventoryMgr.UpdateHandModels());
            PlayerState after;
            CHECK(target->SaveFields(after, error) && after == saved);
            CHECK(target->m_damage.m_fHitPoints == 73 && target->m_InventoryMgr.GetAmmoCount(1) == 31 &&
                  target->respawns == 0 && !target->GetClient());
            DVector source_position;
            CHECK(g.server.GetObjectPos(source->m_hObject, &source_position) == DE_OK);
            CHECK(g.server.TeleportObject(target->m_hObject, &source_position) == DE_OK);
            CHECK(g.objects.remove(source->m_hObject) && g.objects.finish_frame());
            // A diagnostic position/dimensions setup exercises restored air recovery;
            // engine body restoration/association are not part of LoadFields.
            DVector dims{12, 41, 12};
            CHECK(g.server.SetObjectDims(target->m_hObject, &dims) == DE_OK);
            CHECK(target->UpdateContainerState() && !IsLiquid(target->containers.head.code));
            // The original air branch updates its value, then UpdateStats refuses
            // publication because this diagnostic restore has no client.
            CHECK(!target->UpdateAirLevel(.1f) && std::abs(target->m_fAirLevel - (saved.air + 4)) < .001f);
            CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.client_count() == 0 &&
                  g.objects.attachments().empty());
            CHECK(CDestructable::m_dwNumDestructables == 0 && SmellHint::m_dwNumSmells == 0 &&
                  AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0);
            CHECK(saved.air == 17.25f + cycle && saved.trigger == "TRIGGER");
            std::cout << "cycle " << cycle
                      << ": original restore initialization, selected player/aggregate fields and independent "
                         "model/air; no client/world transfer\n";
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
    else if (name == "clocks")
        ok = clocks(root);
    else if (name == "initialization")
        ok = initialization(root);
    else if (name == "failures")
        ok = failures(root);
    else if (name == "capture")
        ok = capture(root);
    else if (name == "private")
        ok = private_data(root);
    return ok ? 0 : 1;
}
