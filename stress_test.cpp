// stress_test.cpp - CPU stress test using all available hardware threads
#include "stress_test.h"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

StressTestState stressState       = StressTestState::IDLE;
float           stressProgress    = 0.f;
int             stressDurationSec = 30;

static std::atomic<bool>  s_stop{false};
static std::atomic<int>   s_threadsRunning{0};

// Each worker spins doing volatile arithmetic to prevent compiler optimisation
static void StressWorker(int durationMs) {
    s_threadsRunning++;
    auto start = std::chrono::steady_clock::now();
    volatile double x = 1.0;
    while (!s_stop.load(std::memory_order_relaxed)) {
        // Busy-loop: mix of multiply/divide to stress FPU and ALU
        for (int i = 0; i < 10000; i++) {
            x = x * 1.0000001 + 0.0000001;
            if (x > 1e10) x = 1.0;
        }
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed >= durationMs) break;
    }
    s_threadsRunning--;
}

static void StressCoordinator(int durationSec) {
    int cores = (int)std::thread::hardware_concurrency();
    if (cores < 1) cores = 1;

    int durationMs = durationSec * 1000;
    s_stop.store(false);

    // Launch one worker per logical core
    std::vector<std::thread> workers;
    workers.reserve(cores);
    for (int i = 0; i < cores; i++)
        workers.emplace_back(StressWorker, durationMs);

    // Update progress on the coordinator thread
    auto start = std::chrono::steady_clock::now();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        stressProgress = (float)elapsed / (float)durationMs;
        if (stressProgress >= 1.f || s_stop.load()) break;
    }

    s_stop.store(true);
    for (auto& t : workers) t.join();

    stressProgress = 1.f;
    stressState    = StressTestState::DONE;
}

void StartStressTest(int durationSec) {
    if (stressState == StressTestState::RUNNING) return;
    stressDurationSec = durationSec;
    stressProgress    = 0.f;
    stressState       = StressTestState::RUNNING;
    s_stop.store(false);
    std::thread(StressCoordinator, durationSec).detach();
}

void StopStressTest() {
    if (stressState != StressTestState::RUNNING) return;
    s_stop.store(true);
    stressState = StressTestState::DONE;
}
