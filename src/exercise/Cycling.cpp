#include "exercise/Cycling.h"

#include <algorithm>

namespace wakeai {

Cycling::Cycling() = default;

bool Cycling::computeLegMetrics(const PoseLandmarks& pose,
                                int hipIndex,
                                int kneeIndex,
                                int ankleIndex,
                                double& angle,
                                float& extension) const {
    const Keypoint& hip = pose[hipIndex];
    const Keypoint& knee = pose[kneeIndex];
    const Keypoint& ankle = pose[ankleIndex];

    if (!(hip.visible(kVisibility_) &&
          knee.visible(kVisibility_) &&
          ankle.visible(kVisibility_))) {
        return false;
    }

    angle = PoseLandmarks::angleDeg(hip, knee, ankle);

    const float thigh = PoseLandmarks::dist(hip, knee);
    const float shin = PoseLandmarks::dist(knee, ankle);
    const float chain = thigh + shin;

    if (chain < 1e-6f) {
        return false;
    }

    // 这表示二维投影下的伸展程度：
    // 腿越直，髋-踝直线越接近“大腿长度 + 小腿长度”，比值越接近 1。
    // 它不是摄像头到腿的真实距离。
    extension = PoseLandmarks::dist(hip, ankle) / chain;
    extension = std::clamp(extension, 0.0f, 1.0f);

    return true;
}

CyclingPhase Cycling::classifyPhase() const {
    const bool phaseA =
        (leftAngle_ < kBendAngle_) &&
        (rightAngle_ > kExtendAngle_);

    const bool phaseB =
        (leftAngle_ > kExtendAngle_) &&
        (rightAngle_ < kBendAngle_);

    if (phaseA) {
        return CyclingPhase::LeftBentRightExtended;
    }

    if (phaseB) {
        return CyclingPhase::LeftExtendedRightBent;
    }

    return CyclingPhase::Unknown;
}

void Cycling::update(const PoseLandmarks& pose) {
    double leftA = -1.0;
    double rightA = -1.0;
    float leftE = 0.0f;
    float rightE = 0.0f;

    const bool leftOk = computeLegMetrics(
        pose, LeftHip, LeftKnee, LeftAnkle, leftA, leftE);
    const bool rightOk = computeLegMetrics(
        pose, RightHip, RightKnee, RightAnkle, rightA, rightE);

    valid_ = leftOk && rightOk;

    if (!valid_) {
        candidatePhase_ = CyclingPhase::Unknown;
        candidateFrames_ = 0;
        return;
    }

    leftAngle_ = leftA;
    rightAngle_ = rightA;
    leftExtension_ = leftE;
    rightExtension_ = rightE;

    // angle() 仅用于兼容旧接口，不参与计数。
    curAngle_ = std::min(leftAngle_, rightAngle_);

    // 用于基础热力进度；真正可视化时建议左右腿分别使用 left/rightExtension。
    curProgress_ = std::max(leftExtension_, rightExtension_);

    const CyclingPhase rawPhase = classifyPhase();

    // 两腿都处于中间过渡区时，不推进状态机。
    if (rawPhase == CyclingPhase::Unknown) {
        candidatePhase_ = CyclingPhase::Unknown;
        candidateFrames_ = 0;
        return;
    }

    if (rawPhase == candidatePhase_) {
        ++candidateFrames_;
    }
    else {
        candidatePhase_ = rawPhase;
        candidateFrames_ = 1;
    }

    if (candidateFrames_ < kConfirmFrames_) {
        return;
    }

    // 已稳定进入该相位。如果和当前稳定相位一样，不重复处理。
    if (phase_ == rawPhase) {
        return;
    }

    phase_ = rawPhase;
    candidateFrames_ = 0;

    // 第一次看到稳定相位，只作为“起点”，不计数。
    if (startPhase_ == CyclingPhase::Unknown) {
        startPhase_ = phase_;
        seenOpposite_ = false;
        state_ = ExerciseState::Ready;
        return;
    }

    if (phase_ != startPhase_) {
        // 已从起点相位走到了对侧相位，例如 A -> B。
        seenOpposite_ = true;
        state_ = ExerciseState::Down;
    }
    else if (seenOpposite_) {
        // 回到起点，例如 A -> B -> A：记为 1 个完整蹬腿循环。
        ++count_;
        seenOpposite_ = false;
        state_ = ExerciseState::Ready;
    }
}

int Cycling::count() const {
    return count_;
}

double Cycling::angle() const {
    return curAngle_;
}

ExerciseState Cycling::state() const {
    return state_;
}

float Cycling::progress() const {
    return valid_ ? std::clamp(curProgress_, 0.0f, 1.0f) : 0.0f;
}

bool Cycling::valid() const {
    return valid_;
}

void Cycling::reset() {
    leftAngle_ = -1.0;
    rightAngle_ = -1.0;
    leftExtension_ = 0.0f;
    rightExtension_ = 0.0f;

    curAngle_ = 180.0;
    curProgress_ = 0.0f;

    count_ = 0;
    state_ = ExerciseState::Ready;
    valid_ = false;

    phase_ = CyclingPhase::Unknown;
    candidatePhase_ = CyclingPhase::Unknown;
    startPhase_ = CyclingPhase::Unknown;

    candidateFrames_ = 0;
    seenOpposite_ = false;
}

void Cycling::setThresholds(double bendAngle,
                            double extendAngle,
                            int confirmFrames) {
    if (bendAngle >= extendAngle) {
        return;
    }

    kBendAngle_ = bendAngle;
    kExtendAngle_ = extendAngle;
    kConfirmFrames_ = std::max(1, confirmFrames);
}

const char* Cycling::name() const {
    return "空中自行车";
}

} // namespace wakeai
