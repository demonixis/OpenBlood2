#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OpenBlood2
{
    // Own names for durable reports, but look up hot-path literals without
    // constructing (and potentially allocating) a temporary std::string.
    using ServiceCalls = std::map<std::string, std::uint64_t, std::less<>>;
    enum class ServiceSupport
    {
        Bounded,
        Unavailable,
        Unclassified
    };
    enum class ServiceBoundary
    {
        Interface,
        Lifecycle,
        Callback,
        Unknown
    };
    struct ServiceDefinition
    {
        std::string_view name;
        ServiceSupport support;
        ServiceBoundary boundary;
    };
    struct ServiceCoverage
    {
        std::string name;
        ServiceSupport support;
        ServiceBoundary boundary;
        std::uint64_t calls = 0;
    };
    std::span<const ServiceDefinition> service_catalog();
    // Called by inspection/tools only. A counted attempt is never a success assertion.
    std::vector<ServiceCoverage> service_coverage(const ServiceCalls& calls);
    const char* service_support_name(ServiceSupport);
    const char* service_boundary_name(ServiceBoundary);
    class ServiceTraceScope
    {
      public:
        explicit ServiceTraceScope(bool& enabled) : enabled_(enabled), previous_(enabled)
        {
            enabled_ = true;
        }
        ~ServiceTraceScope()
        {
            enabled_ = previous_;
        }
        ServiceTraceScope(const ServiceTraceScope&) = delete;
        ServiceTraceScope& operator=(const ServiceTraceScope&) = delete;

      private:
        bool& enabled_;
        bool previous_;
    };
} // namespace OpenBlood2
