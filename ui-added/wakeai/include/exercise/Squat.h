#pragma once

#include "exercise/ExerciseBase.h"

namespace wakeai {

class Squat : public ExerciseBase {
public:
    Squat();

    void update(const PoseLandmarks& pose) override;
    int count() const override;
    double angle() const override;
    ExerciseState state() const override;
    float progress() const override;
    bool valid() const override;
    void reset() override;
    const char* name() const override;

    // 第一轮调参入口：建议从 120 / 155 / 3 开始。
    void setThresholds(double downAngle, double upAngle, int confirmFrames);

    double leftKneeAngle() const { return leftAngle_; }
    double rightKneeAngle() const { return rightAngle_; }
    bool armed() const { return armed_; }
    double downThreshold() const { return kDownAngle_; }
    double upThreshold() const { return kUpAngle_; }
    int confirmFrames() const { return kConfirmFrames_; }

private:
    bool computeKneeAngle(const PoseLandmarks& pose,
                          double& selectedAngle,
                          double& leftAngle,
                          double& rightAngle) const;

    double curAngle_ = 180.0;
    double leftAngle_ = -1.0;
    double rightAngle_ = -1.0;

    int count_ = 0;
    ExerciseState state_ = ExerciseState::Ready;
    bool valid_ = false;

    // 先确认“站直”，再允许进入一次深蹲，避免程序刚启动就在蹲姿时误计数。
    bool armed_ = false;

    double kDownAngle_ = 140.0;
    double kUpAngle_ = 155.0;
    int kConfirmFrames_ = 2;

    int standFrames_ = 0;
    int downFrames_ = 0;
    int upFrames_ = 0;

    float kVisibility_ = 0.30f;
};

} // namespace wakeai
