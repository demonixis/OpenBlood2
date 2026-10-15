#pragma once

#include "engine/model.h"
#include "game/legacy/cpp_engineobjects_de.h"
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace OpenBlood2
{
    class ModelStore;
    class ContainerShape;
    struct World;

    struct CaseLess
    {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const;
    };
    struct EulerAngles
    {
        DVector radians;
    };
    DRotation rotation_from_euler(DVector radians);
    using PropertyValue = std::variant<std::string, float, bool, std::int32_t, DVector, DRotation, EulerAngles>;
    using Properties = std::map<std::string, PropertyValue, CaseLess>;
    struct PropertySpec
    {
        std::string name;
        PropertyValue value;
        bool defaulted = true;
        std::size_t max_string = MAX_CS_FILENAME_LEN;
    };
    struct ClassInfo
    {
        std::string name;
        HCLASS parent;
        std::unique_ptr<BaseClass> (*construct)();
        std::vector<PropertySpec> properties;
    };
    enum class Creation : unsigned
    {
        Runtime,
        World,
        String,
        Restore
    };
    enum class Life
    {
        Constructing,
        Alive,
        Pending,
        Destroying
    };
    enum class ObjectError
    {
        None,
        InvalidHandle,
        UnknownClass,
        InvalidProperty,
        Rejected,
        InvalidTime,
        Busy,
        Limit,
        Unsupported
    };
    struct ObjectId
    {
        std::uint32_t slot = UINT32_MAX, generation = 0;
        bool operator==(const ObjectId&) const = default;
        explicit operator bool() const
        {
            return slot != UINT32_MAX;
        }
    };
    struct ObjectRecord
    {
        ObjectId id;
        HOBJECT handle = nullptr;
        HCLASS type = nullptr;
        std::unique_ptr<BaseClass> game;
        ObjectCreateStruct state;
        Properties properties;
        Creation reason = Creation::Runtime;
        Life life = Life::Constructing;
        DVector dimensions{};
        std::uint64_t stair_attempts = 0, stair_steps = 0;
        float last_step_height = 0;
        DVector velocity{}, acceleration{}, contact_normal{};
        std::uint32_t contact_polygon = UINT32_MAX;
        HOBJECT contact_object = nullptr;
        bool grounded = false;
        bool client_movement = false; // Local MoveMgr owns integration while associated.
        std::shared_ptr<const Model> model;
        std::shared_ptr<const ContainerShape> container;
        // Immutable authored shell; state.Pos is its current translation pivot.
        std::shared_ptr<const ContainerShape> world_model;
        std::uint64_t mover_moves = 0, mover_blocks = 0, mover_pushes = 0, mover_riders = 0;
        HOBJECT mover_blocker = nullptr;
        AnimationTrack animation;
        std::string model_error;
        DDWORD user_flags = 0;
        DDWORD activity = OBJSTATE_ACTIVE;
        double due = 0;
        bool scheduled = false;
        std::uint64_t updates = 0;
    };

    class Objects
    {
      public:
        const World* world = nullptr;
        ModelStore* models = nullptr; // Optional decoded resources, attached before creation.
        Objects() = default;
        ~Objects();
        Objects(const Objects&) = delete;
        Objects& operator=(const Objects&) = delete;
        bool register_class(const ClassInfo& type);
        HCLASS find_class(std::string_view name) const;
        bool knows(HCLASS type) const;
        static bool is_kind_of(HCLASS type, HCLASS parent);
        HOBJECT create(HCLASS type, ObjectCreateStruct state = {}, Creation reason = Creation::Runtime,
                       const Properties& properties = {});
        HOBJECT create_props(HCLASS type, ObjectCreateStruct state, std::string_view text);
        bool remove(HOBJECT handle);
        bool set_next_update(HOBJECT handle, float delay);
        bool advance(double seconds);
        bool finish_frame();
        bool clear();
        bool busy() const
        {
            return callback_depth_ || advancing_ || finishing_ || clearing_;
        }
        bool message(HOBJECT receiver, HOBJECT sender, DDWORD id, HMESSAGEREAD read);
        bool engine_message(HOBJECT receiver, DDWORD id, void* data, float value = 0);
        ObjectRecord* get(HOBJECT handle);
        const ObjectRecord* get(HOBJECT handle) const;
        ObjectRecord* get(ObjectId id);
        const ObjectRecord* get(ObjectId id) const;
        HOBJECT handle_of(const BaseClass* instance) const;
        std::vector<ObjectId> snapshot() const;
        HOBJECT next_alive(HOBJECT previous) const;
        const PropertyValue* property(std::string_view name) const;
        bool link(HOBJECT owner, HOBJECT target);
        void unlink(HOBJECT owner, HOBJECT target);
        std::size_t link_count(HOBJECT owner) const;
        double time() const
        {
            return time_;
        }
        double frame_time() const
        {
            return frame_time_;
        }
        ObjectError error() const
        {
            return error_;
        }
        const std::vector<std::string>& diagnostics() const
        {
            return diagnostics_;
        }
        bool fail(ObjectError error, std::string detail);

      private:
        struct Slot
        {
            std::uint32_t generation = 1;
            std::unique_ptr<ObjectRecord> object;
        };
        std::map<std::string, HCLASS, CaseLess> classes_;
        std::vector<Slot> slots_;
        std::vector<std::unique_ptr<DObject>> handles_;
        std::unordered_map<HOBJECT, ObjectId> identities_;
        std::vector<const Properties*> contexts_;
        std::vector<HOBJECT> pending_;
        std::vector<std::pair<HOBJECT, HOBJECT>> links_;
        std::vector<std::string> diagnostics_;
        ObjectError error_ = ObjectError::None;
        double time_ = 0, frame_time_ = 0;
        unsigned callback_depth_ = 0;
        bool advancing_ = false, finishing_ = false, clearing_ = false;
        DDWORD dispatch(ObjectRecord& record, DDWORD id, void* data, float reason);
    };

    struct ObjectSelection
    {
        ObjectId id{};
        void refresh(const Objects& objects)
        {
            if (!objects.get(id))
                id = {};
        }
    };
    std::string property_text(const PropertyValue& value);
    bool generic_property(const PropertyValue& value, GenericProp& out);
    const char* life_name(Life life);
    const char* creation_name(Creation reason);

} // namespace OpenBlood2

// Stable pointer-shaped legacy token. Engine IDs are never cast to pointers.
struct DObject
{
    OpenBlood2::ObjectId id;
};
