#pragma once
#include <chrono>
#include <array>
#include <iostream>
struct B2Cost { unsigned long long calls=0; double ms=0; };
inline std::array<B2Cost,7> b2_costs;
struct B2CostTimer { unsigned index; std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now(); ~B2CostTimer(){ auto& s=b2_costs[index]; ++s.calls; s.ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); } };
