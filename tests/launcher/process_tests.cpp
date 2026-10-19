#include "engine/resources.h"
#include "launcher/controller.h"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace OpenBlood2;
using namespace OpenBlood2::Launcher;
namespace fs = std::filesystem;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    std::string read(const fs::path& file)
    {
        std::ifstream input(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    void write(const fs::path& file, std::string_view text)
    {
        std::ofstream(file, std::ios::binary) << text;
    }
    void u32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
    }
    void archive(const fs::path& file, bool empty = false)
    {
        std::vector<unsigned char> bytes(empty ? 168 : 200, 0);
        bytes[0] = bytes[62] = bytes[124] = 13;
        bytes[1] = bytes[63] = bytes[125] = 10;
        bytes[126] = 26;
        u32(bytes, 127, 1);
        if (!empty)
        {
            // One opaque payload byte and a file record. Not a valid DAT payload.
            bytes[168] = 42;
            u32(bytes, 131, 169);
            u32(bytes, 135, 31);
            u32(bytes, 173, 168);
            u32(bytes, 177, 1);
            bytes[197] = 'x'; // 28-byte file record, basename x, two NUL terminators.
        }
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    bool protocol()
    {
        LauncherInfo info;
        std::string error;
        const auto valid = launcher_info();
        CHECK(parse_launcher_info(valid, info, error) && !info.revision.empty() && info.preferences == 2);
        const auto revision = info.revision;
        for (const auto& text : std::vector<std::string>{"", "protocol=99\n", valid + "extra=value\n",
                                                         valid + "game=OpenBlood2\n", valid.substr(0, valid.size() - 1),
                                                         valid + std::string(4096, 'x'), std::string("\0", 1) + valid})
        {
            CHECK(!parse_launcher_info(text, info, error) && info.revision == revision && !error.empty());
        }
        return true;
    }
    bool await(Probe& probe)
    {
        const auto end = SDL_GetTicksNS() + 6000000000ull;
        while (probe.status() == Probe::Status::Pending && SDL_GetTicksNS() < end)
        {
            probe.update();
            SDL_Delay(1);
        }
        return probe.status() != Probe::Status::Pending;
    }
    bool await(Controller& controller)
    {
        const auto end = SDL_GetTicksNS() + 6000000000ull;
        while (controller.busy() && SDL_GetTicksNS() < end)
        {
            controller.update();
            SDL_Delay(1);
        }
        if (!controller.error().empty())
            std::cerr << "Controller: " << controller.error() << '\n';
        return !controller.busy();
    }
    bool probes(const fs::path& root, const fs::path& fixture)
    {
        Probe probe;
        CHECK(probe.start(fixture));
        CHECK(!probe.start(fixture));
        CHECK(await(probe) && probe.status() == Probe::Status::Compatible);
        CHECK(!probe.start(root / "missing") && probe.status() == Probe::Status::Failed);
        for (auto mode : {"timeout", "overflow", "malformed", "failure"})
        {
            auto copy = root / (std::string("fixture_") + mode + fixture.extension().string());
            std::error_code ec;
            fs::copy_file(fixture, copy, fs::copy_options::overwrite_existing, ec);
            CHECK(!ec && probe.start(copy, std::string_view(mode) == "timeout" ? 120 : 5000));
            CHECK(await(probe) && probe.status() == Probe::Status::Failed && !probe.error().empty());
            if (std::string_view(mode) == "timeout")
                CHECK(probe.error().find("timed out") != std::string::npos);
            else
                CHECK(probe.error().find("timed out") == std::string::npos);
        }
        CHECK(probe.start(fixture));
        probe.cancel();
        CHECK(probe.status() == Probe::Status::Idle);
        return true;
    }
    bool content(const fs::path& root)
    {
        ContentSummary summary;
        std::string error;
        CHECK(!validate_content({Edition::Retail, root}, summary, error));
        archive(root / "BLOOD2.REZ");
        CHECK(validate_content({Edition::Retail, root}, summary, error));
        CHECK(summary.mounts == 1 && summary.resources == 1);
        if (!fs::exists(root / "blood2.rez"))
        {
            archive(root / "blood2.rez");
            CHECK(!validate_content({Edition::Retail, root}, summary, error));
            fs::remove(root / "blood2.rez");
        }
        fs::create_directory(root / "SOUND.REZ");
        CHECK(!validate_content({Edition::Retail, root}, summary, error));
        fs::remove(root / "SOUND.REZ");
        write(root / "BLOOD2NM.REZ", "Unknown expansion payload is excluded");
        CHECK(validate_content({Edition::Retail, root}, summary, error) && summary.mounts == 1);
        write(root / "BLOOD2P.REZ", "corrupt");
        CHECK(!validate_content({Edition::Retail, root}, summary, error) && summary.mounts == 1);
        archive(root / "B2Demo.rez");
        CHECK(validate_content({Edition::Demo, root}, summary, error) && summary.mounts == 1);
        archive(root / "B2Demo.rez", true);
        CHECK(!validate_content({Edition::Demo, root}, summary, error));
        ContentJob job;
        CHECK(job.start({Edition::Retail, root}));
        CHECK(!job.start({Edition::Retail, root}));
        job.cancel();
        CHECK(!job.pending());
        return true;
    }
    bool controller(const fs::path& root, const fs::path& fixture)
    {
        const auto data = root / fs::path(u8"Blood II é with spaces");
        const auto storage = root / "state";
        fs::create_directories(data);
        archive(data / "BLOOD2.REZ");
        archive(data / "B2Demo.rez");
        Controller app;
        const Installation retail{Edition::Retail, data};
        CHECK(app.prepare(retail, fixture));
        CHECK(!app.prepare(retail, fixture));
        CHECK(await(app) && app.playable());
        write(data / "BLOOD2.REZ", "changed after validation");
        CHECK(app.play(storage));
        CHECK(await(app) && app.status() == Controller::Status::Failed);
        CHECK(!fs::exists(storage / "runs"));
        archive(data / "BLOOD2.REZ");
        const auto previous = fs::current_path();
        fs::current_path(root);
        write(root / "blood2.local.ini", "invalid=working directory ini\n");
        SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "BLOOD2_DATA_DIR", path_text(root / "wrong-env").c_str(),
                                   true);
        CHECK(app.prepare(retail, fixture) && await(app) && app.playable());
        CHECK(app.play(storage) && !app.play(storage));
        CHECK(await(app) && app.status() == Controller::Status::Finished && app.exit_code() == 0);
        CHECK(read(app.logs() / "resolved.txt") == path_text(data) + "\n0\n");
        const auto args = read(app.logs() / "argv.txt");
        CHECK(args.find("--config\n") != args.npos && args.find("--log-dir\n") != args.npos);
        CHECK(args.find("--start-game") == args.npos && args.find("--view-world") == args.npos);
        CHECK(args.find("--demo") == args.npos);
        CHECK(read(app.logs().parent_path() / "bootstrap.ini") == "data_dir=\"" + path_text(data) + "\"\n");
        CHECK(app.prepare({Edition::Demo, data}, fixture) && await(app) && app.playable());
        CHECK(app.play(storage) && await(app) && app.status() == Controller::Status::Finished);
        CHECK(read(app.logs() / "resolved.txt") == path_text(data) + "\n1\n");
        CHECK(read(app.logs() / "argv.txt").find("--demo\n") != std::string::npos);
        write(data / "launcher-test-behavior", "failure");
        CHECK(app.play(storage) && await(app) && app.status() == Controller::Status::Failed && app.exit_code() == 17);
        CHECK(!app.logs().empty() && !app.error().empty());
        CHECK(!app.prepare(retail, root / "missing-game"));
        CHECK(app.prepare(retail, fixture) && await(app) && app.playable());
        write(root / "blocked-storage", "a file cannot be a launch directory");
        CHECK(app.play(root / "blocked-storage") && await(app) && app.status() == Controller::Status::Failed);
        auto changed = root / ("changeable-game" + fixture.extension().string());
        fs::copy_file(fixture, changed, fs::copy_options::overwrite_existing);
        CHECK(app.prepare(retail, changed) && await(app) && app.playable());
        fs::last_write_time(changed, fs::last_write_time(changed) + std::chrono::seconds(1));
        CHECK(!app.play(storage) && app.status() == Controller::Status::Failed);
        CHECK(app.prepare(retail, changed) && await(app) && app.playable());
        CHECK(app.play(storage));
        fs::last_write_time(changed, fs::last_write_time(changed) + std::chrono::seconds(1));
        CHECK(await(app) && app.status() == Controller::Status::Failed);
        fs::current_path(previous);
        return true;
    }
    bool detach_parent(const fs::path& root, const fs::path& fixture)
    {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        CHECK(SDL_Init(SDL_INIT_VIDEO));
        std::string error;
        {
            GameProcess child;
            CHECK(child.start(fixture, root / "bootstrap.ini", {Edition::Retail, root / "data"}, root / "parent-logs",
                              error));
            const auto end = SDL_GetTicksNS() + 1000000000ull;
            while (!fs::exists(root / "parent-logs" / "argv.txt") && SDL_GetTicksNS() < end)
                SDL_Delay(1);
            CHECK(fs::exists(root / "parent-logs" / "argv.txt"));
        }
        SDL_Quit();
        return true;
    }
    bool detach(const fs::path& root, const fs::path& fixture, const fs::path& self)
    {
        const auto data = root / "data", logs = root / "logs", config = root / "bootstrap.ini";
        fs::create_directories(data);
        write(config, "data_dir=\"" + path_text(data) + "\"\n");
        write(data / "launcher-test-behavior", "slow");
        std::string error;
        {
            GameProcess process;
            CHECK(process.start(fixture, config, {Edition::Retail, data}, logs, error));
            CHECK(!process.start(fixture, config, {Edition::Retail, data}, logs, error));
            const auto end = SDL_GetTicksNS() + 1000000000ull;
            while (!fs::exists(logs / "argv.txt") && SDL_GetTicksNS() < end)
                SDL_Delay(1);
            CHECK(fs::exists(logs / "argv.txt") && !fs::exists(logs / "finished.txt"));
        }
        const auto end = SDL_GetTicksNS() + 2000000000ull;
        while (!fs::exists(logs / "finished.txt") && SDL_GetTicksNS() < end)
            SDL_Delay(1);
        CHECK(fs::exists(logs / "finished.txt"));
        GameProcess missing;
        CHECK(!missing.start(root / "missing", config, {Edition::Retail, data}, logs, error));
        write(data / "launcher-test-behavior", "long");
        const auto self_text = path_text(self), root_text = path_text(root), fixture_text = path_text(fixture);
        const char* args[] = {self_text.c_str(), "detach_parent", root_text.c_str(), fixture_text.c_str(), nullptr};
        auto* parent = SDL_CreateProcess(args, false);
        CHECK(parent);
        int code = -1;
        CHECK(SDL_WaitProcess(parent, true, &code));
        SDL_DestroyProcess(parent);
        CHECK(code == 0 && !fs::exists(root / "parent-logs" / "finished.txt"));
        const auto child_end = SDL_GetTicksNS() + 2000000000ull;
        while (!fs::exists(root / "parent-logs" / "finished.txt") && SDL_GetTicksNS() < child_end)
            SDL_Delay(1);
        CHECK(fs::exists(root / "parent-logs" / "finished.txt"));
        return true;
    }
    bool private_content(const fs::path& data, const fs::path& game, bool demo)
    {
        Controller app;
        CHECK(app.prepare({demo ? Edition::Demo : Edition::Retail, data}, game));
        CHECK(await(app) && app.playable() && app.content().resources > 1000 && app.content().mounts >= 1);
        std::cout << "Private " << (demo ? "demo" : "retail") << ": " << app.content().resources << " resources, "
                  << app.content().mounts << " mounts; game query " << app.info().revision << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 4)
        return 2;
    const std::string_view mode(argv[1]);
    const auto root = fs::absolute(path_from_utf8(argv[2]));
    const auto fixture = fs::absolute(path_from_utf8(argv[3]));
    if (mode == "detach_parent")
        return detach_parent(root, fixture) ? 0 : 1;
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec)
        return 2;
    bool ok = false;
    if (mode == "protocol")
        ok = protocol();
    else if (mode == "prepare_ui")
    {
        fs::create_directories(root / "data");
        archive(root / "data" / "BLOOD2.REZ");
        ok = true;
    }
    else if (mode == "probe")
        ok = probes(root, fixture);
    else if (mode == "content")
        ok = content(root);
    else if (mode == "process")
        ok = controller(root, fixture);
    else if (mode == "detach")
        ok = detach(root, fixture, fs::absolute(path_from_utf8(argv[0])));
    else if (argc == 6 && mode == "private")
        ok = private_content(path_from_utf8(argv[4]), fixture, std::string_view(argv[5]) == "demo");
    SDL_Quit();
    return ok ? 0 : 1;
}
