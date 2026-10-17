#include "game/service_coverage.h"
#include <algorithm>
namespace OpenBlood2
{
    std::span<const ServiceDefinition> service_catalog()
    {
        static constexpr ServiceDefinition definitions[]{
#define BLOOD2_SERVICE(name, support, boundary) {#name, ServiceSupport::support, ServiceBoundary::boundary},
#include "game/service_catalog.inc"
#undef BLOOD2_SERVICE
        };
        return definitions;
    }
    std::vector<ServiceCoverage> service_coverage(const std::map<std::string, std::uint64_t>& calls)
    {
        std::vector<ServiceCoverage> rows;
        rows.reserve(service_catalog().size() + calls.size());
        for (const auto& service : service_catalog())
        {
            const auto found = calls.find(std::string(service.name));
            rows.push_back({std::string(service.name), service.support, service.boundary,
                            found == calls.end() ? 0 : found->second});
        }
        for (const auto& [name, count] : calls)
            if (std::none_of(service_catalog().begin(), service_catalog().end(),
                             [&](const auto& service) { return service.name == name; }))
                rows.push_back({name, ServiceSupport::Unclassified, ServiceBoundary::Unknown, count});
        return rows;
    }
    const char* service_support_name(ServiceSupport support)
    {
        switch (support)
        {
            case ServiceSupport::Bounded:
                return "Implemented (bounded)";
            case ServiceSupport::Unavailable:
                return "Explicitly unsupported";
            case ServiceSupport::Unclassified:
                return "Unclassified";
        }
        return "Unclassified";
    }
    const char* service_boundary_name(ServiceBoundary boundary)
    {
        switch (boundary)
        {
            case ServiceBoundary::Interface:
                return "ServerDE";
            case ServiceBoundary::Lifecycle:
                return "Lifecycle";
            case ServiceBoundary::Callback:
                return "Callback";
            case ServiceBoundary::Unknown:
                return "Unknown";
        }
        return "Unknown";
    }
} // namespace OpenBlood2
