#pragma once

#include "exercise/ExerciseBase.h"

namespace wakeai {

    // 开合跳：通过手腕相对肩膀的高度判断"双手上举/放下"
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
        // 手腕高于肩膀的像素距离（正=举过肩，负=在肩下方）
        double wristLift(const PoseLandmarks& pose) const;

        double curLift_ = 0.0;
        int count_ = 0;
        ExerciseState state_ = ExerciseState::Ready;

        double kLiftMargin_ = 30.0;   // 像素阈值，超过才算"举起"
    };

} // namespace wakeai
