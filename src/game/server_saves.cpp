#include "game/server_adapter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <unordered_set>

// Original SaveObjects/RestoreObjects over the in-memory logical store: engine records,
// clock, creation-order save list, two-pass restore and save purity (save_load_contracts,
// lot 3). The restore-mode world (session.cpp) and the candidate session transaction
// (saved_game.cpp) are lot 4.
namespace OpenBlood2
{
    namespace
    {
        namespace R = save_records;
        // Services a MID_SAVEOBJECT callback may use: the message primitives, strings and
        // read-only queries. Everything else, including unknown names, fails the save.
        // Strictly ascending; save_objects.purity checks the order and the catalogue.
        constexpr std::array<std::string_view, 83> pure_services{
            "AlignRotation",
            "CompareStringsUpper",
            "CopyString",
            "CreateString",
            "DemoBuild",
            "EndHMessageRead",
            "EndHMessageWrite",
            "FindAttachment",
            "FreeString",
            "GetAcceleration",
            "GetAnimIndex",
            "GetAttachedModelNodeTransform",
            "GetClass",
            "GetClientUserData",
            "GetFrameTime",
            "GetGameConVar",
            "GetGlobalForce",
            "GetModelAnimUserDims",
            "GetModelAnimation",
            "GetModelAnimationName",
            "GetModelFilenames",
            "GetModelLooping",
            "GetModelNodeHideStatus",
            "GetModelNodeTransform",
            "GetModelPlaybackState",
            "GetNextInactiveObject",
            "GetNextObject",
            "GetObjectClass",
            "GetObjectColor",
            "GetObjectDims",
            "GetObjectFlags",
            "GetObjectName",
            "GetObjectPos",
            "GetObjectRotation",
            "GetObjectState",
            "GetObjectType",
            "GetObjectUserFlags",
            "GetPropBool",
            "GetPropGeneric",
            "GetPropReal",
            "GetPropString",
            "GetPropVector",
            "GetSoundDuration",
            "GetStringData",
            "GetTime",
            "GetVarValueFloat",
            "GetVarValueString",
            "GetVelocity",
            "GetWorldTime",
            "HandleToObject",
            "InterpolateRotation",
            "IsKindOf",
            "IsObjectAlive",
            "IsSoundDone",
            "ObjectToHandle",
            "QueryModelNodeTransform",
            "ReadFromLoadSaveMessageObject",
            "ReadFromMessageByte",
            "ReadFromMessageDWord",
            "ReadFromMessageFloat",
            "ReadFromMessageHMessageRead",
            "ReadFromMessageHString",
            "ReadFromMessageObject",
            "ReadFromMessageRotation",
            "ReadFromMessageString",
            "ReadFromMessageStringBuffer",
            "ReadFromMessageVector",
            "ReadFromMessageWord",
            "ResetRead",
            "StartHMessageWrite",
            "WriteToLoadSaveMessageObject",
            "WriteToMessageByte",
            "WriteToMessageDWord",
            "WriteToMessageFloat",
            "WriteToMessageHMessageRead",
            "WriteToMessageHMessageWrite",
            "WriteToMessageHString",
            "WriteToMessageObject",
            "WriteToMessageRotation",
            "WriteToMessageString",
            "WriteToMessageVector",
            "WriteToMessageWord",
        };
        bool same_bits(float a, float b)
        {
            return std::memcmp(&a, &b, sizeof a) == 0;
        }
        bool same_bits(DVector a, DVector b)
        {
            return same_bits(a.x, b.x) && same_bits(a.y, b.y) && same_bits(a.z, b.z);
        }
        bool same_bits(DRotation a, DRotation b)
        {
            return same_bits(a.m_Vec, b.m_Vec) && same_bits(a.m_Spin, b.m_Spin);
        }
        char folded(char c)
        {
            return c == '/' ? '\\' : c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : c;
        }
        bool same_path(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() &&
                   std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return folded(x) == folded(y); });
        }
        std::uint32_t generator_state(const std::minstd_rand& random)
        {
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << random;
            return static_cast<std::uint32_t>(std::stoul(out.str()));
        }
        SaveClassCount& class_count(std::vector<SaveClassCount>& counts, const std::string& name)
        {
            const auto at = std::lower_bound(counts.begin(), counts.end(), name,
                                             [](const SaveClassCount& c, const std::string& n) { return c.name < n; });
            if (at != counts.end() && at->name == name)
                return *at;
            return *counts.insert(at, SaveClassCount{name});
        }
        void count_payload(std::vector<SaveClassCount>& counts, const SavePayloadReport& payload)
        {
            auto& c = class_count(counts, payload.class_name);
            ++c.records;
            c.references += payload.references;
            c.null_references += payload.null_references;
            c.plain_objects += payload.plain_objects;
            c.bytes += payload.bytes;
        }
        std::uint32_t first_world_model(const World* world, std::string_view name)
        {
            if (world)
                for (std::size_t i = 0; i < world->models.size(); ++i)
                    if (!CaseLess{}(world->models[i].name, name) && !CaseLess{}(name, world->models[i].name))
                        return static_cast<std::uint32_t>(i);
            return R::no_index;
        }
    } // namespace

    bool ServerAdapter::save_service_allowed(const char* name)
    {
        if (std::binary_search(pure_services.begin(), pure_services.end(), std::string_view(name)))
            return true;
        ++payload_->report->impure;
        refuse(ObjectError::Busy, std::string("MID_SAVEOBJECT purity: ") + name + " is not allowed during a save");
        return false;
    }
    bool ServerAdapter::save_store_slot(std::string_view file, SaveStoreSlot& slot)
    {
        // The original shell's paths (BloodServerShell.cpp g_szSavePath + file).
        static constexpr std::array<std::pair<std::string_view, SaveStoreSlot>, 3> files{
            {{"Save\\Current\\Current.sav", SaveStoreSlot::Current},
             {"Save\\Current\\Auto.sav", SaveStoreSlot::Auto},
             {"Save\\Current\\Keep.sav", SaveStoreSlot::Keep}}};
        for (const auto& [path, value] : files)
            if (same_path(file, path))
            {
                slot = value;
                return true;
            }
        return false;
    }
    SaveClock ServerAdapter::save_clock() const
    {
        return {objects_.time(), objects_.frame_time(), world_time_origin_, gravity, generator_state(random_)};
    }
    bool ServerAdapter::apply_save_clock(const SaveClock& clock, std::string& error)
    {
        if (!valid_save_clock(clock, error))
            return false;
        if (!objects_.restore_clock(clock.time, clock.frame_time))
        {
            error = "the simulation clock can be restored only on an empty, idle registry";
            return false;
        }
        world_time_origin_ = clock.world_origin;
        gravity = clock.global_force;
        random_.seed(clock.generator); // Exact for states 1..2147483646.
        return true;
    }

    bool ServerAdapter::capture_save_record(const ObjectRecord& r, SaveEngineRecord& out, std::string& error) const
    {
        out = {};
        out.class_name = r.type->name;
        out.type = r.state.m_ObjectType;
        out.flags = r.state.m_Flags;
        out.user_flags = r.user_flags;
        out.position = r.state.m_Pos;
        out.rotation = r.state.m_Rotation;
        out.scale = r.state.m_Scale;
        out.name = r.state.m_Name;
        out.filename = r.state.m_Filename;
        out.skin = r.state.m_SkinName;
        out.container_code = r.state.m_ContainerCode;
        if (out.type == OT_WORLDMODEL || out.type == OT_CONTAINER)
        {
            out.world_model_index = r.world_model_index;
            // Containers never rotate; their shape is bound at the authored rotation.
            out.world_model_basis = out.type == OT_WORLDMODEL ? r.world_model_basis : r.state.m_Rotation;
        }
        out.color = r.color;
        out.alpha = r.alpha;
        out.dimensions = r.dimensions;
        out.velocity = r.velocity;
        out.acceleration = r.acceleration;
        out.body = static_cast<std::uint8_t>((r.world_body ? R::world_body : 0) |
                                             (r.character_body ? R::character_body : 0) |
                                             (r.model_body ? R::model_body : 0));
        out.body_properties = r.body_properties;
        out.blocking_priority = r.blocking_priority;
        out.activity = r.activity;
        out.auto_deactivate = r.auto_deactivate;
        out.deactivation_period = r.deactivation_period;
        const double now = objects_.time();
        out.deactivation_remaining = remaining_delay(now, r.deactivate_at);
        out.scheduled = r.scheduled;
        out.update_remaining = r.scheduled ? remaining_delay(now, r.due) : 0;
        out.animation = r.animation.animation;
        out.animation_time = r.animation.time;
        out.animation_looping = r.animation.looping;
        out.animation_done = r.animation.done;
        out.animation_first = r.animation.first;
        out.hidden_nodes = r.hidden_nodes;
        out.sprite_milliseconds = r.sprite_track.milliseconds;
        out.sprite_frame = r.sprite_track.frame;
        out.sprite_has_frame = r.sprite_track.has_frame;
        out.in_sky = std::find(sky.registered.begin(), sky.registered.end(), r.id) != sky.registered.end();
        for (std::uint32_t slot = 0; slot < sky.objects.size(); ++slot)
            if (sky.objects[slot] == r.id)
                out.sky_slot = slot;
        out.local_client = std::any_of(clients_.begin(), clients_.end(),
                                       [&](const auto& client) { return client.second.player == r.handle; });
        if (!valid_save_record(out, error))
        {
            error = out.class_name + ": " + error;
            return false;
        }
        return true;
    }

    bool ServerAdapter::apply_save_record(HOBJECT h, const SaveEngineRecord& s, std::string& error)
    {
        auto* r = objects_.get(h);
        auto bad = [&](const std::string& why)
        {
            error = s.class_name + ": " + why;
            return false;
        };
        if (!r || r->life != Life::Alive)
            return bad("removed during its initial update");
        const bool bsp = s.type == OT_WORLDMODEL || s.type == OT_CONTAINER;
        if (bsp && r->world_model_index != s.world_model_index)
            return bad("bound BSP model differs from the saved first same-named model");
        if (s.type == OT_CONTAINER && !same_bits(s.position, r->state.m_Pos))
            return bad("a container cannot move from its BSP origin");
        // Check every resource-dependent field before changing anything.
        if (r->model)
        {
            const auto& model = *r->model;
            if (s.animation >= model.animations.size() || s.animation_time > model.animations[s.animation].duration)
                return bad("saved animation track does not fit the restored model");
            if (!s.hidden_nodes.empty() && s.hidden_nodes.size() != model.nodes.size())
                return bad("saved hidden-node mask does not match the restored model");
        }
        else if ((s.animation != 0 && s.animation != no_animation) || !s.hidden_nodes.empty())
            return bad("animation state saved for an object without a model");
        if (r->sprite && !r->sprite->frames.empty() && s.sprite_frame >= r->sprite->frames.size())
            return bad("saved sprite frame does not fit the restored sprite");
        if (((s.body & R::world_body) && (!r->world_model || r->container())) ||
            ((s.body & (R::character_body | R::model_body)) &&
             (s.type != OT_MODEL || r->world_model || r->container())))
            return bad("saved body kind does not fit the object");
        std::string sky_error;
        if (s.in_sky && !sky.validate(objects_, h, s.sky_slot < R::sky_slots ? s.sky_slot : 0, sky_error))
            return bad("saved sky membership: " + sky_error);
        // The engine overwrites these after INITIALUPDATE_SAVEGAME. Flags, the next-update
        // delay and the name keep whatever precreate and initial update set.
        r->user_flags = s.user_flags;
        r->dimensions = s.dimensions;
        r->color = s.color;
        r->alpha = s.alpha;
        r->velocity = s.velocity;
        r->acceleration = s.acceleration;
        r->world_body = (s.body & R::world_body) != 0;
        r->character_body = (s.body & R::character_body) != 0;
        r->model_body = (s.body & R::model_body) != 0;
        r->body_properties = s.body_properties;
        r->blocking_priority = s.blocking_priority;
        r->animation.animation = s.animation;
        r->animation.time = s.animation_time;
        r->animation.looping = s.animation_looping;
        r->animation.done = s.animation_done;
        r->animation.first = s.animation_first;
        ++r->animation.revision; // Presentation caches see a new track.
        r->hidden_nodes = s.hidden_nodes;
        r->sprite_track = {s.sprite_milliseconds, s.sprite_frame, s.sprite_has_frame};
        r->activity = s.activity;
        r->auto_deactivate = s.auto_deactivate;
        r->deactivation_period = s.deactivation_period;
        r->deactivate_at = restored_deadline(objects_.time(), s.deactivation_remaining);
        // A world model keeps its posed shell; queries offset it by the translation.
        r->state.m_Pos = s.position;
        if (s.in_sky && !sky.restore(objects_, h, s.sky_slot))
            return bad("saved sky membership: " + sky.error);
        return true;
    }

    bool ServerAdapter::save_objects(std::string_view file, std::span<const HOBJECT> list, DDWORD param, DDWORD flags,
                                     SaveObjectsReport& report)
    {
        report = {};
        auto failed = [&](std::string why)
        {
            report.error = std::move(why);
            objects_.fail(ObjectError::Rejected, "SaveObjects: " + report.error);
            last_save = report;
            return false;
        };
        SaveStoreSlot slot{};
        if (!save_store_slot(file, slot))
            return failed("not the original Save\\Current\\Current.sav, Auto.sav or Keep.sav");
        report.file = save_store_slot_name(slot);
        if (!bound_ || active_ != this || g_pServerDE != this)
            return failed("the adapter is not the bound session");
        if (objects_.busy() || payload_)
            return failed("refused inside an object callback or payload dispatch");
        if (param > 1)
            return failed("dwParam must be 0 (full save) or 1 (keep-alive)");
        if (flags & ~(SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS))
            return failed("unknown SaveObjects flags");

        // The save list: listed Alive objects whose class is not always-load, in creation
        // order (D8). Duplicates are written once; pending objects are going away.
        std::vector<std::pair<std::uint32_t, HOBJECT>> selected;
        std::unordered_set<HOBJECT> seen;
        std::map<std::string, std::string> refused;
        for (auto h : list)
        {
            ++report.listed;
            if (!seen.insert(h).second)
            {
                ++report.duplicates;
                continue;
            }
            const auto* r = objects_.get(h);
            if (!r)
                return failed("the list holds an invalid or stale handle");
            if (r->life != Life::Alive)
            {
                ++report.pending;
                continue;
            }
            const auto support = save_support(r->type);
            if (support == SaveSupport::AlwaysLoad)
            {
                ++report.always_load;
                continue;
            }
            if (support != SaveSupport::Original || r->type->payload_version == 0)
            {
                refused.emplace(r->type->name, support == SaveSupport::Original ? "Original without a payload version"
                                                                                : save_support_name(support));
                continue;
            }
            selected.emplace_back(r->creation, h);
        }
        if (!refused.empty())
        {
            std::string names;
            for (const auto& [name, support] : refused)
            {
                report.refused.push_back(name + " (" + support + ")");
                names += (names.empty() ? "" : ", ") + report.refused.back();
            }
            return failed("listed classes cannot be saved: " + names);
        }
        if (selected.size() > R::max_records)
            return failed("more than 65,536 objects");
        std::sort(selected.begin(), selected.end());
        SaveIndex index;
        for (const auto& entry : selected)
            index.add(entry.second);

        // Everything a save could change, compared once the image is complete. Live messages
        // may only shrink: the original player Save consumes its pending client blob.
        const auto records_before = objects_.record_count(), handles_before = objects_.handle_count();
        const auto links_before = objects_.links();
        const auto messages_before = messages_.size();
        const auto strings_before = strings_.size(), lists_before = lists_.size();
        const auto attachments_before = objects_.attachments().size(), voices_before = audio.voices().size();
        const auto clock_before = save_clock();
        const auto random_before = random_;

        SaveImage image;
        image.param = param;
        image.flags = flags;
        std::vector<SaveEngineRecord> records(selected.size());
        std::map<std::string, std::uint32_t> versions;
        for (std::size_t i = 0; i < selected.size(); ++i)
        {
            const auto h = selected[i].second;
            const auto* r = objects_.get(h);
            std::string why;
            if (!r || !capture_save_record(*r, records[i], why))
                return failed(r ? why : "a listed object disappeared");
            versions.emplace(r->type->name, r->type->payload_version);
            SavePayloadReport payload;
            if (!save_object_payload(h, param, index, records[i].payload, payload))
            {
                report.failed = payload;
                return failed("MID_SAVEOBJECT failed for " + payload.class_name + ": " + payload.error);
            }
            count_payload(report.classes, payload);
        }
        std::vector<SaveLink> links;
        for (const auto& [owner, target] : objects_.links())
        {
            std::uint32_t a = 0, b = 0;
            if (index.find(owner, a) && index.find(target, b))
                links.push_back({a, b});
        }
        std::vector<SaveAttachment> attachments;
        for (const auto& [id, attachment] : objects_.attachments())
        {
            std::uint32_t parent = 0, child = 0;
            if (index.find(attachment.parent, parent) && index.find(attachment.child, child))
                attachments.push_back({parent, child, attachment.node, attachment.offset, attachment.rotation});
        }
        if (flags & SAVEOBJECTS_SAVEGAMECONSOLE)
        {
            image.console_saved = true;
            for (const auto& [name, variable] : variables_)
            {
                if (image.console.size() == save_format::max_console || name.empty() ||
                    name.size() > save_format::max_console_name ||
                    variable.text.size() > save_format::max_console_value)
                    return failed("console variable " + name.substr(0, 64) + " exceeds the CONS bounds");
                image.console.push_back({name, variable.text});
            }
        }
        for (const auto& [name, version] : versions)
            image.classes.push_back({name, version});
        image.records = static_cast<std::uint32_t>(records.size());
        SaveError error;
        if (!encode_save_clock(clock_before, image.clock, error) ||
            !encode_save_records(records, image.objects, error) ||
            !encode_save_links(links, records.size(), image.links, error) ||
            !encode_save_attachments(attachments, records.size(), image.attachments, error))
            return failed(error.text);
        if (objects_.record_count() != records_before || objects_.handle_count() != handles_before ||
            objects_.links() != links_before || messages_.size() > messages_before ||
            strings_.size() != strings_before || lists_.size() != lists_before ||
            objects_.attachments().size() != attachments_before || audio.voices().size() != voices_before ||
            !(save_clock() == clock_before) || random_ != random_before)
            return failed("the save changed simulation state, identities or voices");
        report.saved = image.records;
        report.links = static_cast<std::uint32_t>(links.size());
        report.attachments = static_cast<std::uint32_t>(attachments.size());
        save_store.put(slot, std::move(image));
        ++saves;
        last_save = report;
        return true;
    }

    bool ServerAdapter::restore_image_state(SaveStoreSlot slot, bool restore_time, std::string& error)
    {
        error.clear();
        const auto* image = save_store.find(slot);
        if (!image)
        {
            error = std::string("no saved ") + save_store_slot_name(slot) + " image";
            return false;
        }
        if (objects_.busy() || payload_ || objects_.record_count())
        {
            error = "console and clock are restored only on an empty, idle registry";
            return false;
        }
        SaveClock clock;
        SaveError decode;
        if (!decode_save_clock(image->clock, clock, decode))
        {
            error = decode.text;
            return false;
        }
        // CONS, then CLCK (save_load_contracts "Load sequence" 3). The clock was checked
        // above and the registry is empty and idle, so restoring it cannot fail midway.
        if (image->console_saved)
            for (const auto& variable : image->console)
                variables_[variable.name].text = variable.value;
        return !restore_time || apply_save_clock(clock, error);
    }

    bool ServerAdapter::restore_objects(std::string_view file, DDWORD param, DDWORD flags,
                                        RestoreObjectsReport& report)
    {
        report = {};
        auto failed = [&](std::string why)
        {
            report.error = std::move(why);
            objects_.fail(ObjectError::Rejected, "RestoreObjects: " + report.error);
            last_restore = report;
            return false;
        };
        SaveStoreSlot slot{};
        if (!save_store_slot(file, slot))
            return failed("not the original Save\\Current\\Current.sav, Auto.sav or Keep.sav");
        report.file = save_store_slot_name(slot);
        if (!bound_ || active_ != this || g_pServerDE != this)
            return failed("the adapter is not the bound session");
        if (objects_.busy() || payload_)
            return failed("refused inside an object callback or payload dispatch");
        if (objects_.pending_count())
            return failed("deferred removals are pending; restore at a frame boundary");
        if (param > 1)
            return failed("dwParam must be 0 (full restore) or 1 (keep-alive)");
        if (flags & ~RESTOREOBJECTS_RESTORETIME)
            return failed("unknown RestoreObjects flags");
        const auto* image = save_store.find(slot);
        if (!image)
            return failed(std::string("no saved ") + report.file + " image");

        // Decode and check the whole image before anything changes.
        SaveClock clock;
        std::vector<SaveEngineRecord> records;
        std::vector<SaveLink> links;
        std::vector<SaveAttachment> attachments;
        SaveError error;
        if (!decode_save_clock(image->clock, clock, error) || !decode_save_records(image->objects, records, error) ||
            !decode_save_links(image->links, records.size(), links, error) ||
            !decode_save_attachments(image->attachments, records.size(), attachments, error))
            return failed(error.text);
        if (records.size() != image->records)
            return failed("record count differs from the stored image");
        std::map<std::string, std::uint32_t> schema;
        for (std::size_t i = 0; i < image->classes.size(); ++i)
            if ((i && !(image->classes[i - 1].name < image->classes[i].name)) ||
                !schema.emplace(image->classes[i].name, image->classes[i].payload_version).second)
                return failed("class schema is not strictly ascending");
        std::vector<HCLASS> types(records.size());
        std::array<bool, R::sky_slots> slots{};
        std::set<std::string> present;
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            const auto& s = records[i];
            const auto type = objects_.find_class(s.class_name);
            const auto version = schema.find(s.class_name);
            if (!type || type->name != s.class_name)
                return failed("class " + s.class_name + " is not registered");
            if (save_support(type) != SaveSupport::Original || type->payload_version == 0)
                return failed("class " + s.class_name + " is " + save_support_name(save_support(type)));
            if (version == schema.end() || version->second != type->payload_version)
                return failed("class " + s.class_name + " payload version differs from this build");
            if ((s.type == OT_WORLDMODEL || s.type == OT_CONTAINER) &&
                first_world_model(objects_.world, s.filename) != s.world_model_index)
                return failed("record " + std::to_string(i) + " (" + s.class_name +
                              "): saved BSP binding differs from this world");
            if (s.type == OT_CONTAINER && !same_bits(s.world_model_basis, s.rotation))
                return failed("record " + std::to_string(i) + ": container basis differs from its rotation");
            if (s.sky_slot < R::sky_slots && std::exchange(slots[s.sky_slot], true))
                return failed("two records hold sky slot " + std::to_string(s.sky_slot));
            types[i] = type;
            present.insert(s.class_name);
        }
        if (present.size() != schema.size())
            return failed("class schema lists classes absent from the records");

        // Console, then clock, before any object exists (D9). A populated registry means
        // the restore-mode world was created after they were restored.
        struct Previous
        {
            bool clock = false, console = false;
            double time = 0, frame_time = 0, world_origin = 0;
            DVector gravity{};
            std::minstd_rand random;
            std::map<std::string, ::ConsoleVariable, CaseLess> variables;
        } previous;
        const bool restore_time = (flags & RESTOREOBJECTS_RESTORETIME) != 0;
        if (!objects_.record_count())
        {
            previous = {true,    true,    objects_.time(), objects_.frame_time(), world_time_origin_,
                        gravity, random_, variables_};
            std::string why;
            if (!restore_image_state(slot, restore_time, why))
                return failed(why);
            report.console_restored = image->console_saved;
            report.clock_restored = restore_time;
        }
        else
        {
            if (restore_time && (objects_.time() != clock.time || objects_.frame_time() != clock.frame_time))
                return failed("RESTORETIME with existing objects requires the saved clock to be current: restore "
                              "it before creating the restore-mode world");
            // The saved console still wins over what always-load objects set at creation
            // (WorldProperties): the original restores it inside RestoreObjects, after
            // LoadWorld created them (save_load_contracts lot 4). The global force is not
            // reapplied: the original never saves it, so WorldProperties' authored value stands.
            if (image->console_saved)
            {
                previous.console = true;
                previous.variables = variables_;
                for (const auto& variable : image->console)
                    variables_[variable.name].text = variable.value;
                report.console_restored = true;
            }
        }

        const auto before = objects_.snapshot();
        auto rollback = [&](std::string why)
        {
            // Remove every record this restore created, including children their callbacks
            // created and objects an original destructor creates while being reclaimed.
            for (int round = 0; round < 8; ++round)
            {
                bool removed = false;
                for (auto id : objects_.snapshot())
                    if (std::find(before.begin(), before.end(), id) == before.end())
                        if (const auto* r = objects_.get(id); r && r->life == Life::Alive)
                        {
                            objects_.remove(r->handle);
                            removed = true;
                        }
                if (!objects_.finish_frame() || !removed)
                    break;
            }
            if (previous.clock)
            {
                if (!objects_.restore_clock(previous.time, previous.frame_time))
                    why += "; the previous clock could not be reinstated";
                world_time_origin_ = previous.world_origin;
                gravity = previous.gravity;
                random_ = previous.random;
            }
            if (previous.console)
            {
                for (auto it = variables_.begin(); it != variables_.end();)
                    if (!previous.variables.contains(it->first))
                        it = variables_.erase(it);
                    else
                    {
                        it->second.text = previous.variables.find(it->first)->second.text;
                        ++it;
                    }
            }
            report.objects.clear();
            report.local_client = nullptr;
            return failed(std::move(why));
        };

        // Pass 1: PRECREATE(3) with the saved creation fields, INITIALUPDATE(3), then the
        // remaining engine fields. A refusal fails the restore: there are no NULL holes.
        SaveIndex index;
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            const auto& s = records[i];
            ObjectCreateStruct state;
            state.m_ObjectType = s.type;
            state.m_Flags = s.flags;
            state.m_Pos = s.position;
            state.m_Rotation = s.rotation;
            state.m_Scale = s.scale;
            state.m_ContainerCode = s.container_code;
            legacy_copy(state.m_Name, s.name.c_str());
            legacy_copy(state.m_Filename, s.filename.c_str());
            legacy_copy(state.m_SkinName, s.skin.c_str());
            state.m_NextUpdate = s.scheduled ? static_cast<float>(s.update_remaining) : 0;
            const auto h =
                objects_.create_restored(types[i], state, {s.scheduled, s.update_remaining, s.world_model_basis});
            const std::string where = "record " + std::to_string(i) + " (" + s.class_name + ")";
            if (!h)
                return rollback(where + " was refused: " +
                                (objects_.diagnostics().empty() ? std::string("creation failed")
                                                                : objects_.diagnostics().back()));
            std::string why;
            if (!apply_save_record(h, s, why))
                return rollback(where + ": " + why);
            index.add(h);
            report.objects.push_back(h);
            if (s.local_client)
                report.local_client = h;
        }
        // Pass 2 starts once every object exists: MID_LOADOBJECT in the same order, with
        // exact consumption and LoadSave references resolved through the index table.
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            SavePayloadReport payload;
            if (!load_object_payload(report.objects[i], param, records[i].payload, index, payload))
            {
                report.failed = payload;
                return rollback("MID_LOADOBJECT failed for record " + std::to_string(i) + " (" +
                                records[i].class_name + "): " + payload.error);
            }
            count_payload(report.classes, payload);
        }
        // Interlinks, then attachments, after every Load. Creation is idempotent: a Load
        // that already recreated one keeps it.
        for (std::size_t i = 0; i < links.size(); ++i)
        {
            const auto owner = report.objects[links[i].owner], target = report.objects[links[i].target];
            const auto& existing = objects_.links();
            if (std::find(existing.begin(), existing.end(), std::pair{owner, target}) != existing.end())
            {
                ++report.existing_links;
                continue;
            }
            if (!objects_.link(owner, target))
                return rollback("link " + std::to_string(i) + " cannot be restored: an end is no longer alive");
            ++report.links;
        }
        for (std::size_t i = 0; i < attachments.size(); ++i)
        {
            const auto& a = attachments[i];
            const auto parent = report.objects[a.parent], child = report.objects[a.child];
            const auto* c = objects_.get(child);
            if (c && c->attachment)
            {
                HATTACHMENT existing = 0;
                if (objects_.find_attachment(parent, child, &existing) == DE_OK)
                {
                    ++report.existing_attachments;
                    continue;
                }
                return rollback("attachment " + std::to_string(i) + ": the child is already attached elsewhere");
            }
            HATTACHMENT out = 0;
            if (objects_.attach(parent, child, a.node.c_str(), &a.offset, &a.rotation, &out) != DE_OK)
                return rollback("attachment " + std::to_string(i) + " cannot be restored under the engine rules");
            ++report.attachments;
        }
        report.records = static_cast<std::uint32_t>(records.size());
        ++restores;
        last_restore = report;
        return true;
    }
} // namespace OpenBlood2
