#pragma once

#include "core/PoseData.h"

namespace wakeai {

// 第一轮保持原有状态枚举，避免后续 UI / 其他分支大面积改接口。
// Ready：等待动作开始；Down：动作已进入“展开/下蹲/对侧相位”；Up 暂时保留。
enum class ExerciseState {
    Ready,
    Down,
    Up,
};

class ExerciseBase {
public:
    virtual ~ExerciseBase() = default;

    // 每帧调用一次。若关键点无效，派生类应停止推进状态机。
    virtual void update(const PoseLandmarks& pose) = 0;

    virtual int count() const = 0;

    // 为兼容现有工程保留 angle()。
    // Squat：膝角；JumpingJack：双臂抬升归一化值；Cycling：较弯曲腿的膝角。
    virtual double angle() const = 0;

    virtual ExerciseState state() const = 0;

    // 0~1，可供 UI / 热力显示使用。
    virtual float progress() const = 0;

    // 当前帧是否有足够可靠的关键点用于动作判断。
    virtual bool valid() const = 0;

    virtual void reset() = 0;
    virtual const char* name() const = 0;
};

} // namespace wakeai
