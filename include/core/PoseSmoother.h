#pragma once

#include <array>
#include "core/PoseData.h"

namespace wakeai {

// 瀵规瘡涓叧閿偣鍋?EMA锛堟寚鏁扮Щ鍔ㄥ钩鍧囷級骞虫粦銆?
// 娉ㄦ剰锛氬潗鏍囧彲浠ユ部鐢ㄥ巻鍙插€煎仛骞虫粦锛屼絾 visibility 濮嬬粓浣跨敤鈥滃綋鍓嶅抚鈥濈殑鍊硷紝
// 鍥犳鍏抽敭鐐逛涪澶辨椂鍔ㄤ綔鐘舵€佹満涓嶄細鎶婃棫鍧愭爣褰撴垚鏈夋晥鏂版暟鎹€?
class PoseSmoother {
public:
    explicit PoseSmoother(float alpha = 0.35f,
                          float updateVisibilityThreshold = 0.20f);

    PoseLandmarks update(const PoseLandmarks& input);

    void reset();

private:
    float alpha_ = 0.35f;
    float updateVisibilityThreshold_ = 0.20f;

    std::array<Keypoint, PoseLandmarks::kCount> history_{};
    std::array<bool, PoseLandmarks::kCount> initialized_{};
};

} // namespace wakeai
