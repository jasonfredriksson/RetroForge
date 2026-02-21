#pragma once

enum class StressTestState { IDLE, RUNNING, DONE };

extern StressTestState stressState;
extern float           stressProgress;   // 0..1
extern int             stressDurationSec; // configurable, default 30

void StartStressTest(int durationSec = 30);
void StopStressTest();
