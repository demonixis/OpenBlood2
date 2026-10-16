#include "game/server_adapter.h"
namespace OpenBlood2
{
    DRESULT ServerAdapter::CreateAttachment(HOBJECT parent, HOBJECT child, const char* node, const DVector* offset,
                                            const DRotation* rotation, HATTACHMENT* out)
    {
        trace_service("CreateAttachment");
        return objects_.attach(parent, child, node, offset, rotation, out);
    }
    DRESULT ServerAdapter::RemoveAttachment(HATTACHMENT id)
    {
        trace_service("RemoveAttachment");
        return objects_.detach(id);
    }
    DRESULT ServerAdapter::FindAttachment(HOBJECT parent, HOBJECT child, HATTACHMENT* out)
    {
        trace_service("FindAttachment");
        return objects_.find_attachment(parent, child, out);
    }
    DRESULT ServerAdapter::GetAttachedModelNodeTransform(HATTACHMENT id, const char* node, DVector* position,
                                                         DRotation* rotation)
    {
        trace_service("GetAttachedModelNodeTransform");
        const auto it = objects_.attachments().find(id);
        if (it == objects_.attachments().end())
            return DE_INVALIDPARAMS;
        return GetModelNodeTransform(it->second.child, node, position, rotation) ? DE_OK : DE_NODENOTFOUND;
    }
} // namespace OpenBlood2
