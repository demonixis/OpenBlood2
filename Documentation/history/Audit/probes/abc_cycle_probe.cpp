// Creates only a synthetic ABC section chain; parses it with unchanged code.
#include "abc_parser.h"
#include <cstdint>
#include <fstream>
#include <cstdio>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    // Empty unknown section at 0 -> section at 6 -> itself at 6.
    const uint8_t data[] = {0, 0, 6, 0, 0, 0, 0, 0, 6, 0, 0, 0};
    std::ofstream file(argv[1], std::ios::binary);
    file.write(reinterpret_cast<const char*>(data), sizeof(data));
    file.close();
    std::puts("Parsing a synthetic ABC whose next-section offset points to itself.");
    std::fflush(stdout);
    LithTech::ABCParser parser(argv[1]);
    return parser.Parse() ? 0 : 1;
}
