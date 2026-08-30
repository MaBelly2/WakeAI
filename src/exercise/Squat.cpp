#include "exercise/Squat.h"

#include <algorithm>

namespace wakeai {

Squat::Squat() = default;

bool Squat::computeKneeAngle(const PoseLandmarks& pose,
                             double& selectedAngle,
                             double& leftAngle,
                             double& rightAngle) const {
    const Keypoint& lh = pose[LeftHip];
    const Keypoint& lk = pose[LeftKnee];
    const Keypoint& la = pose[LeftAnkle];

    const Keypoint& rh = pose[RightHip];
    const Keypoint& rk = pose[RightKnee];
    const Keypoint& ra = pose[RightAnkle];

    const bool leftOk =
        lh.visible(kVisibility_) && lk.visible(kVisibility_) && la.visible(kVisibility_);
    const bool rightOk =
        rh.visible(kVisibility_) && rk.visible(kVisibility_) && ra.visible(kVisibility_);

    leftAngle = leftOk ? PoseLandmarks::angleDeg(lh, lk, la) : -1.0;
    rightAngle = rightOk ? PoseLandmarks::angleDeg(rh, rk, ra) : -1.0;

    if (!leftOk && !rightOk) {
        return false;
    }

    if (leftOk && rightOk) {
        // 取「更弯」（角度更小）的一侧。
        // 2D 投影下，靠近镜头的一条腿会被测得更弯；取 min 最稳健，
        // 也能兼容 YOLO 偶尔把左右腿标反的情况。
        selectedAngle = std::min(leftAngle, rightAngle);
    }
    else {
        selectedAngle = leftOk ? leftAngle : rightAngle;
    }

    return true;
}

void Squat::update(const PoseLandmarks& pose) {
    double selected = curAngle_;
    double left = -1.0;
    double right = -1.0;

    valid_ = computeKneeAngle(pose, selected, left, right);

    leftAngle_ = left;
    rightAngle_ = right;

    if (!valid_) {
        // 关键点丢失时不沿用上一帧继续推进状态机。
        standFrames_ = 0;
        downFrames_ = 0;
        upFrames_ = 0;
        return;
    }

    curAngle_ = selected;

    // 启动保护：先稳定站直 kConfirmFrames_ 帧。
    if (!armed_) {
        if (curAngle_ > kUpAngle_) {
            ++standFrames_;
            if (standFrames_ >= kConfirmFrames_) {
                armed_ = true;
                standFrames_ = 0;
                state_ = ExerciseState::Ready;
            }
        }
        else {
            standFrames_ = 0;
        }
        return;
    }

    switch (state_) {
    case ExerciseState::Ready:
        if (curAngle_ < kDownAngle_) {
            ++downFrames_;
            if (downFrames_ >= kConfirmFrames_) {
                state_ = ExerciseState::Down;
                downFrames_ = 0;
                upFrames_ = 0;
            }
        }
        else {
            downFrames_ = 0;
        }
        break;

    case ExerciseState::Down:
        if (curAngle_ > kUpAngle_) {
            ++upFrames_;
            if (upFrames_ >= kConfirmFrames_) {
                ++count_;
                state_ = ExerciseState::Ready;
                upFrames_ = 0;
                downFrames_ = 0;
            }
        }
        else {
            upFrames_ = 0;
        }
        break;

    case ExerciseState::Up:
        // 第一轮未使用 Up，防御性回到 Ready。
        state_ = ExerciseState::Ready;
        break;
    }
}

int Squat::count() const {
    return count_;
}

double Squat::angle() const {
    return curAngle_;
}

ExerciseState Squat::state() const {
    return state_;
}

float Squat::progress() const {
    if (!valid_ || kUpAngle_ <= kDownAngle_) {
        return 0.0f;
    }

    // 站直约为 0，蹲得越深越接近 1。
    const double p = (kUpAngle_ - curAngle_) / (kUpAngle_ - kDownAngle_);
    return static_cast<float>(std::clamp(p, 0.0, 1.0));
}

bool Squat::valid() const {
    return valid_;
}

void Squat::reset() {
    curAngle_ = 180.0;
    leftAngle_ = -1.0;
    rightAngle_ = -1.0;
    count_ = 0;
    state_ = ExerciseState::Ready;
    valid_ = false;
    armed_ = false;
    standFrames_ = 0;
    downFrames_ = 0;
    upFrames_ = 0;
}

void Squat::setThresholds(double downAngle, double upAngle, int confirmFrames) {
    if (downAngle >= upAngle) {
        return;
    }

    kDownAngle_ = downAngle;
    kUpAngle_ = upAngle;
    kConfirmFrames_ = std::max(1, confirmFrames);
}

const char* Squat::name() const {
    return "深蹲";
}

} // namespace wakeai
