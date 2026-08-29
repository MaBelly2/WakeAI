#include "exercise/Squat.h"

namespace wakeai {

    Squat::Squat() = default;

    double Squat::kneeAngle(const PoseLandmarks& pose) const {
        const Keypoint& lh = pose[LeftHip];
        const Keypoint& lk = pose[LeftKnee];
        const Keypoint& la = pose[LeftAnkle];
        const Keypoint& rh = pose[RightHip];
        const Keypoint& rk = pose[RightKnee];
        const Keypoint& ra = pose[RightAnkle];

        bool leftOk = lk.visible() && lh.visible() && la.visible();
        bool rightOk = rk.visible() && rh.visible() && ra.visible();

        if (leftOk && rightOk) {
            return (PoseLandmarks::angleDeg(lh, lk, la) +
                PoseLandmarks::angleDeg(rh, rk, ra)) / 2.0;
        }
        else if (leftOk) {
            return PoseLandmarks::angleDeg(lh, lk, la);
        }
        else if (rightOk) {
            return PoseLandmarks::angleDeg(rh, rk, ra);
        }
        return curAngle_;   // 两条腿都不可见，维持上一次角度
    }

    void Squat::update(const PoseLandmarks& pose) {
        curAngle_ = kneeAngle(pose);

        switch (state_) {
        case ExerciseState::Ready:
            if (curAngle_ < kDownAngle_)
                state_ = ExerciseState::Down;
            break;
        case ExerciseState::Down:
            if (curAngle_ > kUpAngle_) {
                upFrames_++;
                if (upFrames_ >= kDebounce_) {
                    count_++;
                    upFrames_ = 0;
                    state_ = ExerciseState::Ready;
                }
            }
            else {
                upFrames_ = 0;
            }
            break;
        }
    }

    int Squat::count() const { return count_; }
    double Squat::angle() const { return curAngle_; }
    ExerciseState Squat::state() const { return state_; }

    float Squat::progress() const {
        float p = static_cast<float>((curAngle_ - kDownAngle_) /
            (kUpAngle_ - kDownAngle_));
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        return p;
    }

    void Squat::reset() {
        curAngle_ = 180.0;
        count_ = 0;
        state_ = ExerciseState::Ready;
    }

    const char* Squat::name() const { return "深蹲"; }

} // namespace wakeai
