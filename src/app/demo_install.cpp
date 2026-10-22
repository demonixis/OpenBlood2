#include "app/demo_install.h"
#include "app/game_setup.h"
#include "app/https_download.h"
#include "engine/resources.h"
#include "engine/sha256.h"
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <array>
#include <miniz.h>
#include <set>

namespace OpenBlood2
{
    namespace fs = std::filesystem;
    namespace
    {
        constexpr std::uint64_t compressed_limit = 64 * 1024 * 1024, expanded_limit = 128 * 1024 * 1024;
        constexpr char download_url[] = "https://archive.org/download/B2demo/B2Demo.zip";
        struct Io
        {
            SDL_IOStream* value = nullptr;
            ~Io()
            {
                if (value)
                    SDL_CloseIO(value);
            }
            bool close()
            {
                auto* p = value;
                value = nullptr;
                return !p || SDL_CloseIO(p);
            }
        };
        bool failure(std::string& error, std::string text)
        {
            error = std::move(text);
            return false;
        }
        struct Zip
        {
            mz_zip_archive value{};
            ~Zip()
            {
                if (value.m_pState)
                    mz_zip_reader_end(&value);
            }
        };
        size_t zip_read(void* opaque, mz_uint64 offset, void* buffer, size_t size)
        {
            auto* io = static_cast<SDL_IOStream*>(opaque);
            if (offset > compressed_limit || SDL_SeekIO(io, Sint64(offset), SDL_IO_SEEK_SET) != Sint64(offset))
                return 0;
            return SDL_ReadIO(io, buffer, size);
        }
        struct Writer
        {
            SDL_IOStream* io;
            InstallProgress* progress;
            std::uint64_t written = 0, limit = 0;
        };
        size_t zip_write(void* opaque, mz_uint64 offset, const void* buffer, size_t size)
        {
            auto& writer = *static_cast<Writer*>(opaque);
            if (writer.progress->cancel || offset != writer.written || size > writer.limit - writer.written)
                return 0;
            const auto count = SDL_WriteIO(writer.io, buffer, size);
            writer.written += count;
            writer.progress->bytes += count;
            return count;
        }
        bool download(const fs::path& file, InstallProgress& progress, std::string& error)
        {
            std::string url = download_url;
            progress.total = demo_package().bytes;
            for (unsigned redirects = 0; redirects <= 5; ++redirects)
            {
                if (!demo_url_allowed(url))
                    return failure(error, "Demo redirect left the approved Archive.org host");
                progress.bytes = 0;
                HttpsResponse response;
                if (!https_get(
                        url, file, demo_package().bytes,
                        [&](std::uint64_t bytes)
                        {
                            progress.bytes = bytes;
                            return !progress.cancel;
                        },
                        response, error))
                    return progress.cancel ? failure(error, "Installation cancelled") : false;
                if (response.status >= 300 && response.status < 400 && !response.location.empty())
                {
                    url = std::move(response.location);
                    continue;
                }
                if (response.status != 200 || response.bytes != demo_package().bytes)
                    return failure(error, "Demo server returned an unexpected response or file length");
                return true;
            }
            return failure(error, "Too many demo download redirects");
        }
        // The installed data, as the game will mount it.
        bool validate_demo(const fs::path& directory, std::string& error)
        {
            Config config;
            config.data_dir = directory;
            config.demo = true;
            DataStatus status;
            Resources resources;
            if (!inspect_data(config, status, error) || !resources.mount_game(config, error))
                return false;
            if (resources.index().empty())
                return failure(error, "The installed demo has no resources");
            return true;
        }
        bool unpack(const fs::path& file, const fs::path& destination, InstallProgress& progress, std::string& error)
        {
            Io input{SDL_IOFromFile(path_text(file).c_str(), "rb")};
            if (!input.value)
                return failure(error, "Cannot open demo ZIP");
            const auto size = SDL_GetIOSize(input.value);
            Zip zip;
            zip.value.m_pRead = zip_read;
            zip.value.m_pIO_opaque = input.value;
            if (size <= 0 || std::uint64_t(size) > compressed_limit ||
                !mz_zip_reader_init(&zip.value, mz_uint64(size), 0))
                return failure(error, "Invalid demo ZIP");
            const auto count = mz_zip_reader_get_num_files(&zip.value);
            if (!count || count > 256)
                return failure(error, "Demo ZIP exceeds the entry limit");
            struct Entry
            {
                mz_uint index;
                std::string path;
                std::uint64_t bytes;
                bool directory;
            };
            std::vector<Entry> entries;
            std::set<std::string> names, regular;
            std::uint64_t total = 0;
            // Validate every path, including setup files which are deliberately not extracted.
            for (mz_uint i = 0; i < count; ++i)
            {
                mz_zip_archive_file_stat stat{};
                std::array<char, 1025> path{};
                const auto length = mz_zip_reader_get_filename(&zip.value, i, path.data(), path.size());
                if (!mz_zip_reader_file_stat(&zip.value, i, &stat) || !length || length > path.size() ||
                    length != SDL_strlen(path.data()) + 1 || !valid_setup_text(path.data(), 1024))
                    return failure(error, "Invalid or overlong ZIP entry name");
                std::string original(path.data());
                std::replace(original.begin(), original.end(), '\\', '/');
                const bool directory = stat.m_is_directory;
                if (directory && original.ends_with('/'))
                    original.pop_back();
                std::string key;
                if (!resource_path(original, key) || key.find(':') != key.npos || !names.insert(key).second ||
                    stat.m_is_encrypted || !stat.m_is_supported ||
                    (((stat.m_external_attr >> 16) & 0170000) != 0 &&
                     ((stat.m_external_attr >> 16) & 0170000) != 0100000 &&
                     ((stat.m_external_attr >> 16) & 0170000) != 0040000))
                    return failure(error, "Unsafe, duplicate or unsupported ZIP entry");
                // Windows device names/trailing dots are rejected on every platform.
                std::size_t begin = 0;
                while (begin < key.size())
                {
                    auto end = key.find('/', begin);
                    if (end == key.npos)
                        end = key.size();
                    const auto part = key.substr(begin, end - begin);
                    const auto stem = part.substr(0, part.find('.'));
                    if (part.back() == '.' || part.back() == ' ' || stem == "con" || stem == "prn" || stem == "aux" ||
                        stem == "nul" ||
                        (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '0' &&
                         stem[3] <= '9'))
                        return failure(error, "Unsafe ZIP destination name");
                    begin = end + 1;
                }
                if (stat.m_uncomp_size > expanded_limit - total)
                    return failure(error, "Demo ZIP exceeds the expanded size limit");
                total += stat.m_uncomp_size;
                if (!directory)
                    regular.insert(key);
                if (key.starts_with("game/"))
                    entries.push_back({i, original.substr(5), stat.m_uncomp_size, directory});
            }
            for (const auto& key : names)
                for (auto slash = key.find('/'); slash != key.npos; slash = key.find('/', slash + 1))
                    if (regular.contains(key.substr(0, slash)))
                        return failure(error, "ZIP file/directory collision");
            if (entries.empty())
                return failure(error, "Demo ZIP has no Game directory");
            progress.stage = InstallStage::Extracting;
            progress.bytes = 0;
            progress.total = total;
            for (const auto& entry : entries)
            {
                if (progress.cancel)
                    return failure(error, "Installation cancelled");
                const auto target = destination / path_from_utf8(entry.path);
                std::error_code ec;
                fs::create_directories(entry.directory ? target : target.parent_path(), ec);
                if (ec)
                    return failure(error, "Cannot create demo directory: " + ec.message());
                if (entry.directory)
                    continue;
                Io output{SDL_IOFromFile(path_text(target).c_str(), "wbx")};
                if (!output.value)
                    return failure(error, "Cannot write demo file: " + path_text(target));
                Writer writer{output.value, &progress, 0, entry.bytes};
                if (!mz_zip_reader_extract_to_callback(&zip.value, entry.index, zip_write, &writer, 0) ||
                    writer.written != entry.bytes || !output.close())
                    return failure(error,
                                   progress.cancel ? "Installation cancelled" : "Demo ZIP extraction or CRC failed");
            }
            return true;
        }
    } // namespace
    const DemoPackage& demo_package()
    {
        static const DemoPackage package{37695395, "2f22bed184ed2a9c39b448041603ee7199310f83cfa791e1a26cd4bdc67b9bc1",
                                         "e410ce2944210c776c7407a79e01cfa031c782132f68407cd0871379c5e7218c"};
        return package;
    }
    bool demo_url_allowed(std::string_view url)
    {
        constexpr std::string_view scheme = "https://";
        if (url.size() > 4096 || url.substr(0, scheme.size()) != scheme)
            return false;
        auto authority = url.substr(scheme.size());
        authority = authority.substr(0, authority.find_first_of("/?#"));
        if (authority.empty() || authority.find('@') != authority.npos || authority.find('[') != authority.npos)
            return false;
        std::string host(authority.substr(0, authority.find(':')));
        if (const auto colon = authority.find(':'); colon != authority.npos && authority.substr(colon + 1) != "443")
            return false;
        std::transform(host.begin(), host.end(), host.begin(),
                       [](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c + 32 : c); });
        if (host.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789.-") != host.npos)
            return false;
        return host == "archive.org" || (host.size() > 12 && host.ends_with(".archive.org"));
    }
    bool file_sha256(const fs::path& path, std::string& digest, std::string& error, InstallProgress& progress)
    {
        Io io{SDL_IOFromFile(path_text(path).c_str(), "rb")};
        const auto size = io.value ? SDL_GetIOSize(io.value) : -1;
        if (size < 0 || std::uint64_t(size) > expanded_limit)
            return failure(error, "Cannot read bounded package file");
        Sha256 hash;
        bool ok = true;
        std::array<unsigned char, 65536> buffer{};
        std::uint64_t bytes = 0;
        while (ok && bytes < std::uint64_t(size))
        {
            const auto n = SDL_ReadIO(io.value, buffer.data(), std::min<std::uint64_t>(buffer.size(), size - bytes));
            ok = n && !progress.cancel;
            hash.update(std::span(buffer.data(), n));
            bytes += n;
        }
        if (!ok || progress.cancel)
            return failure(error, progress.cancel ? "Installation cancelled" : "Cannot verify package SHA-256");
        digest = digest_text(hash.finish());
        return true;
    }
    bool extract_demo(const fs::path& zip, const fs::path& destination, const DemoPackage& package,
                      InstallProgress& progress, std::string& error)
    {
        progress.stage = InstallStage::Checking;
        std::error_code ec;
        const auto size = fs::file_size(zip, ec);
        if (ec || size != package.bytes || size > compressed_limit)
            return failure(error, "Demo ZIP size does not match the qualified package");
        std::string hash;
        if (!file_sha256(zip, hash, error, progress))
            return false;
        if (hash != package.sha256)
            return failure(error, "Demo ZIP checksum does not match the qualified package");
        if (!fs::create_directory(destination, ec))
            return failure(error, "Demo destination already exists or cannot be created");
        bool ok = unpack(zip, destination, progress, error);
        if (ok)
        {
            progress.stage = InstallStage::Validating;
            ok = file_sha256(destination / "B2Demo.rez", hash, error, progress);
            if (ok && hash != package.rez_sha256)
                ok = failure(error, "Extracted demo REZ checksum mismatch");
            if (ok)
                ok = validate_demo(destination, error);
            if (ok && progress.cancel)
                ok = failure(error, "Installation cancelled");
        }
        if (!ok)
            fs::remove_all(destination, ec);
        return ok;
    }
    DemoInstaller::~DemoInstaller()
    {
        cancel();
        if (thread_)
            SDL_WaitThread(thread_, nullptr);
    }
    bool DemoInstaller::start(const fs::path& content, const fs::path& local)
    {
        if (pending() || !content.is_absolute() || (!local.empty() && !local.is_absolute()))
            return false;
        content_ = content;
        local_ = local;
        progress.cancel = false;
        progress.bytes = 0;
        progress.total = 0;
        progress.stage = local.empty() ? InstallStage::Downloading : InstallStage::Checking;
        thread_ = SDL_CreateThread(execute, "Demo installer", this);
        return thread_ != nullptr;
    }
    int SDLCALL DemoInstaller::execute(void* ptr)
    {
        auto& self = *static_cast<DemoInstaller*>(ptr);
        InstallResult result;
        self.work(result);
        std::lock_guard lock(self.mutex_);
        self.result_ = std::move(result);
        return 0;
    }
    void DemoInstaller::work(InstallResult& result)
    {
        std::error_code ec;
        const auto& parent = content_;
        fs::create_directories(parent, ec);
        if (ec)
        {
            result.error = "Cannot create user content storage: " + ec.message();
            return;
        }
        fs::path staging;
        std::string id;
        for (unsigned i = 0; i < 32; ++i)
        {
            id = std::to_string(SDL_GetTicksNS()) + "-" + std::to_string(i);
            const auto candidate = parent / (".demo-install-" + id);
            if (fs::create_directory(candidate, ec))
            {
                staging = candidate;
                break;
            }
            if (ec)
                break;
        }
        if (staging.empty())
        {
            result.error = "Cannot reserve demo staging directory";
            return;
        }
        const auto zip = staging / "B2Demo-original.zip";
        bool ok;
        if (local_.empty())
            ok = download(zip, progress, result.error);
        else
        {
            const auto size = fs::file_size(local_, ec);
            ok = !ec && size == demo_package().bytes && fs::copy_file(local_, zip, fs::copy_options::none, ec);
            if (!ok)
                result.error = "Cannot import qualified demo ZIP";
        }
        if (ok)
            ok = extract_demo(zip, staging / "Game", demo_package(), progress, result.error);
        const auto published = parent / ("Blood2Demo-1.0-" + id);
        if (ok && progress.cancel)
        {
            result.error = "Installation cancelled";
            ok = false;
        }
        if (ok)
        {
            // Same-filesystem atomic publication. Never replace an existing installation.
            if (fs::exists(published, ec) || ec)
            {
                result.error = "Demo destination is occupied";
                ok = false;
            }
            else
            {
                fs::rename(staging, published, ec);
                if (ec)
                {
                    result.error = "Cannot publish demo installation: " + ec.message();
                    ok = false;
                }
            }
        }
        if (!ok)
            fs::remove_all(staging, ec);
        else
        {
            result.directory = published / "Game";
            result.success = true;
            progress.stage = InstallStage::Complete;
        }
    }
    bool DemoInstaller::poll(InstallResult& result)
    {
        {
            std::lock_guard lock(mutex_);
            if (!result_)
                return false;
            result = std::move(*result_);
            result_.reset();
        }
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
        return true;
    }
} // namespace OpenBlood2
