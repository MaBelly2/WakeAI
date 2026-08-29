#include "exercise/JumpingJack.h"

#include <cmath>

namespace wakeai {

    JumpingJack::JumpingJack() = default;

    double JumpingJack::wristLift(const PoseLandmarks& pose) const {
        const Keypoint& ls = pose[LeftShoulder];
        const Keypoint& lw = pose[LeftWrist];
        const Keypoint& rs = pose[RightShoulder];
        const Keypoint& rw = pose[RightWrist];

        bool leftOk = ls.visible() && lw.visible();
        bool rightOk = rs.visible() && rw.visible();

        if (leftOk && rightOk) return ((ls.y - lw.y) + (rs.y - rw.y)) / 2.0;
        if (leftOk)            return ls.y - lw.y;
        if (rightOk)           return rs.y - rw.y;
        return curLift_;
    }

    float JumpingJack::legSpreadRatio(const PoseLandmarks& pose) const {
        const Keypoint& la = pose[LeftAnkle];
        const Keypoint& ra = pose[RightAnkle];
        const Keypoint& ls = pose[LeftShoulder];
        const Keypoint& rs = pose[RightShoulder];

        if (!(la.visible() && ra.visible() && ls.visible() && rs.visible()))
            return curSpread_;

        float ankleSpread = std::abs(la.x - ra.x);
        float shoulderW = PoseLandmarks::dist(ls, rs);
        if (shoulderW < 1e-3f) return curSpread_;
        return ankleSpread / shoulderW;
    }

    void JumpingJack::update(const PoseLandmarks& pose) {
        curLift_ = wristLift(pose);
        curSpread_ = legSpreadRatio(pose);

        // 手臂举过肩 且 双腿分开 → 展开；手臂放下 且 双腿并拢 → 收回
        bool open = (curLift_ > kLiftMargin_) && (curSpread_ > kSpreadOpen_);
        bool close = (curLift_ < -kLiftMargin_) && (curSpread_ < kSpreadClose_);

        switch (state_) {
        case ExerciseState::Ready:
            if (open) state_ = ExerciseState::Down;   // 进入展开
            break;
        case ExerciseState::Down:
            if (close) {
                closeFrames_++;
                if (closeFrames_ >= kDebounce_) {
                    count_++;
                    closeFrames_ = 0;
                    state_ = ExerciseState::Ready;
                }
            }
            else {
                closeFrames_ = 0;
            }
            break;
        }
    }

    int JumpingJack::count() const { return count_; }
    double JumpingJack::angle() const { return curLift_; }
    ExerciseState JumpingJack::state() const { return state_; }

    float JumpingJack::progress() const {
        // 用腿的开合程度做进度（0=并拢，1=完全分开）
        float p = (curSpread_ - kSpreadClose_) / (kSpreadOpen_ - kSpreadClose_);
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        return p;
    }

    void JumpingJack::reset() {
        curLift_ = 0.0;
        curSpread_ = 0.0f;
        closeFrames_ = 0;   // 新增
        count_ = 0;
        state_ = ExerciseState::Ready;
    }

    const char* JumpingJack::name() const { return "开合跳"; }

} // namespace wakeai
