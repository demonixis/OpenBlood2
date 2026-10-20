#include <array>
#include <cstdio>
struct Controls {unsigned volume=100,expression=127,pan=64,bend=8192;};
struct Channel {unsigned msb=0,lsb=0,program=0;bool percussion=false;Controls controls;};
struct Receiver {std::array<Channel,16> channels=[] {std::array<Channel,16> values{};values.fill(Channel{});values[9].percussion=true;return values;}();};
int main(){Receiver r;int failed=0;for(unsigned i=0;i<16;++i){auto c=r.channels[i];std::printf("%u: %u %u %u %u\n",i,c.controls.volume,c.controls.expression,c.controls.pan,c.controls.bend);failed+=c.controls.expression!=127;}return failed?1:0;}
