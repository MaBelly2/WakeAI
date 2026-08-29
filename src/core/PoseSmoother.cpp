#include "core/PoseSmoother.h"

#include <algorithm>

namespace wakeai {

PoseSmoother::PoseSmoother(float alpha, float updateVisibilityThreshold)
    : alpha_(std::clamp(alpha, 0.01f, 1.0f)),
      updateVisibilityThreshold_(std::clamp(updateVisibilityThreshold, 0.0f, 1.0f)) {
}

PoseLandmarks PoseSmoother::update(const PoseLandmarks& input) {
    PoseLandmarks output{};

    for (int i = 0; i < PoseLandmarks::kCount; ++i) {
        const Keypoint& cur = input[i];

        if (cur.visibility >= updateVisibilityThreshold_) {
            if (!initialized_[i]) {
                history_[i] = cur;
                initialized_[i] = true;
            }
            else {
                history_[i].x = alpha_ * cur.x + (1.0f - alpha_) * history_[i].x;
                history_[i].y = alpha_ * cur.y + (1.0f - alpha_) * history_[i].y;
                history_[i].z = alpha_ * cur.z + (1.0f - alpha_) * history_[i].z;
            }

            // visibility 不做 EMA：动作算法必须知道“这一帧”是否真的看到了该点。
            history_[i].visibility = cur.visibility;
            output[i] = history_[i];
        }
        else {
            // 坐标保留历史值，便于下一次重新出现时继续平滑；
            // 但输出可见度使用当前帧值，所以该点对动作算法是无效的。
            if (initialized_[i]) {
                output[i] = history_[i];
            }
            output[i].visibility = cur.visibility;
        }
    }

    return output;
}

void PoseSmoother::reset() {
    history_.fill(Keypoint{});
    initialized_.fill(false);
}

void PoseSmoother::setAlpha(float alpha) {
    alpha_ = std::clamp(alpha, 0.01f, 1.0f);
}

} // namespace wakeai
