#include "exercise/JumpingJack.h"

namespace wakeai {

    JumpingJack::JumpingJack() = default;

    double JumpingJack::wristLift(const PoseLandmarks& pose) const {
        // 手腕相对肩膀的高度：肩.y - 腕.y（图像 y 向下，正=举过肩）
        const Keypoint& ls = pose[LeftShoulder];
        const Keypoint& lw = pose[LeftWrist];
        const Keypoint& rs = pose[RightShoulder];
        const Keypoint& rw = pose[RightWrist];

        bool leftOk = ls.visible() && lw.visible();
        bool rightOk = rs.visible() && rw.visible();

        if (leftOk && rightOk) {
            return ((ls.y - lw.y) + (rs.y - rw.y)) / 2.0;
        }
        else if (leftOk) {
            return ls.y - lw.y;
        }
        else if (rightOk) {
            return rs.y - rw.y;
        }
        return curLift_;
    }

    void JumpingJack::update(const PoseLandmarks& pose) {
        curLift_ = wristLift(pose);

        switch (state_) {
        case ExerciseState::Ready:
            if (curLift_ > kLiftMargin_)      // 双手举起 → 进入展开
                state_ = ExerciseState::Down;
            break;
        case ExerciseState::Down:
            if (curLift_ < -kLiftMargin_) {   // 双手放下 → 完成一次
                count_++;
                state_ = ExerciseState::Up;
            }
            break;
        case ExerciseState::Up:
            state_ = (curLift_ > kLiftMargin_) ? ExerciseState::Down
                : ExerciseState::Ready;
            break;
        }
    }

    int JumpingJack::count() const { return count_; }
    double JumpingJack::angle() const { return curLift_; }  // 开合跳无角度，返回高度差
    ExerciseState JumpingJack::state() const { return state_; }

    float JumpingJack::progress() const {
        float p = static_cast<float>((curLift_ + kLiftMargin_) /
            (2.0 * kLiftMargin_));
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        return p;
    }

    void JumpingJack::reset() {
        curLift_ = 0.0;
        count_ = 0;
        state_ = ExerciseState::Ready;
    }

    const char* JumpingJack::name() const { return "开合跳"; }

} // namespace wakeai
