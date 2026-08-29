#pragma once

#include "exercise/ExerciseBase.h"

namespace wakeai {

enum class CyclingPhase {
    Unknown,
    LeftBentRightExtended,
    LeftExtendedRightBent,
};

class Cycling : public ExerciseBase {
public:
    Cycling();

    void update(const PoseLandmarks& pose) override;
    int count() const override;
    double angle() const override;
    ExerciseState state() const override;
    float progress() const override;
    bool valid() const override;
    void reset() override;
    const char* name() const override;

    void setThresholds(double bendAngle,
                       double extendAngle,
                       int confirmFrames);

    double leftKneeAngle() const { return leftAngle_; }
    double rightKneeAngle() const { return rightAngle_; }

    // 0~1，表示二维几何上的“腿部伸展程度”，不是绝对深度。
    float leftExtension() const { return leftExtension_; }
    float rightExtension() const { return rightExtension_; }

    CyclingPhase phase() const { return phase_; }
    double bendThreshold() const { return kBendAngle_; }
    double extendThreshold() const { return kExtendAngle_; }
    int confirmFrames() const { return kConfirmFrames_; }

private:
    bool computeLegMetrics(const PoseLandmarks& pose,
                           int hipIndex,
                           int kneeIndex,
                           int ankleIndex,
                           double& angle,
                           float& extension) const;

    CyclingPhase classifyPhase() const;

    double leftAngle_ = -1.0;
    double rightAngle_ = -1.0;
    float leftExtension_ = 0.0f;
    float rightExtension_ = 0.0f;

    double curAngle_ = 180.0;
    float curProgress_ = 0.0f;

    int count_ = 0;
    ExerciseState state_ = ExerciseState::Ready;
    bool valid_ = false;

    double kBendAngle_ = 115.0;
    double kExtendAngle_ = 145.0;
    int kConfirmFrames_ = 3;
    float kVisibility_ = 0.45f;

    CyclingPhase phase_ = CyclingPhase::Unknown;
    CyclingPhase candidatePhase_ = CyclingPhase::Unknown;
    CyclingPhase startPhase_ = CyclingPhase::Unknown;

    int candidateFrames_ = 0;
    bool seenOpposite_ = false;
};

} // namespace wakeai
