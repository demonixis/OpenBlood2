#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool ServerAdapter::SetSkyDefinition(const SkyDefinition& d)
    {
        trace_service("SetSkyDefinition");
        if (sky.set(d))
            return true;
        ReportSkyFailure(sky.error);
        return false;
    }
    bool ServerAdapter::AddSkyObject(HOBJECT h, unsigned index)
    {
        trace_service("AddSkyObject");
        if (sky.add(objects_, h, index))
            return true;
        ReportSkyFailure(sky.error);
        return false;
    }
    void ServerAdapter::ReportSkyFailure(std::string_view message)
    {
        trace_service("ReportSkyFailure");
        sky.error = message;
        objects_.fail(ObjectError::Unsupported, sky.error);
        record_unavailable("Sky: " + sky.error);
    }
} // namespace OpenBlood2
