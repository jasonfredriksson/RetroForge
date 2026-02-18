#pragma once
#include <string>

enum class SpeedTestState {
    IDLE,
    RUNNING,
    DONE,
    FAILED
};

struct SpeedTestResult {
    float downloadMbps = 0.f;
    float uploadMbps   = 0.f;   // estimated from ping latency
    float pingMs       = 0.f;
    std::string timestamp;
    std::string server;
};

extern SpeedTestState     speedTestState;
extern SpeedTestResult    speedTestResult;
extern SpeedTestResult    speedTestLastSaved;
extern bool               speedTestHasSaved;
extern float              speedTestProgress;  // 0..1 during download

void StartSpeedTest();
void SaveSpeedTestResult();
