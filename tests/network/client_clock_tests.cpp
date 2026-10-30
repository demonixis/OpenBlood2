#include "client/network_clock.h"
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << '\n';                                                                  \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    NetworkClock clock;
    double time = -1;
    CHECK(!clock.sample(0, time) && time == -1);
    CHECK(clock.observe(10, 0, .025) && clock.sample(0, time) && time == 10.025);
    CHECK(clock.sample(.1, time) && std::abs(time - 10.125) < 1e-9);
    CHECK(clock.observe(10, .1, .1)); // Duplicate fragments cannot reset the anchor.
    CHECK(clock.sample(.2, time) && std::abs(time - 10.225) < 1e-9);
    CHECK(clock.sample(2, time) && time == 10.25); // Bounded during silence.
    CHECK(clock.observe(9, 2, .01) && clock.sample(2, time) && time == 10.25);
    CHECK(clock.observe(10.2, 2, 0) && clock.sample(2, time) && time == 10.25); // No backwards correction.
    CHECK(clock.sample(2.1, time) && std::abs(time - 10.3) < 1e-9);
    const auto previous = time;
    CHECK(!clock.sample(2, time) && time == previous);
    CHECK(!clock.observe(-1, 2.1, 0));
    CHECK(!clock.observe(11, 2.1, 61));
    CHECK(!clock.observe(11, 2, 0));
    CHECK(!clock.sample(std::numeric_limits<double>::quiet_NaN(), time) && time == previous);
    CHECK(clock.observe(11, 2.1, .075) && clock.sample(2.1, time) && time == 11.075);
    // Alternating arrival jitter and RTT changes never make the displayed clock run backwards.
    for (unsigned i = 1; i <= 1000; ++i)
    {
        const auto before = time;
        const double arrival = 2.1 + double(i) / 60;
        if (i % 3 == 0)
            CHECK(clock.observe(11 + double(i) / 60 + (i % 2 ? .02 : 0), arrival, i % 2 ? .025 : .075));
        CHECK(clock.sample(arrival, time) && time >= before);
    }
    std::cout << "Client clock: repeated/reordered stamps, monotonic correction, bounded silence and jitter passed\n";
}
