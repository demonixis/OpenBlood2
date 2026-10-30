#include "client/network_collision.h"
#include "engine/resources.h"
#include "net/collision_codec.h"

namespace OpenBlood2
{
    namespace
    {
        bool same(DVector a, DVector b)
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
        bool same(DRotation a, DRotation b)
        {
            return same(a.m_Vec, b.m_Vec) && a.m_Spin == b.m_Spin;
        }
    } // namespace
    bool NetworkCollisionReceiver::begin(std::uint64_t world, const World& geometry, std::string& error)
    {
        error.clear();
        if (!world || world <= world_.frame().world || geometry.models.empty())
        {
            error = "Missing or stale client collision geometry";
            return false;
        }
        auto primary = std::make_shared<CollisionWorld>();
        if (!primary->build(geometry.models.front(), error))
            return false;
        auto owned = std::make_shared<const World>(geometry);
        if (!world_.begin(world))
            return false;
        geometry_ = std::move(owned);
        primary_ = std::move(primary);
        shapes_.clear();
        return true;
    }
    bool NetworkCollisionReceiver::shape(const ClientObject& object, std::shared_ptr<const ContainerShape>& out,
                                         std::string& error)
    {
        const auto& c = object.collision;
        const auto index = object.world_model_index;
        if (!geometry_ || !index || index >= geometry_->models.size() ||
            CaseLess{}(geometry_->models[index].name, object.filename) ||
            CaseLess{}(object.filename, geometry_->models[index].name))
        {
            error = "Network collider DAT binding mismatch: " + object.filename;
            return false;
        }
        auto& cached = shapes_[object.id.slot];
        if (cached.id != object.id || cached.index != index || cached.world_model != c.world_model ||
            !same(cached.basis, c.basis))
        {
            auto base = std::make_shared<ContainerShape>();
            if (!base->build(geometry_->models[index], c.basis, error, c.world_model))
                return false;
            cached = {};
            cached.id = object.id;
            cached.index = index;
            cached.basis = c.basis;
            cached.world_model = c.world_model;
            cached.base = std::move(base);
        }
        if (!c.shape_transformed)
        {
            if (!same(c.motion.shape_origin, cached.base->origin))
            {
                error = "Untransformed network shape has a displaced origin";
                return false;
            }
            out = cached.base;
            return true;
        }
        if (!cached.posed || !same(cached.rotation, object.pose.rotation) ||
            !same(cached.origin, c.motion.shape_origin))
        {
            auto posed = std::make_shared<ContainerShape>(*cached.base);
            if (!posed->transform(multiply_rotation(object.pose.rotation, inverse_rotation(c.basis)),
                                  c.motion.shape_origin))
            {
                error = "Invalid network collision shape transform";
                return false;
            }
            cached.posed = std::move(posed);
            cached.rotation = object.pose.rotation;
            cached.origin = c.motion.shape_origin;
        }
        out = cached.posed;
        return true;
    }
    bool NetworkCollisionReceiver::receive(const ClientWorld& source, std::string& error)
    {
        error.clear();
        const auto& input = source.frame();
        if (!primary_ || input.world != world_.frame().world || input.sequence <= world_.frame().sequence)
        {
            error = "Invalid network collision frame";
            return false;
        }
        ClientCollisionFrame frame;
        frame.world = input.world;
        frame.sequence = input.sequence;
        frame.primary = primary_;
        frame.objects.reserve(input.objects.size());
        for (const auto& object : input.objects)
        {
            const auto& c = object.collision;
            if (!c.enabled)
                continue;
            if (!Net::valid_collision_values(c))
            {
                error = "Invalid network collision properties";
                return false;
            }
            ClientCollider collider;
            collider.id = object.id;
            collider.ignored_by = c.ignored_by;
            collider.position = object.pose.position;
            collider.dimensions = c.motion.dimensions;
            collider.velocity = c.motion.velocity;
            collider.acceleration = c.motion.acceleration;
            collider.flags = object.flags;
            collider.user_flags = object.user_flags;
            collider.type = object.type;
            collider.attached = object.attached;
            collider.world_model = c.world_model;
            collider.container = c.container;
            collider.dynamic = c.dynamic;
            collider.pushable = c.pushable;
            collider.priority = c.priority;
            collider.volume_brush = c.volume_brush;
            collider.environment = c.environment;
            if ((c.world_model || c.container) && !shape(object, collider.shape, error))
                return false;
            frame.objects.push_back(std::move(collider));
        }
        if (input.player)
        {
            const auto* player = source.get(input.player);
            if (!player || !player->collision.enabled)
            {
                error = "Network player has no collision body";
                return false;
            }
        }
        if (!world_.apply(std::move(frame), error))
            return false;
        for (auto it = shapes_.begin(); it != shapes_.end();)
        {
            const auto* object = world_.get(it->second.id);
            if (!object || !object->shape)
                it = shapes_.erase(it);
            else
                ++it;
        }
        return true;
    }
} // namespace OpenBlood2
