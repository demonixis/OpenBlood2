#include "game/original/BuiltinModel.h"
#include "game/original/CameraSpot.h"
#include "game/original/Key.h"
#include "game/server_adapter.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

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
    struct Probe;
    struct Context
    {
        int constructed = 0, destroyed = 0, engines = 0, aggregates = 0, objects = 0, aggregate_messages = 0;
        DDWORD aggregate_word = 0;
        std::function<void(Probe&, DDWORD, void*, float)> engine;
        std::function<void(Probe&, DDWORD, HMESSAGEREAD)> message;
    } context;
    struct ProbeAggregate : Aggregate
    {
        DDWORD EngineMessageFn(LPBASECLASS, DDWORD, void*, float) override
        {
            ++context.aggregates;
            return 1;
        }
        DDWORD ObjectMessageFn(LPBASECLASS, HOBJECT, DDWORD, HMESSAGEREAD read) override
        {
            ++context.aggregate_messages;
            if (read)
                context.aggregate_word = g_pServerDE->ReadFromMessageDWord(read);
            return 1;
        }
    };
    struct Probe : CameraSpot
    {
        ProbeAggregate aggregate;
        int value = 0;
        Probe()
        {
            ++context.constructed;
            AddAggregate(&aggregate);
        }
        ~Probe() override
        {
            ++context.destroyed;
        }
        DDWORD EngineMessageFn(DDWORD id, void* data, float reason) override
        {
            ++context.engines;
            const auto result = CameraSpot::EngineMessageFn(id, data, reason);
            if (context.engine)
                context.engine(*this, id, data, reason);
            return result;
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD id, HMESSAGEREAD read) override
        {
            ++context.objects;
            if (context.message)
                context.message(*this, id, read);
            return CameraSpot::ObjectMessageFn(sender, id, read);
        }
    };
    std::unique_ptr<BaseClass> probe_factory()
    {
        return std::make_unique<Probe>();
    }
    // Lifetime exceeds each Objects registry. Parent is assigned after original registration.
    ClassInfo probe_type{"ContractProbe", nullptr, probe_factory, {{"Marker", 0.0f}}};
    struct Rejected : Probe
    {
        DDWORD EngineMessageFn(DDWORD id, void* data, float reason) override
        {
            Probe::EngineMessageFn(id, data, reason);
            return id == MID_PRECREATE ? 0 : 1;
        }
    };
    std::unique_ptr<BaseClass> rejected_factory()
    {
        return std::make_unique<Rejected>();
    }
    ClassInfo rejected_type{"RejectedProbe", &probe_type, rejected_factory, {}};
    bool setup(GameSession& session)
    {
        context = {};
        if (!session.initialize())
            return false;
        probe_type.parent = session.objects.find_class("CameraSpot");
        return session.objects.register_class(probe_type);
    }
    HOBJECT probe(GameSession& session, const char* name = "probe")
    {
        return session.objects.create(&probe_type, {}, Creation::World, {{"Name", std::string(name)}});
    }
    bool original_objects()
    {
        GameSession s;
        CHECK(setup(s));
        auto camera = s.create_example("CameraSpot");
        auto key = s.create_example("Key");
        CHECK(camera && key);
        auto* c = s.objects.get(camera);
        auto* k = s.objects.get(key);
        CHECK(dynamic_cast<CameraSpot*>(c->game.get()));
        auto* original_key = dynamic_cast<Key*>(k->game.get());
        CHECK(original_key);
        CHECK(c->state.m_NextUpdate == 0.001f && !c->scheduled); // PostPropRead then InitialUpdate(0).
        CHECK(c->state.m_Flags & FLAG_DONTFOLLOWSTANDING);       // Real B2BaseClass parent callback.
        CHECK(original_key->m_fTimeStamp == 1.5f && s.server.string_count() == 2);
        CHECK(std::string(s.server.GetStringData(original_key->m_hstrMessageTarget)) == "M1_CameraSpot");
        CHECK(s.objects.advance(10) && c->updates == 0 && k->updates == 0);
        CHECK(s.objects.remove(key) && s.server.string_count() == 2);
        CHECK(s.objects.finish_frame() && s.server.string_count() == 0);
        // Original world-only difficulty filtering removes during initial update.
        auto rejected =
            s.objects.create(s.objects.find_class("CameraSpot"), {}, Creation::World, {{"DifficultyMed", false}});
        CHECK(rejected && s.objects.get(rejected)->life == Life::Pending);
        CHECK(s.objects.finish_frame() && !s.objects.get(rejected));
        // Original Key reads its fields only for WORLDFILE, not STRINGPROP. Preserve this distinction.
        auto string_key = s.objects.create_props(s.objects.find_class("Key"), {}, "TimeStamp 9; Name \"string key\"");
        CHECK(string_key && dynamic_cast<Key*>(s.objects.get(string_key)->game.get())->m_fTimeStamp == 0);
        CHECK(std::string(s.objects.get(string_key)->state.m_Name) == "string key");
        return true;
    }
    bool builtin_model()
    {
        GameSession s;
        CHECK(setup(s));
        const auto* type = s.objects.find_class("Model");
        CHECK(type && type->parent == s.objects.find_class("BaseClass"));
        ObjectCreateStruct input;
        input.m_ObjectType = OT_SPRITE; // The source callback unconditionally replaces this.
        input.m_Flags = FLAG_SHADOW;
        input.m_Pos = {7, 8, 9};
        SAFE_STRCPY(input.m_Name, "runtime");
        SAFE_STRCPY(input.m_Filename, "missing.abc");
        SAFE_STRCPY(input.m_SkinName, "missing.dtx");
        const auto runtime = s.objects.create(type, input);
        auto* r = s.objects.get(runtime);
        CHECK(r && dynamic_cast<BuiltinModel*>(r->game.get()));
        CHECK(r->state.m_ObjectType == OT_MODEL && r->state.m_Flags == FLAG_SHADOW);
        CHECK(r->state.m_Pos.x == 7 && std::string(r->state.m_Name) == "runtime");
        CHECK(std::string(r->state.m_Filename) == "missing.abc" && !r->model_error.empty() && !r->model);
        s.server.SetModelAnimation(runtime, 0);
        CHECK(r->animation.animation == no_animation && s.objects.error() == ObjectError::InvalidProperty);
        for (auto reason : {Creation::World, Creation::String})
        {
            auto h = s.objects.create(type, input, reason);
            auto* o = s.objects.get(h);
            CHECK(o && o->state.m_ObjectType == OT_MODEL && o->state.m_Flags == FLAG_VISIBLE);
            CHECK(o->state.m_Pos.x == 0 && std::string(o->state.m_Name) == "noname");
            CHECK(!o->state.m_Filename[0] && !o->state.m_SkinName[0] && !o->model);
            CHECK(o->model_error.empty() && !o->scheduled);
        }
        auto h = s.objects.create_props(type, input,
                                        "Name lamp; Flags 3; Visible false; Shadow false; Pos \"1 2 3\"; "
                                        "Filename absent.abc; Skin absent.dtx");
        const auto* o = s.objects.get(h);
        CHECK(o && o->state.m_Flags == 0 && o->state.m_ObjectType == OT_MODEL && o->state.m_Pos.y == 2);
        CHECK(std::string(o->state.m_Filename) == "absent.abc" && !o->model_error.empty());
        CHECK(!s.objects.create(type, {}, Creation::World, {{"DifficultyMed", false}}));
        CHECK(!s.objects.create(type, {}, Creation::World, {{"Flags", std::string("wrong type")}}));
        // The original C callback delegates to the aggregate chain exactly once.
        ProbeAggregate aggregate;
        bc_AddAggregate(r->game.get(), &aggregate);
        const auto engine_calls = context.aggregates, message_calls = context.aggregate_messages;
        CHECK(BaseClass::_EngineMsgFn(r->game.get(), MID_UPDATE, nullptr, 0) == 1);
        CHECK(BaseClass::_ObjectMsgFn(r->game.get(), nullptr, 999, nullptr) == 1);
        CHECK(context.aggregates == engine_calls + 1 && context.aggregate_messages == message_calls + 1);
        r->game->m_pFirstAggregate = nullptr; // Stack-owned probe, not an engine-owned aggregate.
        const auto id = r->id;
        CHECK(s.objects.remove(runtime) && s.objects.finish_frame() && !s.objects.get(id));
        return true;
    }
    bool nested_creation()
    {
        GameSession s;
        CHECK(setup(s));
        bool restored = false, child_ok = false;
        HOBJECT child = nullptr;
        context.engine = [&](Probe&, DDWORD id, void* data, float reason)
        {
            if (id != MID_PRECREATE)
                return;
            GenericProp marker{};
            s.server.GetPropGeneric("Marker", &marker);
            if (marker.m_Float == 7)
            {
                ObjectCreateStruct child_state;
                auto* instance = s.server.CreateObjectProps(&probe_type, &child_state, "Name \"child\"; Marker 12");
                child = instance ? instance->m_hObject : nullptr;
                s.server.GetPropGeneric("Marker", &marker);
                restored = marker.m_Float == 7;
                auto& state = *static_cast<ObjectCreateStruct*>(data);
                state.m_ObjectType = OT_MODEL;
                state.m_Flags = FLAG_VISIBLE;
                state.m_Pos = {5, 6, 7};
                state.m_Scale = {2, 3, 4};
                state.m_Rotation = {{0, 1, 0}, 0};
                legacy_copy(state.m_Filename, "Models\\original.abc");
                legacy_copy(state.m_Name, "changed in precreate");
            }
            else
                child_ok = marker.m_Float == 12 && reason == PRECREATE_STRINGPROP;
        };
        auto parent = s.objects.create(&probe_type, {}, Creation::World, {{"Marker", 7.0f}});
        CHECK(parent && child && restored && child_ok);
        auto* r = s.objects.get(parent);
        CHECK(r->state.m_ObjectType == OT_MODEL && r->state.m_Pos.x == 5 && r->state.m_Scale.y == 3);
        CHECK(r->state.m_Rotation.m_Vec.y == 1 && r->state.m_Flags == (FLAG_VISIBLE | FLAG_DONTFOLLOWSTANDING));
        CHECK(std::string(r->state.m_Name) == "changed in precreate");
        CHECK(std::string(r->state.m_Filename) == "Models\\original.abc");
        GenericProp outside{};
        CHECK(s.server.GetPropGeneric("Marker", &outside) == DE_NOTFOUND);
        context.engine = {};
        return true;
    }
    bool reasons_properties()
    {
        GameSession s;
        CHECK(setup(s));
        std::vector<float> pre, initial;
        context.engine = [&](Probe&, DDWORD id, void*, float reason)
        {
            if (id == MID_PRECREATE)
                pre.push_back(reason);
            if (id == MID_INITIALUPDATE)
                initial.push_back(reason);
        };
        for (unsigned reason = 0; reason < 4; ++reason)
            CHECK(s.objects.create(&probe_type, {}, static_cast<Creation>(reason)));
        CHECK(pre == std::vector<float>({0, 1, 2, 3}) && initial == pre);
        auto h =
            s.objects.create_props(&probe_type, {}, "name=\"Two words\"; Marker 2.5; Pos \"1 2 3\"; Visible true;");
        CHECK(h && s.objects.get(h)->state.m_Pos.z == 3 && (s.objects.get(h)->state.m_Flags & FLAG_VISIBLE));
        CHECK(std::get<float>(s.objects.get(h)->properties.at("marker")) == 2.5f);
        for (const auto* bad : {"Unknown 1", "Marker no", "Marker nan", "Name \"unfinished", "Name a; name b",
                                "Pos \"1 2\"", "Visible maybe", "Name \"a\" trailing", "Marker"})
            CHECK(!s.objects.create_props(&probe_type, {}, bad));
        CHECK(!s.objects.create(&probe_type, {}, Creation::World, {{"Marker", std::string("wrong type")}}));
        CHECK(!s.objects.create_props(&probe_type, {}, std::string(16385, 'x')));
        context.engine = {};
        return true;
    }
    bool scheduling()
    {
        GameSession s;
        CHECK(setup(s));
        context.engine = [&](Probe& p, DDWORD id, void*, float)
        {
            if (id == MID_INITIALUPDATE)
                s.server.SetNextUpdate(p.m_hObject, 0.01f);
            if (id == MID_UPDATE)
            {
                ++p.value;
                s.server.SetNextUpdate(p.m_hObject, p.value == 1 ? 0.01f : 0);
            }
        };
        auto h = probe(s);
        CHECK(h);
        CHECK(s.objects.advance(0.011) && s.objects.get(h)->updates == 1);
        CHECK(s.objects.advance(0.005) && s.objects.get(h)->updates == 1);
        CHECK(s.objects.advance(0.006) && s.objects.get(h)->updates == 2);
        CHECK(s.objects.advance(5) && s.objects.get(h)->updates == 2); // Return 1 never becomes a one-second delay.
        CHECK(!s.objects.set_next_update(h, -1) &&
              !s.objects.set_next_update(h, std::numeric_limits<float>::quiet_NaN()));
        CHECK(!s.objects.advance(-1) && !s.objects.advance(std::numeric_limits<double>::infinity()));
        CHECK(s.objects.set_next_update(h, 0.01f));
        s.server.SetObjectState(h, OBJSTATE_INACTIVE);
        CHECK(s.objects.advance(1) && s.objects.get(h)->updates == 2);
        s.server.SetObjectState(h, OBJSTATE_ACTIVE);
        CHECK(s.objects.advance(0) && s.objects.get(h)->updates == 3);
        context.engine = {};
        return true;
    }
    bool deferred_removal()
    {
        GameSession s;
        CHECK(setup(s));
        HOBJECT other = nullptr, born = nullptr;
        bool continued = false, blocked_reentry = false;
        auto first = probe(s, "first");
        other = probe(s, "other");
        CHECK(first && other);
        context.engine = [&](Probe& p, DDWORD id, void*, float)
        {
            if (id != MID_UPDATE)
                return;
            s.server.RemoveObject(other);
            s.server.RemoveObject(p.m_hObject);
            s.server.RemoveObject(p.m_hObject);
            p.value = 123;
            continued = s.server.HandleToObject(p.m_hObject) == &p && context.destroyed == 0;
            blocked_reentry = !s.objects.finish_frame() && !s.objects.advance(0) && !s.objects.clear();
            born = probe(s, "born");
            s.server.SetNextUpdate(born, 0.001f);
        };
        s.server.SetNextUpdate(first, 0.001f);
        s.server.SetNextUpdate(other, 0.001f);
        CHECK(s.objects.advance(0.002));
        CHECK(continued && blocked_reentry && context.destroyed == 2 && born);
        CHECK(!s.objects.get(first) && !s.objects.get(other) && s.objects.get(born)->updates == 0);
        context.engine = {};
        CHECK(s.objects.advance(0.002) && s.objects.get(born)->updates == 1);
        return true;
    }
    bool aggregate_dispatch()
    {
        GameSession s;
        CHECK(setup(s));
        auto h = probe(s);
        CHECK(h);
        CHECK(context.engines == 2 && context.aggregates == 2);
        CHECK(s.objects.message(h, nullptr, 42, nullptr));
        CHECK(context.objects == 1 && context.aggregate_messages == 1);
        CHECK(s.server.IsKindOf(s.server.GetObjectClass(h), s.server.GetClass("BaseClass")));
        CHECK(!s.server.IsKindOf(&probe_type, nullptr));
        CHECK(s.server.ObjectToHandle(s.server.HandleToObject(h)) == h);
        return true;
    }
    bool handles_selection()
    {
        GameSession s;
        CHECK(setup(s));
        auto h = probe(s);
        CHECK(h);
        auto old_id = s.objects.get(h)->id;
        ObjectSelection selection{old_id};
        CHECK(s.objects.remove(h));
        selection.refresh(s.objects);
        CHECK(selection.id == old_id);
        CHECK(s.objects.finish_frame());
        selection.refresh(s.objects);
        CHECK(!selection.id);
        auto fresh = probe(s);
        CHECK(fresh && fresh != h);
        CHECK(s.objects.get(fresh)->id.slot == old_id.slot && s.objects.get(fresh)->id.generation != old_id.generation);
        CHECK(!s.objects.get(h) && !s.objects.get(old_id) && !s.server.HandleToObject(h));
        CHECK(!s.objects.remove(h));
        DObject foreign{};
        CHECK(!s.objects.get(&foreign));
        CHECK(!s.objects.message(h, nullptr, 1, nullptr));
        return true;
    }
    bool links_lists()
    {
        GameSession s;
        CHECK(setup(s));
        auto owner = probe(s, "same"), target = probe(s, "same"), third = probe(s, "third");
        CHECK(owner && target && third);
        auto* list = s.server.FindNamedObjects("same");
        CHECK(list && list->m_nInList == 2);
        s.server.RemoveObjectFromList(list, owner);
        CHECK(list->m_nInList == 1);
        CHECK(s.server.AddObjectToList(list, owner) && list->m_nInList == 2);
        CHECK(s.server.CreateInterObjectLink(owner, target) == DE_OK &&
              s.server.CreateInterObjectLink(owner, target) == DE_OK);
        CHECK(s.objects.link_count(owner) == 1 && s.server.CreateInterObjectLink(owner, owner) == DE_ERROR);
        int notifications = 0;
        bool valid_during_notice = false;
        context.engine = [&](Probe& p, DDWORD id, void* data, float)
        {
            if (id == MID_LINKBROKEN)
            {
                ++notifications;
                valid_during_notice = data == target && p.m_hObject == owner && s.objects.get(target);
                s.server.RemoveObject(third); // Cleanup callbacks may extend the pending queue.
            }
        };
        CHECK(s.objects.remove(target) && s.objects.finish_frame());
        CHECK(notifications == 1 && valid_during_notice && !s.objects.get(third) && s.objects.link_count(owner) == 0);
        CHECK(list->m_nInList == 2); // Lists are caller-owned snapshots; stale tokens remain safe to resolve.
        CHECK(!s.server.HandleToObject(target));
        s.server.RelinquishList(list);
        s.server.RelinquishList(list);
        CHECK(s.server.list_count() == 0);
        auto another = probe(s);
        CHECK(s.objects.link(owner, another));
        s.server.BreakInterObjectLink(owner, another);
        CHECK(s.objects.remove(another) && s.objects.finish_frame());
        CHECK(notifications == 1);
        // The original contract also notifies when the owner is removed.
        another = probe(s);
        CHECK(s.objects.link(owner, another));
        CHECK(s.objects.remove(owner) && s.objects.finish_frame() && notifications == 2);
        context.engine = {};
        return true;
    }
    bool messages_strings()
    {
        GameSession s;
        CHECK(setup(s));
        ServerDE* api = &s.server;
        CSBase* base = api;
        auto h = probe(s);
        CHECK(h);
        HSTRING text = api->CreateString("original payload");
        CHECK(text && api->CopyString(text) == text);
        api->FreeString(text);
        CHECK(api->GetStringData(text));
        auto write = api->StartMessageToObject(nullptr, h, 17);
        CHECK(write);
        CHECK(base->WriteToMessageHString(write, text) == DE_OK);
        api->FreeString(text);
        CHECK(!api->GetStringData(text));
        bool read_ok = false;
        context.message = [&](Probe&, DDWORD, HMESSAGEREAD read)
        {
            auto copy = base->ReadFromMessageHString(read);
            read_ok = copy && std::string(api->GetStringData(copy)) == "original payload";
            api->FreeString(copy);
            base->EndHMessageWrite(read); // Borrowed reader cannot be destroyed from its callback.
        };
        CHECK(api->EndMessage(write) == DE_OK && read_ok && s.server.message_count() == 0 &&
              s.server.string_count() == 0);
        CHECK(api->EndMessage(write) == DE_INVALIDPARAMS);
        DDWORD observed = 0;
        context.message = [&](Probe&, DDWORD, HMESSAGEREAD read) { observed = base->ReadFromMessageDWord(read); };
        write = api->StartMessageToObject(nullptr, h, 18);
        CHECK(base->WriteToMessageDWord(write, 0x12345678) == DE_OK);
        CHECK(api->EndMessage(write) == DE_OK && observed == 0x12345678 && context.aggregate_word == observed);
        // Empty/truncated messages fail rather than synthesize valid payloads.
        write = api->StartMessageToObject(nullptr, h, 18);
        CHECK(api->EndMessage(write) == DE_ERROR);
        context.message = {};
        write = api->StartMessageToObject(nullptr, h, 19);
        CHECK(write && s.objects.remove(h));
        CHECK(api->EndMessage(write) == DE_ERROR && s.server.message_count() == 0);
        write = base->StartHMessageWrite();
        CHECK(base->WriteToMessageDWord(write, 1024) == DE_OK);
        CHECK(!base->ReadFromMessageHString(write));
        base->EndHMessageWrite(write);
        write = base->StartHMessageWrite();
        for (unsigned i = 0; i < 16384; ++i)
            CHECK(base->WriteToMessageDWord(write, i) == DE_OK);
        CHECK(base->WriteToMessageDWord(write, 1) == DE_ERROR);
        base->EndHMessageWrite(write);
        return true;
    }
    bool unsupported_teardown()
    {
        CHECK(g_pServerDE == nullptr);
        for (int i = 0; i < 25; ++i)
        {
            GameSession s;
            CHECK(setup(s));
            CHECK(s.create_example("Key"));
            GameSession second;
            CHECK(!second.initialize() && g_pServerDE == &s.server);
            ServerDE* api = &s.server;
            auto h = probe(s);
            DVector pos{1, 2, 3};
            CHECK(api->MoveObject(h, &pos) == DE_ERROR && s.objects.error() == ObjectError::Unsupported);
            CHECK(api->SaveObjects("unused", nullptr, 0, 0) == DE_ERROR);
            CHECK(api->RestoreObjects("unused", 0, 0) == DE_ERROR);
            CHECK(!s.objects.create(nullptr) && !s.server.GetClass("PlayerObj"));
            CHECK(s.objects.clear() && s.server.string_count() == 0 && s.objects.snapshot().empty());
            CHECK(context.constructed == context.destroyed);
        }
        CHECK(g_pServerDE == nullptr);
        return true;
    }
    bool creation_failures()
    {
        GameSession s;
        CHECK(setup(s));
        CHECK(s.objects.register_class(rejected_type));
        CHECK(!s.objects.register_class(rejected_type));
        int initial = 0;
        context.engine = [&](Probe&, DDWORD id, void*, float)
        {
            if (id == MID_INITIALUPDATE)
                ++initial;
        };
        CHECK(!s.objects.create(&rejected_type) && initial == 0 && context.destroyed == 0);
        CHECK(s.objects.finish_frame() && context.destroyed == 1);
        // Removal during precreate must not revive the record or call initial update.
        context.engine = [&](Probe& p, DDWORD id, void*, float)
        {
            if (id == MID_PRECREATE)
                s.server.RemoveObject(p.m_hObject);
            if (id == MID_INITIALUPDATE)
                ++initial;
        };
        auto h = probe(s);
        CHECK(h && initial == 0 && s.objects.get(h)->life == Life::Pending);
        CHECK(s.objects.finish_frame() && context.destroyed == 2);
        // Invalid state returned by original-style precreate is rejected before storage.
        context.engine = [&](Probe&, DDWORD id, void* data, float)
        {
            if (id == MID_PRECREATE)
                static_cast<ObjectCreateStruct*>(data)->m_Pos.x = std::numeric_limits<float>::infinity();
        };
        CHECK(!probe(s) && s.objects.finish_frame());
        // Missing BSP rejects construction before initial update, for both world
        // models and containers. Runtime callers still receive null, never a dummy.
        s.world.models.resize(1);
        s.world.models[0].name = "existing";
        for (const auto kind : {OT_WORLDMODEL, OT_CONTAINER})
        {
            HOBJECT rejected = nullptr;
            context.engine = [&](Probe& p, DDWORD id, void* data, float)
            {
                if (id == MID_PRECREATE)
                {
                    rejected = p.m_hObject;
                    auto& state = *static_cast<ObjectCreateStruct*>(data);
                    state.m_ObjectType = kind;
                    std::snprintf(state.m_Filename, sizeof(state.m_Filename), "%s", "absent");
                }
                if (id == MID_INITIALUPDATE)
                    ++initial;
            };
            CHECK(!probe(s) && s.objects.error() == ObjectError::MissingWorldModel && initial == 0);
            CHECK(s.objects.get(rejected)->life == Life::Pending);
            CHECK(s.objects.finish_frame() && !s.objects.get(rejected));
            // A bound but invalid shape, or ambiguous case-insensitive matches,
            // is not the original missing-name case and cannot become an omission.
            s.world.models[0].name = "absent";
            CHECK(!probe(s) && s.objects.error() == ObjectError::Unsupported && initial == 0);
            CHECK(s.objects.finish_frame());
            s.world.models.push_back(s.world.models[0]);
            s.world.models[1].name = "ABSENT";
            CHECK(!probe(s) && s.objects.error() == ObjectError::Unsupported && initial == 0);
            CHECK(s.objects.diagnostics().back().find("Ambiguous") != std::string::npos);
            CHECK(s.objects.finish_frame());
            s.world.models.resize(1);
            s.world.models[0].name = "existing";
        }
        // Bound recursive spawning and restore property context at every unwind.
        int nested = 0;
        context.engine = [&](Probe&, DDWORD id, void*, float)
        {
            if (id != MID_PRECREATE)
                return;
            ++nested;
            probe(s);
        };
        CHECK(probe(s) && nested == 64);
        CHECK(s.objects.error() == ObjectError::Limit);
        GenericProp prop;
        CHECK(s.server.GetPropGeneric("Name", &prop) == DE_NOTFOUND);
        context.engine = {};
        CHECK(s.objects.clear() && context.constructed == context.destroyed);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::pair<const char*, bool (*)()> cases[] = {{"builtin_model", builtin_model},
                                                        {"original_objects", original_objects},
                                                        {"nested_creation", nested_creation},
                                                        {"reasons_properties", reasons_properties},
                                                        {"scheduling", scheduling},
                                                        {"deferred_removal", deferred_removal},
                                                        {"aggregate_dispatch", aggregate_dispatch},
                                                        {"handles_selection", handles_selection},
                                                        {"links_lists", links_lists},
                                                        {"messages_strings", messages_strings},
                                                        {"unsupported_teardown", unsupported_teardown},
                                                        {"creation_failures", creation_failures}};
    if (argc != 2)
        return 2;
    for (const auto& [name, test] : cases)
        if (std::string_view(argv[1]) == name)
        {
            if (!test())
                return 1;
            std::cout << "PASS " << name << '\n';
            return 0;
        }
    return 2;
}
