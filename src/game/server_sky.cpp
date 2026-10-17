#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool ServerAdapter::SetSkyDefinition(const SkyDefinition& d)
    {
        trace_service("SetSkyDef");
        if (sky.set(d))
            return true;
        ReportSkyFailure(sky.error);
        return false;
    }
    bool ServerAdapter::AddSkyObject(HOBJECT h, unsigned index)
    {
        trace_service("AddObjectToSky");
        if (sky.add(objects_, h, index))
            return true;
        ReportSkyFailure(sky.error);
        return false;
    }
    void ServerAdapter::ReportSkyFailure(std::string_view message)
    {
        sky.error = message;
        objects_.fail(ObjectError::Unsupported, sky.error);
        ++unavailable_services["Sky: " + sky.error];
    }
} // namespace OpenBlood2
