#pragma once

#include "core/PoseData.h"

namespace wakeai {

    // 动作阶段（状态机）：READY → DOWN → UP → 计数，循环
    enum class ExerciseState {
        Ready,  // 准备：回到起始姿势，等待下一次动作
        Down,   // 第一阶段：下蹲 / 蹬腿屈膝 / 双手展开
        Up,     // 第二阶段：起身 / 蹬腿伸直 / 双手收回
    };

    // 所有动作类的统一接口
    class ExerciseBase {
    public:
        virtual ~ExerciseBase() = default;

        // 每帧调用一次：传入最新关键点，内部推进状态机
        virtual void update(const PoseLandmarks& pose) = 0;

        // 已完成次数
        virtual int count() const = 0;

        // 当前关键角度（度），供 UI 显示
        virtual double angle() const = 0;

        // 当前状态
        virtual ExerciseState state() const = 0;

        // 当前进度 0~1，用于热力图/进度条
        virtual float progress() const = 0;

        // 复位：计数清零，回到 Ready
        virtual void reset() = 0;

        // 动作名称（中文），供 UI 显示
        virtual const char* name() const = 0;
    };

} // namespace wakeai
