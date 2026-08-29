#pragma once

#include "exercise/ExerciseBase.h"

namespace wakeai {

    // 开合跳：手臂上举 + 双腿分开 = 展开；手臂放下 + 双腿并拢 = 收回
    class JumpingJack : public ExerciseBase {
    public:
        JumpingJack();

        void update(const PoseLandmarks& pose) override;
        int count() const override;
        double angle() const override;
        ExerciseState state() const override;
        float progress() const override;
        void reset() override;
        const char* name() const override;

    private:
        double wristLift(const PoseLandmarks& pose) const;      // 手腕相对肩的高度(像素)
        float legSpreadRatio(const PoseLandmarks& pose) const;  // 脚踝间距/肩宽

        double curLift_ = 0.0;
        float curSpread_ = 0.0f;
        int count_ = 0;
        ExerciseState state_ = ExerciseState::Ready;

        // 可调阈值
        double kLiftMargin_ = 30.0;    // 手臂举起的像素阈值
        float kSpreadOpen_ = 0.5f;     // 脚踝间距/肩宽 > 此值 → 腿分开
        float kSpreadClose_ = 0.25f;   // 脚踝间距/肩宽 < 此值 → 腿并拢

        // 防止噪声导致误计，在 Down 状态里要求「连续 N 帧都超过阈值」才计数
        int closeFrames_ = 0;      // 连续超过阈值的帧数
        int kDebounce_ = 2;     // 需要连续2帧确认
    };

} // namespace wakeai
