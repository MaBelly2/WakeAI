#pragma once
#include "exercise/Squat.h"
#include "exercise/JumpingJack.h"
#include "exercise/Cycling.h"
namespace wakeai {
// Single parameter source for console and GUI; matches main at 5fa8c06.
inline void applyDefaultExerciseConfig(Squat& squat, JumpingJack& jack, Cycling& cycling) {
    squat.setThresholds(120.0, 155.0, 3);
    jack.setThresholds(0.45f, -0.25f, 1.25f, 0.75f, 3);
    cycling.setPartialBodyConfig(0.22f, 2, 5, 8);
    cycling.setCalibrationConfig(8, 0.10f, 0.30f, 0.28f);
    cycling.setSignalTriggerFloor(0.10f, 0.12f);
    cycling.setMinCountIntervalFrames(10);
    cycling.setCountMode(CyclingCountMode::EachPedal);
    cycling.setThresholds(115.0, 145.0, 2);
}
}
