#pragma once

#include "exercise/ExerciseBase.h"

namespace wakeai {

    // 空中自行车 / 床上蹬腿
    // 状态机：腿伸直(Ready) → 屈膝(Down) → 蹬直(计数) → 循环
    // 同时用"骨骼长度比例"提供热力图进度（对应文档方案一）
    class Cycling : public ExerciseBase {
    public:
        Cycling();

        void update(const PoseLandmarks& pose) override;
        int count() const override;
        double angle() const override;
        ExerciseState state() const override;
        float progress() const override;
        void reset() override;
        const char* name() const override;

    private:
        double kneeAngle(const PoseLandmarks& pose) const;
        float extensionRatio(const PoseLandmarks& pose) const;  // 骨骼长度比例 0~1

        double curAngle_ = 180.0;
        float curRatio_ = 1.0f;
        int count_ = 0;
        ExerciseState state_ = ExerciseState::Ready;

        // 阈值，可调
        double kBendAngle_ = 100.0;   // 低于此角 → 屈膝
        double kExtendAngle_ = 150.0;   // 高于此角 → 蹬直

        // 防止噪声导致误计，在 Down 状态里要求「连续 N 帧都超过阈值」才计数
        int upFrames_ = 0;      // 连续超过阈值的帧数
        int kDebounce_ = 2;     // 需要连续2帧确认
    };

} // namespace wakeai
