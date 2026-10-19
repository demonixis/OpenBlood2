// A subprocess argument recorder, not a playable game or a commercial resource.
#include "engine/config.h"
#include "startup/protocol.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <fstream>
#include <iostream>

using namespace OpenBlood2;
namespace fs = std::filesystem;
int main(int argc, char** argv)
{
    const auto mode = path_text(path_from_utf8(argv[0]).filename());
    if (argc == 2 && std::string_view(argv[1]) == "--launcher-info")
    {
        if (mode.find("timeout") != mode.npos)
            SDL_Delay(2000);
        if (mode.find("overflow") != mode.npos)
        {
            std::cout << std::string(65536, 'x') << std::flush;
            return 0;
        }
        if (mode.find("malformed") != mode.npos)
        {
            std::cout << "protocol=999\n";
            return 0;
        }
        if (mode.find("failure") != mode.npos)
        {
            std::cerr << "Synthetic query failure\n";
            return 7;
        }
        std::cout << launcher_info();
        return 0;
    }
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);
    Options options;
    Config config;
    std::string error;
    const char* env = std::getenv("BLOOD2_DATA_DIR");
    if (!parse_options(args, options, error) || !load_config(options, env ? env : "", config, error))
    {
        std::cerr << error << '\n';
        return 12;
    }
    std::error_code ec;
    fs::create_directories(options.log_dir, ec);
    if (ec)
        return 13;
    {
        std::ofstream out(options.log_dir / "argv.txt");
        for (int i = 1; i < argc; ++i)
            out << argv[i] << '\n';
        std::ofstream(options.log_dir / "resolved.txt") << path_text(config.data_dir) << '\n' << config.demo << '\n';
    }
    std::string behavior;
    std::ifstream(config.data_dir / "launcher-test-behavior") >> behavior;
    if (behavior == "long")
        SDL_Delay(1000);
    else if (behavior == "slow")
        SDL_Delay(200);
    else
        SDL_Delay(50);
    std::ofstream(options.log_dir / "finished.txt") << "finished\n";
    return behavior == "failure" ? 17 : 0;
}
