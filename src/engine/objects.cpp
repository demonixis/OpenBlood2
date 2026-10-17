#include "engine/objects.h"
#include "engine/container.h"
#include "engine/model_store.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>

namespace OpenBlood2
{
    namespace
    {
        char lower(char c)
        {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : c;
        }
        bool equal(std::string_view a, std::string_view b)
        {
            return !CaseLess{}(a, b) && !CaseLess{}(b, a);
        }
        bool finite(DVector v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }
        bool valid(const PropertyValue& p, std::size_t max_string = MAX_CS_FILENAME_LEN)
        {
            return std::visit(
                [max_string](const auto& value)
                {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, std::string>)
                        return value.size() <= max_string && value.find('\0') == std::string::npos;
                    else if constexpr (std::is_same_v<T, float>)
                        return std::isfinite(value);
                    else if constexpr (std::is_same_v<T, DVector>)
                        return finite(value);
                    else if constexpr (std::is_same_v<T, EulerAngles>)
                        return finite(value.radians);
                    else if constexpr (std::is_same_v<T, DRotation>)
                        return finite(value.m_Vec) && std::isfinite(value.m_Spin);
                    else
                        return true;
                },
                p);
        }
        void defaults(HCLASS type, Properties& values)
        {
            if (type->parent)
                defaults(type->parent, values);
            for (const auto& p : type->properties)
                if (p.defaulted)
                    values.insert_or_assign(p.name, p.value);
        }
        const PropertySpec* spec(HCLASS type, std::string_view name)
        {
            for (; type; type = type->parent)
                for (const auto& p : type->properties)
                    if (equal(p.name, name))
                        return &p;
            return nullptr;
        }
        bool parse_value(std::string_view text, PropertyValue& value)
        {
            return std::visit(
                [&](auto& out)
                {
                    using T = std::decay_t<decltype(out)>;
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        out = text;
                        return valid(out);
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        if (equal(text, "true") || text == "1")
                        {
                            out = true;
                            return true;
                        }
                        if (equal(text, "false") || text == "0")
                        {
                            out = false;
                            return true;
                        }
                        return false;
                    }
                    else if constexpr (std::is_arithmetic_v<T>)
                    {
                        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
                        return error == std::errc{} && end == text.data() + text.size() && valid(out);
                    }
                    else
                    {
                        // Vector and quaternion values are quoted whitespace-separated numbers.
                        std::istringstream in{std::string(text)};
                        in.imbue(std::locale::classic());
                        if constexpr (std::is_same_v<T, DVector>)
                            in >> out.x >> out.y >> out.z;
                        else if constexpr (std::is_same_v<T, EulerAngles>)
                            in >> out.radians.x >> out.radians.y >> out.radians.z;
                        else
                            in >> out.m_Vec.x >> out.m_Vec.y >> out.m_Vec.z >> out.m_Spin;
                        if (!in)
                            return false;
                        in >> std::ws;
                        return in.eof() && valid(out);
                    }
                },
                value);
        }
        bool valid_state(const ObjectCreateStruct& s)
        {
            return std::memchr(s.m_Name, 0, sizeof(s.m_Name)) && std::memchr(s.m_Filename, 0, sizeof(s.m_Filename)) &&
                   std::memchr(s.m_SkinName, 0, sizeof(s.m_SkinName)) && finite(s.m_Pos) && finite(s.m_Scale) &&
                   finite(s.m_Rotation.m_Vec) && std::isfinite(s.m_Rotation.m_Spin) && std::isfinite(s.m_NextUpdate) &&
                   s.m_NextUpdate >= 0;
        }
    } // namespace

    // LithTech SetupMatrixEuler uses yaw * pitch * roll. Preserve source Euler
    // values separately for GenericProp.m_Vec (GameStartPoint consumes them).
    DRotation rotation_from_euler(DVector v)
    {
        const float sx = std::sin(v.x / 2), cx = std::cos(v.x / 2);
        const float sy = std::sin(v.y / 2), cy = std::cos(v.y / 2);
        const float sz = std::sin(v.z / 2), cz = std::cos(v.z / 2);
        return {{cy * sx * cz + sy * cx * sz, sy * cx * cz - cy * sx * sz, cy * cx * sz - sy * sx * cz},
                cy * cx * cz + sy * sx * sz};
    }

    bool CaseLess::operator()(std::string_view a, std::string_view b) const
    {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                            [](char x, char y) { return lower(x) < lower(y); });
    }
    const char* life_name(Life value)
    {
        switch (value)
        {
            case Life::Constructing:
                return "Constructing";
            case Life::Alive:
                return "Alive";
            case Life::Pending:
                return "Pending removal";
            case Life::Destroying:
                return "Destroying";
        }
        return "Unknown";
    }
    const char* creation_name(Creation value)
    {
        switch (value)
        {
            case Creation::Runtime:
                return "Runtime";
            case Creation::World:
                return "World properties";
            case Creation::String:
                return "String properties";
            case Creation::Restore:
                return "Restore callback";
        }
        return "Unknown";
    }
    std::string property_text(const PropertyValue& value)
    {
        return std::visit(
            [](const auto& v) -> std::string
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>)
                    return v;
                else if constexpr (std::is_same_v<T, bool>)
                    return v ? "true" : "false";
                else if constexpr (std::is_arithmetic_v<T>)
                    return std::to_string(v);
                else
                {
                    std::ostringstream out;
                    out.imbue(std::locale::classic());
                    if constexpr (std::is_same_v<T, DVector>)
                        out << v.x << ' ' << v.y << ' ' << v.z;
                    else if constexpr (std::is_same_v<T, EulerAngles>)
                        out << "Euler " << v.radians.x << ' ' << v.radians.y << ' ' << v.radians.z;
                    else
                        out << v.m_Vec.x << ' ' << v.m_Vec.y << ' ' << v.m_Vec.z << ' ' << v.m_Spin;
                    return out.str();
                }
            },
            value);
    }
    bool generic_property(const PropertyValue& value, GenericProp& out)
    {
        out = {};
        const std::string text = property_text(value);
        if (text.size() > MAX_GP_STRING_LEN)
            return false;
        legacy_copy(out.m_String, text.c_str());
        std::visit(
            [&](const auto& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, DVector>)
                    out.m_Vec = out.m_Color = v;
                else if constexpr (std::is_same_v<T, DRotation>)
                    out.m_Rotation = v;
                else if constexpr (std::is_same_v<T, EulerAngles>)
                {
                    out.m_Vec = v.radians;
                    out.m_Rotation = rotation_from_euler(v.radians);
                }
                else if constexpr (std::is_arithmetic_v<T>)
                {
                    out.m_Float = static_cast<float>(v);
                    out.m_Bool = v != 0;
                    const double number = static_cast<double>(v);
                    out.m_Long = static_cast<std::int32_t>(std::clamp(number, -2147483648.0, 2147483647.0));
                }
                else
                {
                    PropertyValue numeric = 0.0f;
                    if (equal(v, "true") || equal(v, "false"))
                    {
                        out.m_Bool = equal(v, "true");
                        out.m_Long = out.m_Bool;
                        out.m_Float = static_cast<float>(out.m_Bool);
                    }
                    else if (parse_value(v, numeric))
                    {
                        const float n = std::get<float>(numeric);
                        out.m_Float = n;
                        out.m_Bool = n != 0;
                        out.m_Long =
                            static_cast<std::int32_t>(std::clamp(static_cast<double>(n), -2147483648.0, 2147483647.0));
                    }
                    PropertyValue vec = DVector{};
                    if (parse_value(v, vec))
                        out.m_Vec = out.m_Color = std::get<DVector>(vec);
                }
            },
            value);
        return true;
    }
    Objects::~Objects()
    {
        clear();
    }
    bool Objects::fail(ObjectError error, std::string detail)
    {
        error_ = error;
        if (diagnostics_.size() == 32)
            diagnostics_.erase(diagnostics_.begin());
        diagnostics_.push_back(std::move(detail));
        return false;
    }
    bool Objects::register_class(const ClassInfo& type)
    {
        if (type.name.empty() || !type.construct || (type.parent && !knows(type.parent)))
            return fail(ObjectError::UnknownClass, "Invalid class registration: " + type.name);
        if (!classes_.emplace(type.name, &type).second)
            return fail(ObjectError::UnknownClass, "Duplicate class: " + type.name);
        return true;
    }
    HCLASS Objects::find_class(std::string_view name) const
    {
        const auto it = classes_.find(name);
        return it == classes_.end() ? nullptr : it->second;
    }
    bool Objects::knows(HCLASS type) const
    {
        return std::any_of(classes_.begin(), classes_.end(), [type](const auto& p) { return p.second == type; });
    }
    bool Objects::is_kind_of(HCLASS type, HCLASS parent)
    {
        if (!parent)
            return false;
        for (; type; type = type->parent)
            if (type == parent)
                return true;
        return false;
    }
    DDWORD Objects::dispatch(ObjectRecord& record, DDWORD id, void* data, float reason)
    {
        ++callback_depth_;
        auto result = BaseClass::_EngineMsgFn(record.game.get(), id, data, reason);
        --callback_depth_;
        return result;
    }
    ObjectError Objects::validate_properties(HCLASS type, const Properties& properties, std::string& error) const
    {
        error.clear();
        if (!knows(type))
        {
            error = "Create: unregistered class";
            return ObjectError::UnknownClass;
        }
        for (const auto& [name, value] : properties)
        {
            const auto* def = spec(type, name);
            if (!def ||
                (def->value.index() != value.index() &&
                 !(std::holds_alternative<DRotation>(def->value) && std::holds_alternative<EulerAngles>(value))) ||
                !valid(value, def->max_string))
            {
                error = type->name + ": invalid property " + name;
                return ObjectError::InvalidProperty;
            }
        }
        return ObjectError::None;
    }
    HOBJECT Objects::create(HCLASS type, ObjectCreateStruct state, Creation reason, const Properties& properties)
    {
        if (!knows(type))
        {
            fail(ObjectError::UnknownClass, "Create: unregistered class");
            return nullptr;
        }
        if (clearing_ || contexts_.size() >= 64 || handles_.size() >= 65536 || callback_depth_ >= 128)
        {
            fail(ObjectError::Limit, "Create: session/token/recursion limit or teardown");
            return nullptr;
        }
        auto record = std::make_unique<ObjectRecord>();
        defaults(type, record->properties);
        std::string property_error;
        const auto checked = validate_properties(type, properties, property_error);
        if (checked != ObjectError::None)
        {
            fail(checked, std::move(property_error));
            return nullptr;
        }
        for (const auto& [name, value] : properties)
            record->properties.insert_or_assign(spec(type, name)->name, value);
        if (!valid_state(state) || static_cast<unsigned>(reason) > 3)
        {
            fail(ObjectError::InvalidProperty, "Invalid creation structure/reason");
            return nullptr;
        }
        std::uint32_t index = 0;
        while (index < slots_.size() && slots_[index].object)
            ++index;
        if (index == slots_.size())
            slots_.emplace_back();
        record->id = {index, slots_[index].generation};
        handles_.push_back(std::make_unique<DObject>(DObject{record->id}));
        HOBJECT handle = handles_.back().get();
        record->handle = handle;
        record->type = type;
        record->reason = reason;
        identities_.emplace(handle, record->id);
        // A stable heap record survives vector growth during nested callbacks.
        ObjectRecord* object = record.get();
        slots_[index].object = std::move(record);
        // Publish before constructors/precreate: nested callbacks must see the
        // same occupied slots as a direct registry scan, in ascending slot order.
        records_.insert(std::lower_bound(records_.begin(), records_.end(), index,
                                         [](ObjectId id, std::uint32_t slot) { return id.slot < slot; }),
                        object->id);
        ++callback_depth_;
        object->game = type->construct();
        --callback_depth_;
        if (!object->game)
        {
            remove(handle);
            fail(ObjectError::Rejected, "Class factory returned null");
            return nullptr;
        }
        object->game->m_hObject = handle;
        contexts_.push_back(&object->properties);
        const auto accepted = dispatch(*object, MID_PRECREATE, &state, static_cast<float>(reason));
        if (!accepted || !valid_state(state))
        {
            contexts_.pop_back();
            remove(handle);
            fail(ObjectError::Rejected, "Rejected precreate: " + type->name);
            return nullptr;
        }
        object->state = state; // Consume AFTER all derived/base/aggregate precreate mutations.
        if (state.m_ObjectType == OT_CONTAINER || state.m_ObjectType == OT_WORLDMODEL)
        {
            const WorldModel* model = nullptr;
            std::string error;
            if (world)
                for (const auto& candidate : world->models)
                    if (!CaseLess{}(candidate.name, state.m_Filename) && !CaseLess{}(state.m_Filename, candidate.name))
                    {
                        if (model)
                        {
                            error = state.m_ObjectType == OT_CONTAINER ? "Ambiguous named container model"
                                                                       : "Ambiguous named world model";
                            break;
                        }
                        model = &candidate;
                    }
            auto shape = std::make_shared<ContainerShape>();
            if (!model || !error.empty() || !shape->build(*model, state.m_Rotation, error))
            {
                contexts_.pop_back();
                remove(handle);
                // The original engine rejects this object before initial update. Its
                // world-file loader continues after a missing named BSP; ambiguity,
                // invalid shapes and an unattached world remain hard failures here.
                const bool absent = world && !world->models.empty() && state.m_Filename[0] && !model && error.empty();
                fail(absent ? ObjectError::MissingWorldModel : ObjectError::Unsupported,
                     std::string(state.m_Name) + ": " +
                         (error.empty() ? (state.m_ObjectType == OT_CONTAINER ? "Missing named container model"
                                                                              : "Missing named world model")
                                        : error));
                return nullptr;
            }
            object->state.m_Pos = shape->origin;
            object->dimensions = vector_scale(vector_subtract(shape->max, shape->min), .5f);
            if (state.m_ObjectType == OT_CONTAINER)
            {
                object->container_ = std::move(shape);
                const auto at = std::lower_bound(containers_.begin(), containers_.end(), object->id.slot,
                                                 [](ObjectId id, std::uint32_t slot) { return id.slot < slot; });
                containers_.insert(at, object->id); // Visible to nested initial-update callbacks.
            }
            else
            {
                object->world_model = std::move(shape);
                object->world_model_base = object->world_model;
                object->world_model_basis = object->state.m_Rotation;
            }
        }
        if (state.m_ObjectType == OT_SPRITE)
        {
            object->color = {1, 1, 1}; // Sprites use multiplicative RGB; models retain additive zero.
            std::string error = "Sprite services are not attached";
            if (!sprites || !sprites->load(state.m_Filename, object->sprite, error))
            {
                contexts_.pop_back();
                remove(handle);
                fail(ObjectError::Unsupported, std::string(state.m_Name) + ": " + error);
                return nullptr;
            }
        }
        if (state.m_ObjectType == OT_MODEL && state.m_Filename[0])
        {
            if (models)
                models->load(state.m_Filename, object->model, object->model_error);
            else
                object->model_error = "Model services are not attached";
            // Inspection retains an original instance with an explicit missing
            // resource diagnostic; animation APIs cannot succeed on that instance.
            if (!object->model)
                object->animation.animation = no_animation;
        }
        if (object->life != Life::Pending)
        {
            object->life = Life::Alive;
            set_next_update(handle, state.m_NextUpdate);
            dispatch(*object, MID_INITIALUPDATE, nullptr, static_cast<float>(reason));
        }
        contexts_.pop_back();
        return handle;
    }
    bool Objects::parse_properties(HCLASS type, std::string_view input, Properties& props)
    {
        if (!knows(type))
        {
            fail(ObjectError::UnknownClass, "CreateObjectProps: unknown class");
            return false;
        }
        if (input.size() > 16384)
        {
            fail(ObjectError::Limit, "Property string exceeds 16 KiB");
            return false;
        }
        props.clear();
        std::size_t cursor = 0;
        auto whitespace = [&]
        {
            while (cursor < input.size() &&
                   (input[cursor] == ' ' || input[cursor] == '\t' || input[cursor] == '\n' || input[cursor] == '\r'))
                ++cursor;
        };
        auto invalid = [&]() -> bool
        {
            fail(ObjectError::InvalidProperty, "Invalid property string near offset " + std::to_string(cursor));
            return false;
        };
        while (true)
        {
            whitespace();
            if (cursor == input.size())
                break;
            const auto start = cursor;
            while (cursor < input.size() && input[cursor] != ' ' && input[cursor] != '\t' && input[cursor] != '=' &&
                   input[cursor] != ';')
                ++cursor;
            const auto name = input.substr(start, cursor - start);
            const auto* definition = spec(type, name);
            if (!definition || props.contains(name))
                return invalid();
            whitespace();
            if (cursor < input.size() && input[cursor] == '=')
            {
                ++cursor;
                whitespace();
            }
            if (cursor == input.size())
                return invalid();
            std::string value;
            if (input[cursor] == '"')
            {
                ++cursor;
                while (cursor < input.size() && input[cursor] != '"')
                    value += input[cursor++];
                if (cursor == input.size())
                    return invalid();
                ++cursor;
                whitespace();
                if (cursor < input.size() && input[cursor] != ';')
                    return invalid();
            }
            else
            {
                const auto begin = cursor;
                while (cursor < input.size() && input[cursor] != ';')
                    ++cursor;
                auto end = cursor;
                while (end > begin && (input[end - 1] == ' ' || input[end - 1] == '\t' || input[end - 1] == '\r' ||
                                       input[end - 1] == '\n'))
                    --end;
                value = input.substr(begin, end - begin);
            }
            PropertyValue converted = definition->value;
            if (std::holds_alternative<std::string>(converted))
                converted = value;
            else if (!parse_value(value, converted))
                return invalid();
            if (!valid(converted, definition->max_string))
                return invalid();
            props.emplace(definition->name, std::move(converted));
            if (cursor < input.size())
                ++cursor;
        }
        return true;
    }
    HOBJECT Objects::create_props(HCLASS type, ObjectCreateStruct state, std::string_view input)
    {
        Properties props;
        if (!parse_properties(type, input, props))
            return nullptr;
        // String properties override the supplied transform only when authored.
        if (spec(type, "Pos"))
            props.try_emplace("Pos", state.m_Pos);
        if (spec(type, "Rotation"))
            props.try_emplace("Rotation", state.m_Rotation);
        if (state.m_Name[0] && spec(type, "Name"))
            props.try_emplace("Name", std::string(state.m_Name));
        return create(type, state, Creation::String, props);
    }
    ObjectRecord* Objects::get(ObjectId id)
    {
        if (id.slot >= slots_.size() || slots_[id.slot].generation != id.generation)
            return nullptr;
        return slots_[id.slot].object.get();
    }
    const ObjectRecord* Objects::get(ObjectId id) const
    {
        return const_cast<Objects*>(this)->get(id);
    }
    ObjectRecord* Objects::get(HOBJECT h)
    {
        const auto it = identities_.find(h);
        return it == identities_.end() ? nullptr : get(it->second);
    }
    const ObjectRecord* Objects::get(HOBJECT h) const
    {
        return const_cast<Objects*>(this)->get(h);
    }
    HOBJECT Objects::handle_of(const BaseClass* instance) const
    {
        for (const auto& slot : slots_)
            if (slot.object && slot.object->game.get() == instance)
                return slot.object->handle;
        return nullptr;
    }
    std::vector<ObjectId> Objects::snapshot() const
    {
        // Each caller still owns a fresh snapshot across callbacks. Copy compact
        // identities without walking heap records or vacant high-water slots.
        return records_;
    }
    std::vector<ObjectId> Objects::container_snapshot() const
    {
        // Copy current bound identities before callbacks. Callers still check live
        // state/geometry; no membership, hidden flag or physics result is cached.
        return containers_;
    }
    HOBJECT Objects::next_alive(HOBJECT previous) const
    {
        std::size_t begin = 0;
        if (previous)
        {
            const auto* record = get(previous);
            if (!record)
                return nullptr;
            begin = std::size_t(record->id.slot) + 1;
        }
        for (auto i = begin; i < slots_.size(); ++i)
        {
            const auto* record = slots_[i].object.get();
            if (record && record->life == Life::Alive)
                return record->handle;
        }
        return nullptr;
    }
    const PropertyValue* Objects::property(std::string_view name) const
    {
        if (contexts_.empty())
            return nullptr;
        const auto& context = *contexts_.back();
        const auto it = context.find(name);
        return it == context.end() ? nullptr : &it->second;
    }
    bool Objects::remove(HOBJECT h)
    {
        auto* record = get(h);
        if (!record)
            return fail(ObjectError::InvalidHandle, "RemoveObject: invalid/stale handle");
        if (record->life == Life::Pending || record->life == Life::Destroying)
            return true;
        // Relations never own children. Both endpoint removals invalidate them before callbacks.
        for (auto it = attachments_.begin(); it != attachments_.end();)
        {
            if (it->second.parent == h || it->second.child == h)
            {
                if (auto* child = get(it->second.child))
                    child->attachment = 0;
                it = attachments_.erase(it);
            }
            else
                ++it;
        }
        record->life = Life::Pending;
        record->scheduled = false;
        pending_.push_back(h);
        return true;
    }
    bool Objects::set_next_update(HOBJECT h, float delay)
    {
        auto* record = get(h);
        if (!record)
            return fail(ObjectError::InvalidHandle, "SetNextUpdate: invalid/stale handle");
        if (!std::isfinite(delay) || delay < 0)
            return fail(ObjectError::InvalidTime, "SetNextUpdate: delay must be finite and nonnegative");
        if (record->life == Life::Pending || record->life == Life::Destroying)
            return false;
        record->scheduled = delay > 0;
        record->due = delay > 0 ? time_ + delay : 0;
        return true;
    }
    bool Objects::advance(double seconds)
    {
        if (advancing_ || finishing_ || callback_depth_)
            return fail(ObjectError::Busy, "Reentrant simulation step rejected");
        if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(time_ + seconds))
            return fail(ObjectError::InvalidTime, "Invalid simulation delta");
        advancing_ = true;
        time_ += seconds;
        frame_time_ = seconds;
        const auto ids = snapshot(); // New objects never extend this frame's worklist.
        for (auto id : ids)
        {
            auto* r = get(id);
            if (r && r->life == Life::Alive && (r->state.m_Flags & FLAG_REMOVEIFOUTSIDE) && world &&
                !world->models.empty())
            {
                const auto p = r->state.m_Pos;
                if (p.x < world->min.x || p.y < world->min.y || p.z < world->min.z || p.x > world->max.x ||
                    p.y > world->max.y || p.z > world->max.z)
                    remove(r->handle); // Deferred; checked identities become unavailable immediately.
            }
            if (r && r->life == Life::Alive && r->activity == OBJSTATE_ACTIVE && r->auto_deactivate &&
                r->deactivation_period > 0 && time_ >= r->deactivate_at)
                r->activity = OBJSTATE_AUTODEACTIVATE_NOW;
            if (!r || r->life != Life::Alive || r->activity != OBJSTATE_ACTIVE || !r->scheduled || r->due > time_)
                continue;
            r->scheduled = false;
            r->due = 0;
            ++r->updates;
            // MID_UPDATE's return value is not a delay. Only SetNextUpdate rearms it.
            dispatch(*r, MID_UPDATE, nullptr, 0);
        }
        advancing_ = false;
        return finish_frame();
    }
    bool Objects::finish_frame()
    {
        if (callback_depth_ || advancing_ || finishing_)
            return fail(ObjectError::Busy, "Destruction attempted inside a callback/frame");
        finishing_ = true;
        for (std::size_t i = 0; i < pending_.size(); ++i)
        {
            HOBJECT h = pending_[i];
            auto* record = get(h);
            if (!record)
                continue;
            // Detach before callbacks: callbacks may add/remove objects and links.
            std::vector<std::pair<HOBJECT, HOBJECT>> broken;
            std::erase_if(links_,
                          [&](auto link)
                          {
                              if (link.first != h && link.second != h)
                                  return false;
                              broken.push_back(link);
                              return true;
                          });
            for (auto [owner, target] : broken)
                if (auto* r = get(owner); r && r->game)
                    dispatch(*r, MID_LINKBROKEN, target, 0);
            record->life = Life::Destroying;
            const auto id = record->id;
            ++callback_depth_;
            record->game.reset();
            --callback_depth_;
            // The original destructor can query volumes or create another object.
            // Keep this binding visible until it returns; do not retain an iterator.
            if (record->container_)
                std::erase(containers_, id);
            std::erase(records_, id); // The destructor has returned; nested mutations are finished.
            identities_.erase(h);
            slots_[id.slot].object.reset();
            ++slots_[id.slot].generation;
        }
        pending_.clear();
        finishing_ = false;
        return true;
    }
    bool Objects::clear()
    {
        if (callback_depth_ || finishing_ || advancing_)
            return fail(ObjectError::Busy, "Reentrant world teardown rejected");
        clearing_ = true;
        for (const auto& s : slots_)
            if (s.object)
                remove(s.object->handle);
        const bool ok = finish_frame();
        clearing_ = false;
        return ok;
    }
    bool Objects::message(HOBJECT receiver, HOBJECT sender, DDWORD id, HMESSAGEREAD read)
    {
        auto* r = get(receiver);
        if (!r || r->life != Life::Alive || (sender && !get(sender)))
            return fail(ObjectError::InvalidHandle, "Message receiver/sender is invalid or pending removal");
        if (callback_depth_ >= 128)
            return fail(ObjectError::Limit, "Message recursion limit");
        ++callback_depth_;
        BaseClass::_ObjectMsgFn(r->game.get(), sender, id, read);
        --callback_depth_;
        return true;
    }
    bool Objects::engine_message(HOBJECT receiver, DDWORD id, void* data, float value)
    {
        auto* r = get(receiver);
        if (!r || r->life != Life::Alive)
            return fail(ObjectError::InvalidHandle, "Engine event receiver is invalid or pending removal");
        if (id == MID_TOUCHNOTIFY && r->activity != OBJSTATE_ACTIVE && r->activity != OBJSTATE_INACTIVE_TOUCH)
            return false; // Manual/automatic inactivity suppresses contact callbacks, not object messages.
        if (callback_depth_ >= 128)
            return fail(ObjectError::Limit, "Engine event recursion limit");
        dispatch(*r, id, data, value);
        return true;
    }
    bool Objects::link(HOBJECT owner, HOBJECT target)
    {
        const auto* a = get(owner);
        const auto* b = get(target);
        if (!a || !b || owner == target || a->life != Life::Alive || b->life != Life::Alive)
            return fail(ObjectError::InvalidHandle, "Invalid inter-object link");
        const auto pair = std::pair{owner, target};
        if (std::find(links_.begin(), links_.end(), pair) == links_.end())
            links_.push_back(pair);
        return true;
    }
    void Objects::unlink(HOBJECT owner, HOBJECT target)
    {
        std::erase(links_, std::pair{owner, target});
    }
    std::size_t Objects::link_count(HOBJECT owner) const
    {
        return static_cast<std::size_t>(
            std::count_if(links_.begin(), links_.end(), [owner](auto p) { return p.first == owner; }));
    }
} // namespace OpenBlood2
