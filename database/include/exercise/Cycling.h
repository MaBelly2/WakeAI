#pragma once

#include "exercise/ExerciseBase.h"

#include <limits>

namespace wakeai {

// 对于床上蹬腿模式，A/B 只表示左右腿在画面中的“相对运动相位”。
// 不把它解释为绝对三维远近。
enum class CyclingPhase {
    Unknown,
    LeftDominant,
    RightDominant,
};

// 局部腿部模式会自动在两种二维特征中选择变化更明显的一种：
// 1) ShinScale: 左右小腿二维投影长度之比
// 2) KneeVertical: 左右膝盖在图像 y 方向的相对位移（用平均小腿长度归一化）
enum class CyclingSignalSource {
    Unknown,
    ShinScale,
    KneeVertical,
};

class Cycling : public ExerciseBase {
public:
    Cycling();

    void update(const PoseLandmarks& pose) override;
    int count() const override;
    double angle() const override;
    ExerciseState state() const override;
    float progress() const override;
    bool valid() const override;
    void reset() override;
    const char* name() const override;

    // 保留第一轮接口，避免其他代码编译失败。
    // bend/extend 现在只用于“髋点可见时”的辅助膝角调试，不再作为主计数条件。
    void setThresholds(double bendAngle,
                       double extendAngle,
                       int confirmFrames);

    // 床上局部腿部模式的主要参数。
    // visibility: 膝、踝关键点最低置信度；建议 0.18~0.30。
    // confirmFrames: A/B 相位连续确认帧数；固定视频先用 2。
    // maxMissingFrames: 最多允许多少帧关键点短暂丢失而不清空候选相位。
    // minHalfCycleFrames: A->B 或 B->A 至少间隔多少帧，防止抖动反复切相位。
    void setPartialBodyConfig(float visibility,
                              int confirmFrames,
                              int maxMissingFrames,
                              int minHalfCycleFrames);

    // 自动标定：程序先观察左右腿的相对二维运动范围，再自动选特征和阈值。
    // minCalibrationFrames: 至少积累多少个有效帧。
    // minScaleRange: log(左小腿长度/右小腿长度) 至少变化多少才认为可用。
    // minKneeYRange: 归一化左右膝 y 差至少变化多少才认为可用。
    // triggerFraction: 相位阈值 = 标定动态范围 * 该比例。
    void setCalibrationConfig(int minCalibrationFrames,
                              float minScaleRange,
                              float minKneeYRange,
                              float triggerFraction);

    // ===== 调试信息 =====
    double leftKneeAngle() const { return leftAngle_; }
    double rightKneeAngle() const { return rightAngle_; }

    float leftShinLength() const { return leftShin_; }
    float rightShinLength() const { return rightShin_; }

    // 0~1，仅用于颜色热力显示：数值越大表示该腿在当前二维特征中更“占优势”。
    // 不是摄像头到腿的真实三维距离。
    float leftExtension() const { return leftHeat_; }
    float rightExtension() const { return rightHeat_; }

    CyclingPhase phase() const { return phase_; }
    CyclingSignalSource signalSource() const { return signalSource_; }

    bool calibrated() const { return calibrated_; }
    int calibrationFrames() const { return validSignalFrames_; }

    float signal() const { return selectedSignal_; }
    float signalBaseline() const { return signalBaseline_; }
    float signalTrigger() const { return signalTrigger_; }

    float rawScaleSignal() const { return rawScaleSignal_; }
    float rawKneeYSignal() const { return rawKneeYSignal_; }

    float leftHipVisibility() const { return leftHipVis_; }
    float rightHipVisibility() const { return rightHipVis_; }
    float leftKneeVisibility() const { return leftKneeVis_; }
    float rightKneeVisibility() const { return rightKneeVis_; }
    float leftAnkleVisibility() const { return leftAnkleVis_; }
    float rightAnkleVisibility() const { return rightAnkleVis_; }

    float visibilityThreshold() const { return kVisibility_; }
    int confirmFrames() const { return kConfirmFrames_; }

    // 兼容旧调试界面。
    double bendThreshold() const { return kBendAngle_; }
    double extendThreshold() const { return kExtendAngle_; }

private:
    bool computeLowerLegMetrics(const PoseLandmarks& pose,
                                int kneeIndex,
                                int ankleIndex,
                                float& shinLength) const;

    bool computeOptionalKneeAngle(const PoseLandmarks& pose,
                                  int hipIndex,
                                  int kneeIndex,
                                  int ankleIndex,
                                  double& angle) const;

    void updateCalibration(float scaleSignal, float kneeYSignal);
    CyclingPhase classifySelectedSignal() const;
    void updateHeatValues();

    // ===== 当前几何量 =====
    double leftAngle_ = -1.0;
    double rightAngle_ = -1.0;

    float leftShin_ = 0.0f;
    float rightShin_ = 0.0f;

    float leftHeat_ = 0.5f;
    float rightHeat_ = 0.5f;

    float rawScaleSignal_ = 0.0f;
    float rawKneeYSignal_ = 0.0f;
    float filteredScaleSignal_ = 0.0f;
    float filteredKneeYSignal_ = 0.0f;
    bool signalFilterInitialized_ = false;

    // ===== 可见度调试 =====
    float leftHipVis_ = 0.0f;
    float rightHipVis_ = 0.0f;
    float leftKneeVis_ = 0.0f;
    float rightKneeVis_ = 0.0f;
    float leftAnkleVis_ = 0.0f;
    float rightAnkleVis_ = 0.0f;

    // ===== 自动标定 =====
    bool calibrated_ = false;
    int validSignalFrames_ = 0;

    float scaleMin_ = std::numeric_limits<float>::infinity();
    float scaleMax_ = -std::numeric_limits<float>::infinity();
    float kneeYMin_ = std::numeric_limits<float>::infinity();
    float kneeYMax_ = -std::numeric_limits<float>::infinity();

    CyclingSignalSource signalSource_ = CyclingSignalSource::Unknown;
    float selectedSignal_ = 0.0f;
    float signalBaseline_ = 0.0f;
    float signalTrigger_ = 0.0f;

    // ===== 状态机 =====
    int count_ = 0;
    ExerciseState state_ = ExerciseState::Ready;
    bool valid_ = false;

    CyclingPhase phase_ = CyclingPhase::Unknown;
    CyclingPhase candidatePhase_ = CyclingPhase::Unknown;
    CyclingPhase startPhase_ = CyclingPhase::Unknown;

    int candidateFrames_ = 0;
    int missingFrames_ = 0;
    int frameCounter_ = 0;
    int lastStablePhaseFrame_ = -100000;
    int oppositePhaseFrame_ = -100000;
    bool seenOpposite_ = false;

    // ===== 参数 =====
    // 局部腿部关键点阈值刻意低于全身动作，因为只拍胯以下时 COCO Pose 的关键点置信度会明显下降。
    float kVisibility_ = 0.22f;
    float kHipVisibility_ = 0.18f;

    int kConfirmFrames_ = 2;
    int kMaxMissingFrames_ = 3;
    int kMinHalfCycleFrames_ = 4;

    int kMinCalibrationFrames_ = 8;
    float kMinScaleRange_ = 0.10f;
    float kMinKneeYRange_ = 0.30f;
    float kTriggerFraction_ = 0.28f;

    float kSignalAlpha_ = 0.35f;

    // 只用于可选膝角调试。
    double kBendAngle_ = 115.0;
    double kExtendAngle_ = 145.0;

    // ExerciseBase 兼容字段。
    double curAngle_ = -1.0;
    float curProgress_ = 0.0f;
};

} // namespace wakeai
