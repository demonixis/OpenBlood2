// libcurl transport (Linux with OpenSSL, Windows with Schannel).
#include "app/https_download.h"
#include "engine/config.h"
#include <SDL3/SDL_iostream.h>
#include <curl/curl.h>

namespace OpenBlood2
{
    namespace
    {
        struct Writer
        {
            SDL_IOStream* io = nullptr;
            const std::function<bool(std::uint64_t)>* progress = nullptr;
            std::uint64_t written = 0, limit = 0;
            bool cancelled = false;
        };
        size_t write(char* bytes, size_t size, size_t count, void* opaque)
        {
            auto& writer = *static_cast<Writer*>(opaque);
            if (size && count > (writer.limit - writer.written) / size)
                return 0;
            const auto n = SDL_WriteIO(writer.io, bytes, size * count);
            writer.written += n;
            return n;
        }
        int transfer(void* opaque, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
        {
            auto& writer = *static_cast<Writer*>(opaque);
            writer.cancelled = writer.cancelled || !(*writer.progress)(writer.written);
            return writer.cancelled ? 1 : 0;
        }
    } // namespace
    bool https_get(std::string_view url, const std::filesystem::path& file, std::uint64_t limit,
                   const std::function<bool(std::uint64_t)>& progress, HttpsResponse& response, std::string& error)
    {
        response = {};
        // Thread-safe one-time global initialization, before easy handles exist.
        struct CurlRuntime
        {
            CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
            ~CurlRuntime()
            {
                if (result == CURLE_OK)
                    curl_global_cleanup();
            }
        };
        static CurlRuntime runtime;
        if (runtime.result != CURLE_OK)
        {
            error = "Cannot initialize HTTPS download";
            return false;
        }
        auto* io = SDL_IOFromFile(path_text(file).c_str(), "wb");
        if (!io)
        {
            error = "Cannot create download file: " + std::string(SDL_GetError());
            return false;
        }
        auto* curl = curl_easy_init();
        if (!curl)
        {
            SDL_CloseIO(io);
            error = "Cannot create HTTPS download";
            return false;
        }
        Writer writer{io, &progress, 0, limit};
        const std::string address(url);
        char details[CURL_ERROR_SIZE]{};
        curl_easy_setopt(curl, CURLOPT_URL, address.c_str());
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, long(CURLSSLOPT_NATIVE_CA));
#endif
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, curl_off_t(limit));
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "OpenBlood2/1");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writer);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &writer);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, details);
        const auto result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
        char* next = nullptr;
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &next);
        if (response.status >= 300 && response.status < 400 && next)
            response.location = next;
        curl_easy_cleanup(curl);
        response.bytes = writer.written;
        if (!SDL_CloseIO(io))
        {
            error = "Cannot finish the download file";
            return false;
        }
        if (writer.cancelled)
        {
            error = "Installation cancelled";
            return false;
        }
        if (result != CURLE_OK)
        {
            error = "Download failed: " + std::string(details[0] ? details : curl_easy_strerror(result));
            return false;
        }
        return true;
    }
} // namespace OpenBlood2
