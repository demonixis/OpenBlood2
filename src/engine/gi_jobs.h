#pragma once
#include <atomic>
#include <cstdint>
#include <functional>

// The GI bake's job pool and per-ray randomness (gi_contracts.md, lot G5). Results never depend
// on the thread count: jobs write their own slots and every ray draws from a seed derived from
// its own indices, never from a shared generator.
namespace OpenBlood2
{
    struct GiJobRun
    {
        unsigned threads = 0;     // Threads that ran jobs, the caller included.
        std::uint64_t done = 0;   // Jobs completed.
        bool cancelled = false;   // The flag was seen before every job finished.
    };
    // Runs job(index, thread) for every index in [0, count) on up to `threads` threads (the caller
    // is thread 0; a thread that fails to start leaves its share to the others). `cancel` is read
    // before each job; once set, no further job starts. Returns when every started job finished.
    GiJobRun run_gi_jobs(std::uint64_t count, unsigned threads, const std::atomic<bool>* cancel,
                         const std::function<void(std::uint64_t index, unsigned thread)>& job);
    // Default --gi-jobs: the logical core count.
    unsigned gi_default_jobs();

    // splitmix64 over a stream identity: the same (a, b, c) always gives the same sequence.
    struct GiRandom
    {
        std::uint64_t state;
        GiRandom(std::uint64_t a, std::uint64_t b, std::uint64_t c = 0);
        std::uint64_t next();
        float uniform(); // [0, 1), 24 bits.
    };
} // namespace OpenBlood2
