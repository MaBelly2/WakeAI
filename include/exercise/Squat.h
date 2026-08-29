#pragma once

#include "exercise/ExerciseBase.h"

namespace wakeai {

    // 深蹲：通过膝关节角度(髋-膝-踝)判断下蹲/起身
    class Squat : public ExerciseBase {
    public:
        Squat();

        void update(const PoseLandmarks& pose) override;
        int count() const override;
        double angle() const override;
        ExerciseState state() const override;
        float progress() const override;
        void reset() override;
        const char* name() const override;

    private:
        // 当前膝关节角度（度），自动处理左右腿可见度
        double kneeAngle(const PoseLandmarks& pose) const;

        double curAngle_ = 180.0;
        int count_ = 0;
        ExerciseState state_ = ExerciseState::Ready;

        // 阈值（度），可调
        double kDownAngle_ = 90.0;    // 低于此角 → 判定"蹲下"
        double kUpAngle_ = 150.0;   // 高于此角 → 判定"站起"

        // 防止噪声导致误计，在 Down 状态里要求「连续 N 帧都超过阈值」才计数
        int upFrames_ = 0;      // 连续超过阈值的帧数
        int kDebounce_ = 2;     // 需要连续2帧确认

    };

} // namespace wakeai
