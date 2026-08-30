#include "exercise/JumpingJack.h"

#include <algorithm>
#include <cmath>

namespace wakeai {

JumpingJack::JumpingJack() = default;

bool JumpingJack::computeMetrics(const PoseLandmarks& pose,
                                 float& leftArmLift,
                                 float& rightArmLift,
                                 float& legSpread) const {
    const Keypoint& ls = pose[LeftShoulder];
    const Keypoint& rs = pose[RightShoulder];
    const Keypoint& lw = pose[LeftWrist];
    const Keypoint& rw = pose[RightWrist];
    const Keypoint& la = pose[LeftAnkle];
    const Keypoint& ra = pose[RightAnkle];

    const bool allOk =
        ls.visible(kVisibility_) &&
        rs.visible(kVisibility_) &&
        lw.visible(kVisibility_) &&
        rw.visible(kVisibility_) &&
        la.visible(kVisibility_) &&
        ra.visible(kVisibility_);

    if (!allOk) {
        return false;
    }

    const float shoulderWidth = PoseLandmarks::dist(ls, rs);
    if (shoulderWidth < 10.0f) {
        return false;
    }

    // 图像 y 轴向下，所以“肩 y - 腕 y”越大，手举得越高。
    leftArmLift = (ls.y - lw.y) / shoulderWidth;
    rightArmLift = (rs.y - rw.y) / shoulderWidth;
    legSpread = std::abs(la.x - ra.x) / shoulderWidth;

    return true;
}

void JumpingJack::update(const PoseLandmarks& pose) {
    float leftLift = leftArmLift_;
    float rightLift = rightArmLift_;
    float spread = curSpread_;

    valid_ = computeMetrics(pose, leftLift, rightLift, spread);

    if (!valid_) {
        initialCloseFrames_ = 0;
        openFrames_ = 0;
        closeFrames_ = 0;
        return;
    }

    leftArmLift_ = leftLift;
    rightArmLift_ = rightLift;
    curSpread_ = spread;

    const bool open =
        (leftArmLift_ > kArmOpen_) &&
        (rightArmLift_ > kArmOpen_) &&
        (curSpread_ > kSpreadOpen_);

    const bool close =
        (leftArmLift_ < kArmClose_) &&
        (rightArmLift_ < kArmClose_) &&
        (curSpread_ < kSpreadClose_);

    // 启动保护：先看到稳定的并拢站姿。
    if (!armed_) {
        if (close) {
            ++initialCloseFrames_;
            if (initialCloseFrames_ >= kConfirmFrames_) {
                armed_ = true;
                initialCloseFrames_ = 0;
                state_ = ExerciseState::Ready;
            }
        }
        else {
            initialCloseFrames_ = 0;
        }
        return;
    }

    switch (state_) {
    case ExerciseState::Ready:
        if (open) {
            ++openFrames_;
            if (openFrames_ >= kConfirmFrames_) {
                state_ = ExerciseState::Down; // Down 在这里表示“完全展开”
                openFrames_ = 0;
                closeFrames_ = 0;
            }
        }
        else {
            openFrames_ = 0;
        }
        break;

    case ExerciseState::Down:
        if (close) {
            ++closeFrames_;
            if (closeFrames_ >= kConfirmFrames_) {
                ++count_;
                state_ = ExerciseState::Ready;
                closeFrames_ = 0;
                openFrames_ = 0;
            }
        }
        else {
            closeFrames_ = 0;
        }
        break;

    case ExerciseState::Up:
        state_ = ExerciseState::Ready;
        break;
    }
}

int JumpingJack::count() const {
    return count_;
}

double JumpingJack::angle() const {
    // 为兼容现有 ExerciseBase 接口，返回两臂抬升归一化值的平均值。
    // 注意它不是“角度”。
    return (static_cast<double>(leftArmLift_) +
            static_cast<double>(rightArmLift_)) / 2.0;
}

ExerciseState JumpingJack::state() const {
    return state_;
}

float JumpingJack::progress() const {
    if (!valid_) {
        return 0.0f;
    }

    const float armDen = kArmOpen_ - kArmClose_;
    const float legDen = kSpreadOpen_ - kSpreadClose_;
    if (armDen <= 1e-6f || legDen <= 1e-6f) {
        return 0.0f;
    }

    const float leftP = std::clamp((leftArmLift_ - kArmClose_) / armDen, 0.0f, 1.0f);
    const float rightP = std::clamp((rightArmLift_ - kArmClose_) / armDen, 0.0f, 1.0f);
    const float armP = std::min(leftP, rightP); // 两只手都要达标

    const float legP = std::clamp(
        (curSpread_ - kSpreadClose_) / legDen, 0.0f, 1.0f);

    return 0.5f * armP + 0.5f * legP;
}

bool JumpingJack::valid() const {
    return valid_;
}

void JumpingJack::reset() {
    leftArmLift_ = 0.0f;
    rightArmLift_ = 0.0f;
    curSpread_ = 0.0f;

    count_ = 0;
    state_ = ExerciseState::Ready;
    valid_ = false;
    armed_ = false;

    initialCloseFrames_ = 0;
    openFrames_ = 0;
    closeFrames_ = 0;
}

void JumpingJack::setThresholds(float armOpen,
                                float armClose,
                                float spreadOpen,
                                float spreadClose,
                                int confirmFrames) {
    if (armClose >= armOpen || spreadClose >= spreadOpen) {
        return;
    }

    kArmOpen_ = armOpen;
    kArmClose_ = armClose;
    kSpreadOpen_ = spreadOpen;
    kSpreadClose_ = spreadClose;
    kConfirmFrames_ = std::max(1, confirmFrames);
}

const char* JumpingJack::name() const {
    return "开合跳";
}

} // namespace wakeai
