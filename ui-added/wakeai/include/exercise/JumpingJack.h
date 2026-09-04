#pragma once

#include "exercise/ExerciseBase.h"

namespace wakeai {

class JumpingJack : public ExerciseBase {
public:
    JumpingJack();

    void update(const PoseLandmarks& pose) override;
    int count() const override;
    double angle() const override;
    ExerciseState state() const override;
    float progress() const override;
    bool valid() const override;
    void reset() override;
    const char* name() const override;

    // armOpen / armClose：手腕相对肩部的“肩宽归一化高度”。
    // spreadOpen / spreadClose：脚踝间距 / 肩宽。
    void setThresholds(float armOpen,
                       float armClose,
                       float spreadOpen,
                       float spreadClose,
                       int confirmFrames);

    float leftArmLift() const { return leftArmLift_; }
    float rightArmLift() const { return rightArmLift_; }
    float legSpread() const { return curSpread_; }
    bool armed() const { return armed_; }
    float armOpenThreshold() const { return kArmOpen_; }
    float armCloseThreshold() const { return kArmClose_; }
    float spreadOpenThreshold() const { return kSpreadOpen_; }
    float spreadCloseThreshold() const { return kSpreadClose_; }
    int confirmFrames() const { return kConfirmFrames_; }

private:
    bool computeMetrics(const PoseLandmarks& pose,
                        float& leftArmLift,
                        float& rightArmLift,
                        float& legSpread) const;

    float leftArmLift_ = 0.0f;
    float rightArmLift_ = 0.0f;
    float curSpread_ = 0.0f;

    int count_ = 0;
    ExerciseState state_ = ExerciseState::Ready;
    bool valid_ = false;
    bool armed_ = false;

    // 推荐第一轮初值。
    float kArmOpen_ = 0.45f;
    float kArmClose_ = -0.25f;
    float kSpreadOpen_ = 1.25f;
    float kSpreadClose_ = 0.75f;
    int kConfirmFrames_ = 3;

    int initialCloseFrames_ = 0;
    int openFrames_ = 0;
    int closeFrames_ = 0;

    float kVisibility_ = 0.45f;
};

} // namespace wakeai
