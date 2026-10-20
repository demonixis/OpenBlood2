#pragma once
#include <chrono>
#include <array>
#include <iostream>
struct MCost {std::uint64_t calls=0; double ms=0;};
inline std::array<MCost,4> mcost;
struct MTimer{unsigned i;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();~MTimer(){auto&s=mcost[i];++s.calls;s.ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}};
