#include "exercise/Cycling.h"

namespace wakeai {

    Cycling::Cycling() = default;

    double Cycling::kneeAngle(const PoseLandmarks& pose) const {
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
        return curAngle_;
    }

    float Cycling::extensionRatio(const PoseLandmarks& pose) const {
        // 骨骼长度比例：髋到踝 / (髋到膝 + 膝到踝)
        // 腿伸直 ≈ 1，完全折叠 ≈ 0（这就是文档"方案一"的骨骼比例）
        const Keypoint& h = pose[LeftHip];
        const Keypoint& k = pose[LeftKnee];
        const Keypoint& a = pose[LeftAnkle];

        float thigh = PoseLandmarks::dist(h, k);
        float shin = PoseLandmarks::dist(k, a);
        float full = thigh + shin;
        if (full < 1e-6f) return curRatio_;
        float ratio = PoseLandmarks::dist(h, a) / full;
        if (ratio > 1.0f) ratio = 1.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        return ratio;
    }

    void Cycling::update(const PoseLandmarks& pose) {
        curAngle_ = kneeAngle(pose);
        curRatio_ = extensionRatio(pose);

        switch (state_) {
        case ExerciseState::Ready:
            if (curAngle_ < kBendAngle_)
                state_ = ExerciseState::Down;
            break;
        case ExerciseState::Down:
            if (curAngle_ > kExtendAngle_) {
                count_++;
                state_ = ExerciseState::Up;
            }
            break;
        case ExerciseState::Up:
            state_ = (curAngle_ < kBendAngle_) ? ExerciseState::Down
                : ExerciseState::Ready;
            break;
        }
    }

    int Cycling::count() const { return count_; }
    double Cycling::angle() const { return curAngle_; }
    ExerciseState Cycling::state() const { return state_; }
    float Cycling::progress() const { return curRatio_; }  // 热力图用骨骼比例

    void Cycling::reset() {
        curAngle_ = 180.0;
        curRatio_ = 1.0f;
        count_ = 0;
        state_ = ExerciseState::Ready;
    }

    const char* Cycling::name() const { return "空中自行车"; }

} // namespace wakeai
