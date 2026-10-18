#pragma once

#include "engine/model.h"
#include "engine/sprite.h"
#include "game/legacy/cpp_engineobjects_de.h"
#include "game/mover_messages.h"
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
    bool align_rotation(DVector forward, DRotation&);
    bool alignment_axes(DVector direction, DVector up, DVector& forward, DVector& right, bool& fallback);
    bool align_rotation(DVector forward, DVector up, DRotation&);
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
        Unsupported,
        MissingWorldModel
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
    struct Attachment
    {
        HATTACHMENT id = 0;
        HOBJECT parent = nullptr, child = nullptr;
        std::string node;
        DVector offset{};
        DRotation rotation;
        bool resolved = false;
    };
    enum class ActivationVisibility
    {
        Unqueried,
        Visible,
        Blocked,
        Invalid,
        Unavailable
    };
    struct ActivationProbe
    {
        std::uint64_t checks = 0;
        double time = 0, fraction = 1;
        ObjectId observer, hit_object;
        DVector from{}, to{};
        std::uint32_t polygon = UINT32_MAX;
        ActivationVisibility visibility = ActivationVisibility::Unqueried;
        bool refreshed = false; // Visibility alone is not a committed batch refresh.
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
        DVector color{}; // Original model additive-light RGB; shading is not implemented yet.
        float alpha = 1;
        DVector dimensions{};
        std::uint64_t stair_attempts = 0, stair_steps = 0;
        // Overlapped faces left by original MoveObjectToGround placement moves.
        std::uint64_t start_departures = 0;
        float last_step_height = 0;
        DVector velocity{}, acceleration{}, contact_normal{};
        std::uint32_t contact_polygon = UINT32_MAX;
        HOBJECT contact_object = nullptr;
        bool grounded = false;
        // Last nonzero delivered stopping force; historical values, never support authority.
        float last_impact_force = 0;
        DVector last_impact_velocity{};
        std::uint64_t impact_notifications = 0;
        bool world_body = false, character_body = false, model_body = false;
        WorldBodyProperties body_properties;
        DBYTE blocking_priority = 0;
        std::uint64_t body_steps = 0, body_pushes = 0;
        MoverResult body_result = MoverResult::Moved;
        HATTACHMENT attachment = 0;
        bool client_movement = false; // Local MoveMgr owns integration while associated.
        std::shared_ptr<const Model> model;
        std::shared_ptr<const Sprite> sprite;
        SpriteTrack sprite_track;
        const std::shared_ptr<const ContainerShape>& container() const
        {
            return container_;
        }
        // Current posed shell plus immutable authored basis for drift-free rotation.
        std::shared_ptr<const ContainerShape> world_model_base;
        DRotation world_model_basis;
        std::uint32_t rotation_steps = 0;
        // state.Pos is the current translation pivot.
        std::shared_ptr<const ContainerShape> world_model;
        std::uint64_t mover_moves = 0, mover_blocks = 0, mover_pushes = 0, mover_riders = 0;
        HOBJECT mover_blocker = nullptr;
        AnimationTrack animation;
        std::vector<std::uint8_t> hidden_nodes; // Empty means all visible; otherwise one byte per model node.
        std::string model_error;
        DDWORD user_flags = 0;
        DDWORD activity = OBJSTATE_ACTIVE;
        bool auto_deactivate = false;
        double deactivation_period = 0, deactivate_at = 0;
        std::uint64_t activation_pings = 0;
        ActivationProbe activation_probe;
        double due = 0;
        bool scheduled = false;
        std::uint64_t updates = 0;

      private:
        friend class Objects;
        std::shared_ptr<const ContainerShape> container_; // Binding and index membership change together.
    };

    class Objects
    {
      public:
        const World* world = nullptr;
        SpriteStore* sprites = nullptr;
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
        ObjectError validate_properties(HCLASS, const Properties&, std::string&) const;
        bool parse_properties(HCLASS, std::string_view, Properties&);
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
        std::size_t record_count() const
        {
            return records_.size();
        }
        std::vector<ObjectId> container_snapshot() const;
        std::size_t bound_container_count() const
        {
            return containers_.size();
        }
        HOBJECT next_alive(HOBJECT previous) const;
        const PropertyValue* property(std::string_view name) const;
        DRESULT attach(HOBJECT, HOBJECT, const char*, const DVector*, const DRotation*, HATTACHMENT*);
        DRESULT detach(HATTACHMENT);
        DRESULT find_attachment(HOBJECT, HOBJECT, HATTACHMENT*) const;
        bool refresh_attachment(HOBJECT);
        bool refresh_attachments();
        const std::map<HATTACHMENT, Attachment>& attachments() const
        {
            return attachments_;
        }
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
        std::map<HATTACHMENT, Attachment> attachments_;
        HATTACHMENT next_attachment_ = 1;
        std::map<std::string, HCLASS, CaseLess> classes_;
        std::vector<Slot> slots_;
        std::vector<ObjectId> records_;    // Occupied slots, including construction and deferred destruction.
        std::vector<ObjectId> containers_; // Bound shapes, in slot order, including deferred destruction.
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
    bool object_node_transform(const ObjectRecord&, const char*, DVector*, DRotation*);
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
