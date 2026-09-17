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

            // visibility 涓嶅仛 EMA锛氬姩浣滅畻娉曞繀椤荤煡閬撯€滆繖涓€甯р€濇槸鍚︾湡鐨勭湅鍒颁簡璇ョ偣銆?
            history_[i].visibility = cur.visibility;
            output[i] = history_[i];
        }
        else {
            // 鍧愭爣淇濈暀鍘嗗彶鍊硷紝渚夸簬涓嬩竴娆￠噸鏂板嚭鐜版椂缁х画骞虫粦锛?
            // 浣嗚緭鍑哄彲瑙佸害浣跨敤褰撳墠甯у€硷紝鎵€浠ヨ鐐瑰鍔ㄤ綔绠楁硶鏄棤鏁堢殑銆?
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


} // namespace wakeai
