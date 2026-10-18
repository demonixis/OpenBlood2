#include "brush_fixture.h"
#include "game/original/Door.h"
#include "game/original/KeyPickup.h"
#include "game/original/Trigger.h"
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
    struct Probe : BaseClass
    {
        std::vector<std::string> received;
        std::function<void()> callback;
        DDWORD ObjectMessageFn(HOBJECT, DDWORD message, HMESSAGEREAD read) override
        {
            if (message == 1000)
            {
                auto str = g_pServerDE->ReadFromMessageHString(read);
                auto raw = g_pServerDE->GetStringData(str);
                received.emplace_back(raw ? raw : "");
                g_pServerDE->FreeString(str);
                if (callback)
                    callback();
            }
            return 1;
        }
    };
    std::unique_ptr<BaseClass> make_probe()
    {
        return std::make_unique<Probe>();
    }
    const ClassInfo probe_class{"Probe", nullptr, make_probe, {{"Name", std::string("target")}}};
    HOBJECT trigger(GameSession& g, Properties props = {}, const char* type = "Trigger")
    {
        props.try_emplace("Name", std::string("sensor"));
        props.try_emplace("TargetName1", std::string("target"));
        props.try_emplace("MessageName1", std::string("FIRST"));
        props.try_emplace("ObjectActivate", true);
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
    }
    template <class T> T* object(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<T*>(r->game.get()) : nullptr;
    }
    bool send(GameSession& g, HOBJECT dest, const char* command, HOBJECT sender = nullptr)
    {
        auto text = g.server.CreateString(command);
        auto msg = g.server.StartMessageToObject(sender ? g.server.HandleToObject(sender) : nullptr, dest, 1000);
        CHECK(text && msg);
        CHECK(g.server.WriteToMessageHString(msg, text) == DE_OK);
        const auto result = g.server.EndMessage(msg);
        g.server.FreeString(text);
        CHECK(result == DE_OK);
        return true;
    }
    HOBJECT body(GameSession& g, DVector pos = {200, 100, 200}, DDWORD flags = 0)
    {
        return g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                {{"Pos", pos}, {"Flags", std::int32_t(flags)}, {"Name", std::string("body")}});
    }
    bool init(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(g, root));
        CHECK(g.objects.register_class(probe_class));
        CHECK(g.objects.create(&probe_class, {}, Creation::World));
        return true;
    }
    Probe* probe(GameSession& g)
    {
        for (auto id : g.objects.snapshot())
            if (auto* p = dynamic_cast<Probe*>(g.objects.get(id)->game.get()))
                return p;
        return nullptr;
    }
    bool schedule(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(init(g, root));
        auto sender = body(g);
        auto h = trigger(g, {{"ResetTime", .1f},
                             {"ActivationCount", std::int32_t(2)},
                             {"MessageDelay", .05f},
                             {"TargetName2", std::string("target")},
                             {"MessageName2", std::string("NOW")},
                             {"TargetName3", std::string("target")},
                             {"MessageName3", std::string("LATER")},
                             {"MessageDelay3", .15f}});
        auto* t = object<Trigger>(g, h);
        auto* p = probe(g);
        CHECK(t && p);
        CHECK(send(g, h, "TRIGGER", sender) && t->current_activation == 1 && p->received.empty());
        CHECK(send(g, h, "TRIGGER", sender) && !t->active && p->received == std::vector<std::string>{"NOW"});
        CHECK(send(g, h, "UNLOCK") && send(g, h, "RESET") && !t->active); // Source early active guard.
        CHECK(g.step(.06) && p->received.size() == 2 && p->received.back() == "FIRST");
        CHECK(g.step(.1) && p->received.size() == 3 && p->received.back() == "LATER" && !t->active);
        CHECK(g.step(.11) && t->active);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        h = trigger(g);
        t = object<Trigger>(g, h);
        CHECK(send(g, h, "TRIGGER", sender) && !t->active && !g.objects.get(h)->scheduled);
        CHECK(g.step(.2) && !t->active && send(g, h, "RESET") && !t->active); // ResetTime=0 is one-shot.
        CHECK(!trigger(g, {{"Dims", DVector{-1, 1, 1}}}));
        CHECK(!trigger(g, {{"MessageDelay", std::numeric_limits<float>::infinity()}}));
        auto negative = trigger(g, {{"ResetTime", -1.0f}});
        CHECK(negative && send(g, negative, "TRIGGER", sender) && !g.objects.get(negative)->scheduled);
        CHECK(g.step(.1) && !object<Trigger>(g, negative)->active);
        CHECK(g.objects.finish_frame());
        auto long_message = trigger(g, {{"MessageName1", std::string(200, 'x')}});
        CHECK(long_message && send(g, long_message, "TRIGGER", sender));
        CHECK(p->received.back().size() == 200);
        CHECK(!trigger(g, {{"MessageName1", std::string(201, 'x')}}));
        auto negative_count = trigger(g, {{"ActivationCount", std::int32_t(-1)}});
        CHECK(negative_count && send(g, negative_count, "TRIGGER", sender));
        CHECK(object<Trigger>(g, negative_count)->activation_count == UINT32_MAX &&
              object<Trigger>(g, negative_count)->current_activation == 1 &&
              object<Trigger>(g, negative_count)->active);
        CHECK(g.unload_world() && g.server.message_count() == 0 && g.server.string_count() == 0);
        return true;
    }
    bool toggle(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(init(g, root));
        auto sender = body(g);
        auto h = trigger(g, {{"ResetTime", .01f}, {"OffMessageName1", std::string("OFF")}}, "ToggleTrigger");
        auto* t = object<ToggleTrigger>(g, h);
        CHECK(t && !t->on);
        CHECK(send(g, h, "TRIGGER", sender) && t->on && probe(g)->received.back() == "FIRST");
        CHECK(g.step(.02) && send(g, h, "TRIGGER", sender) && !t->on && probe(g)->received.back() == "OFF");
        auto delayed = trigger(g, {{"MessageDelay", .01f}, {"StartOn", true}, {"OffMessageName1", std::string("OFF")}},
                               "ToggleTrigger");
        CHECK(send(g, delayed, "TRIGGER", sender) && g.step(.02));
        CHECK(object<ToggleTrigger>(g, delayed)->on &&
              probe(g)->received.back() == "FIRST"); // Original delayed path bypasses toggle override.
        auto big = trigger(g, {}, "BigTrigger");
        CHECK(object<BigTrigger>(g, big));
        return true;
    }
    bool filters(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(init(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto player = g.client_shell.player;
        auto sender = body(g);
        auto ai = g.objects.create(g.objects.find_class("CBaseCharacter"));
        auto relay = trigger(g);
        auto h = trigger(g, {{"Locked", true},
                             {"ObjectActivate", false},
                             {"PlayerActivate", false},
                             {"AIActivate", false},
                             {"TriggerRelayActivate", false}});
        auto* t = object<Trigger>(g, h);
        CHECK(t);
        for (auto from : {player, sender, ai, relay})
            CHECK(send(g, h, "TRIGGER", from) && t->activations == 0);
        t->relay_activate = true;
        CHECK(send(g, h, "TRIGGER", relay) && t->activations == 1 && t->locked);
        t->SetActive(true);
        t->ai_activate = true;
        CHECK(send(g, h, "TRIGGER", ai) && t->activations == 2 && t->locked);
        t->SetActive(true);
        t->object_activate = true;
        t->named_activate = true;
        t->activation_name = "BODY";
        CHECK(send(g, h, "TRIGGER", sender) && t->activations == 3);
        t->SetActive(true);
        t->activation_name = "other";
        CHECK(send(g, h, "TRIGGER", sender) && t->activations == 3);
        t->named_activate = false;
        t->touch_activate = false;
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, sender) && t->activations == 3);
        CHECK(send(g, h, "TRIGGER", sender) && t->activations == 4); // Touch flag doesn't gate messages.
        t->SetActive(true);
        CHECK(g.objects.remove(sender));
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, sender) && t->activations == 4);
        return true;
    }
    HOBJECT pickup(GameSession& g, std::string name = "RedKey", int uses = 2)
    {
        return g.objects.create(g.objects.find_class("KeyPickup"), {}, Creation::World,
                                {{"Name", std::string("pickup")},
                                 {"ObjectName", name},
                                 {"UseCount", std::int32_t(uses)},
                                 {"Pos", DVector{200, 100, 200}},
                                 {"ModelFile", std::string("models/characters/caleb.abc")},
                                 {"ModelSkin", std::string("skins/characters/caleb1.dtx")},
                                 {"PickupTriggerTarget", std::string("target")},
                                 {"PickupTriggerMessage", std::string("PICKED")}});
    }
    bool keys(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(init(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto ph = g.client_shell.player;
        auto* p = object<CPlayerObj>(g, ph);
        CHECK(p);
        auto h = trigger(g, {{"Locked", true}, {"UnlockKeyName", std::string("REDkey")}});
        auto* t = object<Trigger>(g, h);
        CHECK(send(g, h, "TRIGGER", ph) && t->locked && t->activations == 0 && t->key_queries == 1);
        auto item = pickup(g);
        CHECK(item && object<KeyPickup>(g, item)->error.empty());
        CHECK(g.step(.1));
        CHECK(g.objects.get(item)->state.m_Pos.y >= g.objects.get(item)->dimensions.y);
        auto pos = g.objects.get(item)->state.m_Pos;
        CHECK(g.server.TeleportObject(ph, &pos) == DE_OK);
        CHECK(g.server.MoveObject(ph, &pos) == DE_OK && object<KeyPickup>(g, item)->picked_up);
        CHECK(p->m_InventoryMgr.keys.size() == 1 && p->m_InventoryMgr.keys[0].uses == 2 &&
              probe(g)->received.back() == "PICKED");
        CHECK(g.objects.finish_frame() && !g.objects.get(item));
        auto duplicate = pickup(g, "redkey");
        CHECK(g.objects.engine_message(duplicate, MID_TOUCHNOTIFY, ph));
        CHECK(g.server.IsObjectAlive(duplicate) && p->m_InventoryMgr.keys[0].uses == 2);
        CHECK(send(g, h, "TRIGGER", ph) && !t->locked && t->activations == 1 && p->m_InventoryMgr.keys[0].uses == 1);
        t->SetActive(true);
        CHECK(send(g, h, "LOCK"));
        t->named_activate = true;
        t->activation_name = "wrong";
        CHECK(send(g, h, "TRIGGER", ph) && !t->locked && t->activations == 1 && p->m_InventoryMgr.keys.empty());
        t->named_activate = false;
        CHECK(send(g, h, "LOCK"));
        p->m_bDead = true;
        CHECK(send(g, h, "TRIGGER", ph) && t->locked);
        p->m_bDead = false;
        auto zero = pickup(g, "Zero", 0);
        CHECK(g.objects.engine_message(zero, MID_TOUCHNOTIFY, ph));
        CHECK(g.server.IsObjectAlive(zero) && p->m_InventoryMgr.keys.empty());
        auto missing = g.objects.create(g.objects.find_class("KeyPickup"), {}, Creation::World);
        CHECK(missing && !g.objects.get(missing)->model && !object<KeyPickup>(g, missing)->error.empty());
        CHECK(!(g.objects.get(missing)->state.m_Flags & (FLAG_TOUCH_NOTIFY | FLAG_GRAVITY)));
        CHECK(g.leave_player() && g.unload_world() && g.server.message_count() == 0 && g.server.string_count() == 0);
        return true;
    }
    HOBJECT door(GameSession& g, bool start_open = false, bool defaults = false)
    {
        auto model = test_brush::box("gate", {-10, 10, -10}, {10, 30, 10});
        model.translation = {0, 20, 0};
        g.world.models.push_back(model);
        return g.objects.create(g.objects.find_class("Door"), {}, Creation::World,
                                {{"Name", std::string("gate")},
                                 {"SelfTrigger", true},
                                 {"TouchActivate", true},
                                 {"ObjectActivate", true},
                                 {"Locked", true},
                                 {"UnlockKeyName", std::string("GateKey")},
                                 {"MoveDist", 50.0f},
                                 {"MoveDir", DVector{1, 0, 0}},
                                 {"Speed", 100.0f},
                                 {"TriggerClosed", true},
                                 {"StartOpen", start_open},
                                 {"TriggerDims", defaults ? DVector{} : DVector{15, 15, 15}}});
    }
    bool doors(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(init(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto ph = g.client_shell.player;
        auto h = door(g);
        auto* d = object<Door>(g, h);
        CHECK(d && d->trigger);
        auto child = d->trigger;
        auto* t = object<Trigger>(g, child);
        CHECK(t && t->locked && t->dims.x == 15 && t->owner_door == h);
        CHECK(send(g, h, "UNLOCK", ph) && !t->locked);
        CHECK(send(g, h, "LOCK", ph) && t->locked);
        CHECK(send(g, child, "TRIGGER", ph) && d->state == Door::Closed);
        CHECK(object<CPlayerObj>(g, ph)->m_InventoryMgr.AddKey({"GateKey", "Gate", "k", "kh", 1}) ==
              InventoryResult::Ok);
        CHECK(send(g, child, "TRIGGER", ph) && d->state == Door::Opening && !t->active && !t->locked);
        CHECK(send(g, h, "LOCK", ph) && !t->locked); // Inactive child ignores relay, source rule.
        for (unsigned i = 0; i < 35; ++i)
            CHECK(g.step(.02));
        CHECK(d->state == Door::Open && !t->active && g.objects.get(child)->state.m_Pos.x == 0);
        CHECK(send(g, h, "TRIGGERCLOSE"));
        for (unsigned i = 0; i < 35; ++i)
            CHECK(g.step(.02));
        CHECK(d->state == Door::Closed && t->active);
        CHECK(g.objects.remove(child) && g.objects.finish_frame() && !d->trigger);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        g.world.models.clear();
        h = door(g, true, true);
        d = object<Door>(g, h);
        CHECK(d && d->trigger);
        child = d->trigger;
        t = object<Trigger>(g, child);
        CHECK(t->dims.x == 11 && t->dims.y == 11 && t->active && g.objects.get(child)->state.m_Pos.x == 0 &&
              g.objects.get(h)->state.m_Pos.x == 50);
        const auto activated = t->activations;
        auto sender = body(g);
        CHECK(g.objects.remove(h) && send(g, child, "TRIGGER", sender) && t->activations == activated);
        CHECK(g.objects.finish_frame() && !g.objects.get(child));
        return true;
    }
    bool touch(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(init(g, root));
        auto h = trigger(g, {{"Dims", DVector{1, 5, 5}}, {"Pos", DVector{0, 20, 0}}, {"ResetTime", .01f}});
        auto* t = object<Trigger>(g, h);
        auto b = body(g, {-100, 20, 0}, FLAG_SOLID);
        DVector dims{1, 1, 1};
        g.server.SetObjectDims(b, &dims);
        DVector end{100, 20, 0};
        CHECK(g.server.MoveObject(b, &end) == DE_OK && t->activations == 1);
        CHECK(g.step(.02));
        end = {0, 20, 0};
        CHECK(g.server.TeleportObject(b, &end) == DE_OK);
        CHECK(g.server.MoveObject(b, &end) == DE_OK && t->activations == 2); // Stationary overlapping body.
        CHECK(g.step(.02));
        end = {-100, 20, 0};
        CHECK(g.server.TeleportObject(b, &end) == DE_OK);
        auto wall = body(g, {-20, 20, 0}, FLAG_SOLID);
        dims = {1, 20, 20};
        g.server.SetObjectDims(wall, &dims);
        end = {100, 20, 0};
        CHECK(g.server.MoveObject(b, &end) == DE_OK && t->activations == 2); // No speculative sensor behind wall.
        CHECK(g.objects.remove(wall) && g.objects.remove(h) && g.objects.remove(b) && g.objects.finish_frame());
        auto dh = door(g);
        auto* d = object<Door>(g, dh);
        CHECK(d); // sensor at crossed pusher path
        h = trigger(g, {{"Dims", DVector{1, 5, 5}}, {"Pos", DVector{30, 20, 0}}});
        t = object<Trigger>(g, h);
        b = body(g, {15, 20, 0}, FLAG_SOLID);
        dims = {1, 1, 1};
        g.server.SetObjectDims(b, &dims);
        CHECK(g.server.MoveWorldModel(dh, {50, 20, 0}) == MoverResult::Moved && t->activations == 1);
        CHECK(g.objects.clear());
        g.world.models.clear();
        dh = door(g);
        h = trigger(g, {{"Dims", DVector{1, 5, 5}}, {"Pos", DVector{30, 20, 0}}});
        t = object<Trigger>(g, h);
        b = body(g, {15, 20, 0}, FLAG_SOLID);
        g.server.SetObjectDims(b, &dims);
        wall = body(g, {40, 20, 0}, FLAG_SOLID);
        dims = {1, 10, 10};
        g.server.SetObjectDims(wall, &dims);
        CHECK(g.server.MoveWorldModel(dh, {50, 20, 0}) == MoverResult::Blocked && t->activations == 0);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(init(g, root));
        auto b = body(g);
        auto h = trigger(g, {{"TargetName1", std::string("sensor")}, {"MessageName1", std::string("TRIGGER")}});
        auto* t = object<Trigger>(g, h);
        CHECK(send(g, h, "TRIGGER", b) && t->activations == 1 && t->current_activation == 0);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        auto a = trigger(
            g,
            {{"Name", std::string("a")}, {"TargetName1", std::string("b")}, {"MessageName1", std::string("TRIGGER")}},
            "ToggleTrigger");
        auto c = trigger(
            g,
            {{"Name", std::string("b")}, {"TargetName1", std::string("a")}, {"MessageName1", std::string("TRIGGER")}},
            "ToggleTrigger");
        CHECK(send(g, a, "TRIGGER", b) && object<Trigger>(g, a)->activations == 1 &&
              object<Trigger>(g, c)->activations == 1);
        h = trigger(g, {{"TargetName2", std::string("target")}, {"MessageName2", std::string("SHOULD_NOT_SEND")}});
        auto* p = probe(g);
        p->callback = [&] { g.objects.remove(h); };
        CHECK(send(g, h, "TRIGGER", b) && p->received.size() == 1 && g.objects.get(h)->life == Life::Pending);
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        p->callback = {};
        CHECK(g.unload_world() && g.server.message_count() == 0 && g.server.string_count() == 0);
        return true;
    }
    bool protocol(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(init(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto p = g.client_shell.player;
        auto h = trigger(g, {{"Locked", true}});
        KeyMessage key;
        key.item = {"Key", "Key", "k", "kh", 1};
        key.have = true;
        auto msg = g.server.StartMessageToObject(g.server.HandleToObject(p), h, key_response_message);
        CHECK(g.server.WriteKeyMessage(msg, key) == DE_OK && g.server.EndMessage(msg) == DE_OK &&
              object<Trigger>(g, h)->locked); // Unsolicited response cannot unlock.
        msg = g.server.StartMessageToObject(g.server.HandleToObject(h), p, key_pickup_message);
        CHECK(g.server.WriteToMessageDWord(msg, 1) == DE_OK && g.server.WriteKeyMessage(msg, key) == DE_INVALIDPARAMS &&
              g.server.EndMessage(msg) != DE_OK);
        msg = g.server.StartMessageToObject(g.server.HandleToObject(h), p, key_pickup_message);
        key.item.name = std::string(256, 'x');
        CHECK(g.server.WriteKeyMessage(msg, key) == DE_INVALIDPARAMS && g.server.EndMessage(msg) != DE_OK);
        key.item.name = "Key";
        msg = g.server.StartMessageToObject(g.server.HandleToObject(h), p, key_pickup_message);
        CHECK(g.server.WriteKeyMessage(msg, key) == DE_OK && g.server.WriteToMessageDWord(msg, 1) == DE_INVALIDPARAMS &&
              g.server.EndMessage(msg) != DE_OK);
        msg = g.server.StartMessageToObject(g.server.HandleToObject(h), p, key_pickup_message);
        CHECK(g.server.WriteKeyMessage(msg, key) == DE_OK);
        CHECK(g.objects.remove(h) && g.server.EndMessage(msg) != DE_OK &&
              object<CPlayerObj>(g, p)->m_InventoryMgr.keys.empty());
        CHECK(g.server.message_count() == 0);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        CHECK(g.request_world("01_SubwayA", error));
        std::map<EntryState, unsigned> counts;
        unsigned triggers = 0, children = 0;
        for (const auto& e : g.entries)
            ++counts[e.state];
        for (auto id : g.objects.snapshot())
            if (auto* t = dynamic_cast<Trigger*>(g.objects.get(id)->game.get()))
            {
                ++triggers;
                children += t->owner_door != nullptr;
            }
        std::cout << "SubwayA: created=" << counts[EntryState::Created] << " editor=" << counts[EntryState::EditorOnly]
                  << " unsupported=" << counts[EntryState::Unsupported] << " failed=" << counts[EntryState::Failed]
                  << " triggers=" << triggers << " children=" << children << '\n';
        CHECK(triggers > children && children > 0 && counts[EntryState::Failed] == 0);
        CHECK(g.enter_player(error));
        for (unsigned i = 0; i < 10; ++i)
            CHECK(g.step(.02));
        CHECK(g.leave_player() && g.unload_world() && g.objects.snapshot().empty() && g.server.string_count() == 0 &&
              g.server.message_count() == 0);
        CHECK(g.request_world("01_SubwayA", error) && g.unload_world());
        CHECK(g.request_world("05_CDM", error) && g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        unsigned picked = 0;
        for (auto id : g.objects.snapshot())
        {
            auto* r = g.objects.get(id);
            if (auto* key = dynamic_cast<KeyPickup*>(r->game.get()))
            {
                CHECK(r->model && r->model_error.empty() && r->dimensions.x >= 10);
                std::cout << "CDM pickup " << r->state.m_Name << " / " << key->item.name << " / " << key->error << '\n';
                CHECK(g.objects.engine_message(r->handle, MID_TOUCHNOTIFY, player->m_hObject));
                CHECK(key->picked_up);
                ++picked;
            }
        }
        CHECK(picked == 3 && player->m_InventoryMgr.keys.size() == 3);
        CHECK(g.objects.finish_frame() && g.unload_world() && g.server.message_count() == 0 &&
              g.server.string_count() == 0);
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        unsigned worlds = 0, unreadable = 0;
        std::map<std::string, unsigned> found, created, filtered;
        std::map<std::string, unsigned> failures;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World world;
            if (!g.resources.read(path, bytes, error) || !read_world(bytes, world, error))
            {
                ++unreadable;
                continue;
            }
            ++worlds;
            CHECK(g.collision.build(world.models.front(), error));
            g.server.collision_ready = true;
            for (const auto& o : world.objects)
                if (o.class_name == "Trigger" || o.class_name == "BigTrigger" || o.class_name == "ToggleTrigger" ||
                    o.class_name == "KeyPickup")
                {
                    ++found[o.class_name];
                    Properties props;
                    for (const auto& v : o.properties)
                        props.emplace(v.name, v.value);
                    auto h = g.objects.create(g.objects.find_class(o.class_name), {}, Creation::World, props);
                    if (g.server.IsObjectAlive(h))
                    {
                        ++created[o.class_name];
                        if (o.class_name == "KeyPickup")
                        {
                            auto* r = g.objects.get(h);
                            std::cout << path << " key " << r->state.m_Name << " model=" << r->state.m_Filename
                                      << " model_error=" << r->model_error
                                      << " error=" << object<KeyPickup>(g, h)->error << '\n';
                        }
                    }
                    else if (const auto* r = g.objects.get(h); r && dynamic_cast<B2BaseClass*>(r->game.get()) &&
                                                               static_cast<B2BaseClass*>(r->game.get())->filtered)
                        ++filtered[o.class_name];
                    else
                    {
                        ++failures[g.objects.diagnostics().empty() ? "unknown" : g.objects.diagnostics().back()];
                        std::cout << path << " " << property_text(props.at("Name")) << " failed";
                        for (const auto* field : {"Dims", "ResetTime", "SoundRadius", "MessageName1"})
                            if (props.contains(field))
                                std::cout << " " << field << "=" << property_text(props.at(field));
                        std::cout << '\n';
                    }
                    CHECK(g.objects.clear());
                }
        }
        std::cout << "Worlds=" << worlds << " unreadable=" << unreadable << '\n';
        for (const auto& [type, count] : found)
            std::cout << type << ": " << created[type] << '/' << count << " created; " << filtered[type]
                      << " filtered\n";
        for (const auto& [reason, count] : failures)
            std::cout << count << " rejected: " << reason << '\n';
        CHECK(worlds == 57 && unreadable == 1 && found["Trigger"] == 603 && created["Trigger"] == 601 &&
              found["BigTrigger"] == 89 && created["BigTrigger"] == 88 && created["ToggleTrigger"] == 3 &&
              created["KeyPickup"] == 13 && filtered["Trigger"] == 2 && filtered["BigTrigger"] == 1);
        CHECK(failures.empty()); // Retired TypeAction rows are created.
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"schedule", schedule},    {"toggle", toggle},  {"filters", filters},   {"keys", keys},
        {"doors", doors},          {"touch", touch},    {"lifetime", lifetime}, {"protocol", protocol},
        {"private", private_data}, {"catalog", catalog}};
    auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
