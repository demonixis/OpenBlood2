#include "engine/gi_jobs.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

namespace OpenBlood2
{
    namespace
    {
        struct Shared
        {
            std::uint64_t count = 0;
            const std::atomic<bool>* cancel = nullptr;
            const std::function<void(std::uint64_t, unsigned)>* job = nullptr;
            std::atomic<std::uint64_t> next{0}, done{0};
        };
        struct Worker
        {
            Shared* shared;
            unsigned thread;
        };
        void work(Shared& shared, unsigned thread)
        {
            for (;;)
            {
                if (shared.cancel && shared.cancel->load(std::memory_order_relaxed))
                {
                    return;
                }
                const auto index = shared.next.fetch_add(1, std::memory_order_relaxed);
                if (index >= shared.count)
                    return;
                (*shared.job)(index, thread);
                shared.done.fetch_add(1, std::memory_order_relaxed);
            }
        }
        int SDLCALL entry(void* data)
        {
            auto* worker = static_cast<Worker*>(data);
            work(*worker->shared, worker->thread);
            return 0;
        }
        std::uint64_t mix(std::uint64_t z)
        {
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            return z ^ (z >> 31);
        }
    } // namespace

    GiJobRun run_gi_jobs(std::uint64_t count, unsigned threads, const std::atomic<bool>* cancel,
                         const std::function<void(std::uint64_t, unsigned)>& job)
    {
        Shared shared;
        shared.count = count;
        shared.cancel = cancel;
        shared.job = &job;
        threads = static_cast<unsigned>(std::clamp<std::uint64_t>(std::min<std::uint64_t>(threads, count), 1, 256));
        std::vector<Worker> workers(threads);
        std::vector<SDL_Thread*> handles;
        for (unsigned i = 1; i < threads; ++i)
        {
            workers[i] = {&shared, i};
            if (auto* handle = SDL_CreateThread(entry, "gi-bake", &workers[i]))
                handles.push_back(handle);
        }
        work(shared, 0);
        for (auto* handle : handles)
            SDL_WaitThread(handle, nullptr);
        GiJobRun run;
        run.threads = static_cast<unsigned>(handles.size()) + 1;
        run.done = shared.done.load();
        run.cancelled = run.done < count;
        return run;
    }
    unsigned gi_default_jobs()
    {
        return static_cast<unsigned>(std::clamp(SDL_GetNumLogicalCPUCores(), 1, 256));
    }

    GiRandom::GiRandom(std::uint64_t a, std::uint64_t b, std::uint64_t c)
        : state(mix(mix(mix(a + 0x9e3779b97f4a7c15ull) ^ b) ^ c))
    {
    }
    std::uint64_t GiRandom::next()
    {
        state += 0x9e3779b97f4a7c15ull;
        return mix(state);
    }
    float GiRandom::uniform()
    {
        return static_cast<float>(next() >> 40) * (1.f / 16777216.f);
    }
} // namespace OpenBlood2
