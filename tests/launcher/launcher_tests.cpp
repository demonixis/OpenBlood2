#include "launcher/dialog.h"
#include "launcher/state.h"
#include <fstream>
#include <iostream>
#include <thread>

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
    void write(const fs::path& file, std::string_view text)
    {
        std::ofstream stream(file, std::ios::binary);
        stream << text;
    }
    std::string read(const fs::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
    bool state_test(const fs::path& root)
    {
        const auto file = root / "launcher.cfg";
        State state;
        std::string error;
        CHECK(load_state(file, state, error));
        CHECK(state.installations.empty() && state.selected == -1);
        state.game = root / fs::path(u8"game executable é");
        state.selected = 0;
        state.installations = {{Edition::Retail, root / fs::path(u8"Blood II é=1")}, {Edition::Demo, root / "Demo"}};
        CHECK(save_state(file, state, error));
        State loaded;
        CHECK(load_state(file, loaded, error) && loaded == state);
        const auto saved = read(file);
        auto bad = state;
        bad.selected = 12;
        CHECK(!save_state(file, bad, error) && read(file) == saved);
        bad = state;
        bad.installations.push_back(state.installations.front());
        CHECK(!save_state(file, bad, error) && read(file) == saved);
        for (const std::string& text : std::vector<std::string>{
                 "version=9\ngame=\nselected=-1\n", saved + "unknown=1\n", saved + "selected=0\n",
                 "version=1\ngame=\nselected=0\n",
                 "version=1\ngame=\nselected=-1\ninstallation.1.edition=demo\ninstallation.1.path=/tmp\n",
                 saved + std::string(65537, 'x'), "version=1\ngame=relative\nselected=-1\n"})
        {
            write(file, text);
            loaded = state;
            CHECK(!load_state(file, loaded, error) && loaded == state);
            CHECK(!save_state(file, state, error) && read(file) == text);
        }
        write(file, saved);
        CHECK(save_state(file, state, error));
        CHECK(valid_text("Unicode \xc3\xa9", 40));
        CHECK(!valid_text("invalid \xc0\xaf", 40));
        CHECK(!valid_text(std::string("zero\0byte", 9), 40));
        CHECK(!valid_text("newline\n", 40));
        CHECK(!valid_text("surrogate \xed\xa0\x80", 40));
        for (auto& entry : fs::directory_iterator(root))
            CHECK(entry.path().filename().string().find(".tmp-") == std::string::npos);
        return true;
    }
    SDL_DialogFileCallback callback;
    void* token = nullptr;
    void show(SDL_DialogFileCallback cb, void* value, SDL_Window*, bool)
    {
        callback = cb;
        token = value;
    }
    bool dialogs()
    {
        Dialog dialog(show);
        CHECK(dialog.open(nullptr, false));
        CHECK(!dialog.open(nullptr, false));
        std::string source = "/directory with spaces \xc3\xa9";
        const char* paths[] = {source.c_str(), nullptr};
        std::thread worker([&] { callback(token, paths, -1); });
        worker.join();
        source.clear();
        DialogResult result;
        CHECK(dialog.poll(result) && result.path == "/directory with spaces \xc3\xa9" && result.error.empty());
        CHECK(!dialog.pending() && !dialog.poll(result));
        CHECK(dialog.open(nullptr, true));
        const char* cancel[] = {nullptr};
        callback(token, cancel, -1);
        CHECK(dialog.poll(result) && result.executable && result.path.empty() && result.error.empty());
        CHECK(dialog.open(nullptr, false));
        SDL_SetError("Synthetic dialog failure");
        callback(token, nullptr, -1);
        CHECK(dialog.poll(result) && result.error == "Synthetic dialog failure");
        {
            Dialog closing(show);
            CHECK(closing.open(nullptr, false));
        }
        callback(token, cancel, -1); // Completion after UI owner destruction remains safe.
        CHECK(dialog.open(nullptr, false));
        dialog.abandon();
        callback(token, cancel, -1);
        CHECK(!dialog.poll(result));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const auto root = fs::absolute(path_from_utf8(argv[2]));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec)
        return 2;
    const std::string_view test(argv[1]);
    const bool ok = test == "state" ? state_test(root) : test == "dialogs" && dialogs();
    return ok ? 0 : 1;
}
