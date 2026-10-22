#include "engine/collision.h"
#include "engine/objects.h"
#include <cmath>
namespace OpenBlood2
{
    DRESULT Objects::attach(HOBJECT parent, HOBJECT child, const char* node, const DVector* offset,
                            const DRotation* rotation, HATTACHMENT* out)
    {
        if (out)
            *out = 0;
        if (pure_)
        {
            refuse_impure("CreateAttachment");
            return DE_ERROR;
        }
        auto* p = get(parent);
        auto* c = get(child);
        if (!out || !p || !c || p == c || p->life != Life::Alive || c->life != Life::Alive || !offset || !rotation ||
            !physics_vector(*offset) || !physics_vector(rotation->m_Vec) || !std::isfinite(rotation->m_Spin))
            return DE_INVALIDPARAMS;
        const auto norm = std::sqrt(vector_dot(rotation->m_Vec, rotation->m_Vec) + rotation->m_Spin * rotation->m_Spin);
        if (!std::isfinite(norm) || norm < 1e-6f)
            return DE_INVALIDPARAMS;
        // Touch-only children retain their flags, but attached records are excluded from contacts.
        // Bounded visual children only. Do not silently replace physics or permit cycles/chains.
        if (p->attachment || c->attachment || c->container() || c->world_model || c->client_movement || c->model_body ||
            (c->state.m_ObjectType != OT_MODEL && c->state.m_ObjectType != OT_NORMAL &&
             c->state.m_ObjectType != OT_SPRITE) ||
            (c->state.m_Flags & (FLAG_SOLID | FLAG_GRAVITY | FLAG_RAYHIT)) ||
            vector_dot(c->velocity, c->velocity) != 0 || vector_dot(c->acceleration, c->acceleration) != 0)
            return DE_ERROR;
        for (const auto& [id, a] : attachments_)
            if (a.parent == child)
                return DE_ERROR;
        std::string name;
        if (node)
        {
            std::size_t n = 0;
            while (n <= MAX_CS_FILENAME_LEN && node[n])
                ++n;
            if (n > MAX_CS_FILENAME_LEN)
                return DE_INVALIDPARAMS;
            name.assign(node, n);
        }
        if (next_attachment_ > 65536)
            return DE_ERROR;
        Attachment a{next_attachment_,
                     parent,
                     child,
                     name,
                     *offset,
                     {vector_scale(rotation->m_Vec, 1 / norm), rotation->m_Spin / norm},
                     false};
        DVector position = p->state.m_Pos;
        DRotation base = p->state.m_Rotation;
        if (!name.empty() && !object_node_transform(*p, name.c_str(), &position, &base))
            return DE_NODENOTFOUND;
        position = vector_add(position, rotate_vector(base, a.offset));
        if (!physics_vector(position))
            return DE_INVALIDPARAMS;
        c->state.m_Pos = position;
        c->state.m_Rotation = compose_rotation(base, a.rotation);
        a.resolved = true;
        c->attachment = a.id;
        attachments_.emplace(a.id, a);
        *out = next_attachment_++;
        return DE_OK;
    }
    DRESULT Objects::detach(HATTACHMENT id)
    {
        if (pure_)
        {
            refuse_impure("RemoveAttachment");
            return DE_ERROR;
        }
        auto it = attachments_.find(id);
        if (it == attachments_.end())
            return DE_NOTFOUND;
        if (auto* child = get(it->second.child))
            child->attachment = 0;
        attachments_.erase(it);
        return DE_OK;
    }
    DRESULT Objects::find_attachment(HOBJECT parent, HOBJECT child, HATTACHMENT* out) const
    {
        if (!out)
            return DE_INVALIDPARAMS;
        *out = 0;
        const auto* p = get(parent);
        const auto* c = get(child);
        if (!p || !c || p->life != Life::Alive || c->life != Life::Alive)
            return DE_INVALIDPARAMS;
        auto it = attachments_.find(c->attachment);
        if (it == attachments_.end() || it->second.parent != parent)
            return DE_NOTFOUND;
        *out = it->first;
        return DE_OK;
    }
    bool Objects::refresh_attachment(HOBJECT child)
    {
        auto* c = get(child);
        if (!c)
            return false;
        if (!c->attachment)
            return true;
        if (c->life != Life::Alive)
            return false;
        auto it = attachments_.find(c->attachment);
        if (it == attachments_.end())
            return false;
        auto& a = it->second;
        const auto* p = get(a.parent);
        a.resolved = false;
        if (!p || p->life != Life::Alive)
            return false;
        auto position = p->state.m_Pos;
        auto rotation = p->state.m_Rotation;
        if (!a.node.empty() && !object_node_transform(*p, a.node.c_str(), &position, &rotation))
            return false;
        position = vector_add(position, rotate_vector(rotation, a.offset));
        if (!physics_vector(position))
            return false;
        c->state.m_Pos = position;
        c->state.m_Rotation = compose_rotation(rotation, a.rotation);
        a.resolved = true;
        return true;
    }
    bool Objects::refresh_attachments()
    {
        bool ok = true;
        for (const auto& [id, a] : attachments_)
            ok = refresh_attachment(a.child) && ok;
        if (!ok)
            fail(ObjectError::InvalidProperty, "Attachment pose unavailable; last child transform retained");
        return ok;
    }
} // namespace OpenBlood2
