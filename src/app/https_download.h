#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

// One HTTPS GET into a file through the platform's own TLS stack: libcurl (Linux with OpenSSL,
// Windows with Schannel), NSURLSession (macOS, iOS) or HttpsURLConnection (Android). Redirects are
// returned, never followed, so the caller validates every hop.
namespace OpenBlood2
{
    struct HttpsResponse
    {
        long status = 0;
        std::string location; // Absolute redirect target of a 3xx response, else empty.
        std::uint64_t bytes = 0;
    };
    // `progress` receives the bytes written so far and returns false to cancel. More than `limit`
    // bytes fail. The file is created (or truncated); on failure its content is unspecified.
    bool https_get(std::string_view url, const std::filesystem::path& file, std::uint64_t limit,
                   const std::function<bool(std::uint64_t)>& progress, HttpsResponse& response, std::string& error);
} // namespace OpenBlood2
