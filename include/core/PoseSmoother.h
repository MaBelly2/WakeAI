#pragma once

#include <array>
#include "core/PoseData.h"

namespace wakeai {

// 对每个关键点做 EMA（指数移动平均）平滑。
// 注意：坐标可以沿用历史值做平滑，但 visibility 始终使用“当前帧”的值，
// 因此关键点丢失时动作状态机不会把旧坐标当成有效新数据。
class PoseSmoother {
public:
    explicit PoseSmoother(float alpha = 0.35f,
                          float updateVisibilityThreshold = 0.20f);

    PoseLandmarks update(const PoseLandmarks& input);

    void reset();

    void setAlpha(float alpha);
    float alpha() const { return alpha_; }

private:
    float alpha_ = 0.35f;
    float updateVisibilityThreshold_ = 0.20f;

    std::array<Keypoint, PoseLandmarks::kCount> history_{};
    std::array<bool, PoseLandmarks::kCount> initialized_{};
};

} // namespace wakeai
