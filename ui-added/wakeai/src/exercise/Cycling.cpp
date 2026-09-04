#include "exercise/Cycling.h"

#include <algorithm>
#include <cmath>

namespace wakeai {

Cycling::Cycling() = default;

bool Cycling::computeLowerLegMetrics(const PoseLandmarks& pose,
                                     int kneeIndex,
                                     int ankleIndex,
                                     float& shinLength) const {
    const Keypoint& knee = pose[kneeIndex];
    const Keypoint& ankle = pose[ankleIndex];

    if (!(knee.visible(kVisibility_) && ankle.visible(kVisibility_))) {
        return false;
    }

    shinLength = PoseLandmarks::dist(knee, ankle);

    // 太短通常意味着关键点重合/严重误检。
    if (shinLength < 8.0f) {
        return false;
    }

    return true;
}

bool Cycling::computeOptionalKneeAngle(const PoseLandmarks& pose,
                                       int hipIndex,
                                       int kneeIndex,
                                       int ankleIndex,
                                       double& angle) const {
    const Keypoint& hip = pose[hipIndex];
    const Keypoint& knee = pose[kneeIndex];
    const Keypoint& ankle = pose[ankleIndex];

    // 注意：髋点只作为“辅助角度”，不再决定本帧是否有效。
    if (!(hip.visible(kHipVisibility_) &&
          knee.visible(kVisibility_) &&
          ankle.visible(kVisibility_))) {
        angle = -1.0;
        return false;
    }

    const float thigh = PoseLandmarks::dist(hip, knee);
    const float shin = PoseLandmarks::dist(knee, ankle);

    if (thigh < 8.0f || shin < 8.0f) {
        angle = -1.0;
        return false;
    }

    angle = PoseLandmarks::angleDeg(hip, knee, ankle);
    return true;
}

void Cycling::updateCalibration(float scaleSignal, float kneeYSignal) {
    ++validSignalFrames_;

    scaleMin_ = std::min(scaleMin_, scaleSignal);
    scaleMax_ = std::max(scaleMax_, scaleSignal);
    kneeYMin_ = std::min(kneeYMin_, kneeYSignal);
    kneeYMax_ = std::max(kneeYMax_, kneeYSignal);

    if (calibrated_ || validSignalFrames_ < kMinCalibrationFrames_) {
        return;
    }

    const float scaleRange = scaleMax_ - scaleMin_;
    const float kneeYRange = kneeYMax_ - kneeYMin_;

    const float scaleScore = scaleRange / std::max(kMinScaleRange_, 1e-6f);
    const float kneeYScore = kneeYRange / std::max(kMinKneeYRange_, 1e-6f);

    // 至少有一种二维特征已经出现足够明显的左右交替变化，才结束标定。
    if (scaleScore < 1.0f && kneeYScore < 1.0f) {
        return;
    }

    if (scaleScore >= kneeYScore) {
        signalSource_ = CyclingSignalSource::ShinScale;
        signalBaseline_ = 0.5f * (scaleMin_ + scaleMax_);
        signalTrigger_ = std::max(kMinScaleTrigger_, kTriggerFraction_ * scaleRange);
    }
    else {
        signalSource_ = CyclingSignalSource::KneeVertical;
        signalBaseline_ = 0.5f * (kneeYMin_ + kneeYMax_);
        signalTrigger_ = std::max(kMinKneeYTrigger_, kTriggerFraction_ * kneeYRange);
    }

    calibrated_ = true;

    // 标定结束后从干净状态开始找 A/B 相位。
    phase_ = CyclingPhase::Unknown;
    candidatePhase_ = CyclingPhase::Unknown;
    startPhase_ = CyclingPhase::Unknown;
    candidateFrames_ = 0;
    seenOpposite_ = false;
    state_ = ExerciseState::Ready;
}

CyclingPhase Cycling::classifySelectedSignal() const {
    if (!calibrated_ || signalSource_ == CyclingSignalSource::Unknown) {
        return CyclingPhase::Unknown;
    }

    if (selectedSignal_ > signalBaseline_ + signalTrigger_) {
        return CyclingPhase::LeftDominant;
    }

    if (selectedSignal_ < signalBaseline_ - signalTrigger_) {
        return CyclingPhase::RightDominant;
    }

    return CyclingPhase::Unknown;
}

void Cycling::updateHeatValues() {
    // 热力颜色仍优先基于“小腿二维投影长度比例”，符合你们方案一的视觉设定。
    // 这里表达相对尺度变化，不表达真实三维距离。
    if (!std::isfinite(scaleMin_) || !std::isfinite(scaleMax_)) {
        leftHeat_ = 0.5f;
        rightHeat_ = 0.5f;
        return;
    }

    const float scaleMid = 0.5f * (scaleMin_ + scaleMax_);
    const float halfRange = std::max(0.05f, 0.5f * (scaleMax_ - scaleMin_));

    const float normalized = std::clamp(
        (filteredScaleSignal_ - scaleMid) / halfRange,
        -1.0f,
        1.0f);

    leftHeat_ = std::clamp(0.5f + 0.5f * normalized, 0.0f, 1.0f);
    rightHeat_ = 1.0f - leftHeat_;
}

void Cycling::update(const PoseLandmarks& pose) {
    ++frameCounter_;

    // 保存当前帧置信度，方便 CSV/屏幕直接看到到底是哪一个点经常掉。
    leftHipVis_ = pose[LeftHip].visibility;
    rightHipVis_ = pose[RightHip].visibility;
    leftKneeVis_ = pose[LeftKnee].visibility;
    rightKneeVis_ = pose[RightKnee].visibility;
    leftAnkleVis_ = pose[LeftAnkle].visibility;
    rightAnkleVis_ = pose[RightAnkle].visibility;

    float leftShin = 0.0f;
    float rightShin = 0.0f;

    const bool leftOk = computeLowerLegMetrics(
        pose, LeftKnee, LeftAnkle, leftShin);
    const bool rightOk = computeLowerLegMetrics(
        pose, RightKnee, RightAnkle, rightShin);

    // V2 的关键变化：
    // 床上蹬腿只要求“左右膝 + 左右踝”，不再强制要求两个髋点。
    valid_ = leftOk && rightOk;

    if (!valid_) {
        ++missingFrames_;

        // 短暂丢点不立即破坏已建立的 A/B 相位；
        // 连续丢太久才清空“候选相位”，但保留当前已稳定相位和计数。
        if (missingFrames_ > kMaxMissingFrames_) {
            candidatePhase_ = CyclingPhase::Unknown;
            candidateFrames_ = 0;
        }

        return;
    }

    missingFrames_ = 0;

    leftShin_ = leftShin;
    rightShin_ = rightShin;

    // 髋点可见时才计算膝角；不可见时保持 -1，绝不影响局部腿部计数。
    computeOptionalKneeAngle(
        pose, LeftHip, LeftKnee, LeftAnkle, leftAngle_);
    computeOptionalKneeAngle(
        pose, RightHip, RightKnee, RightAnkle, rightAngle_);

    if (leftAngle_ >= 0.0 && rightAngle_ >= 0.0) {
        curAngle_ = std::min(leftAngle_, rightAngle_);
    }
    else if (leftAngle_ >= 0.0) {
        curAngle_ = leftAngle_;
    }
    else if (rightAngle_ >= 0.0) {
        curAngle_ = rightAngle_;
    }
    else {
        curAngle_ = -1.0;
    }

    // ---------------- 主特征1：左右小腿二维投影尺度 ----------------
    // 同一个人的左右小腿真实长度近似相同，因此 log(L/R) 可以较好地抵消
    // “手机整体靠近/远离”造成的共同尺度变化。
    // 注意：这只是二维投影尺度，不是绝对深度。
    const float ratio = std::max(leftShin_, 1e-3f) /
                        std::max(rightShin_, 1e-3f);
    rawScaleSignal_ = std::clamp(std::log(ratio), -1.2f, 1.2f);

    // ---------------- 主特征2：左右膝相对 y 位移 ----------------
    // 这是备用二维运动特征。除以平均小腿长度后，对用户与摄像头整体距离更不敏感。
    const float avgShin = std::max(0.5f * (leftShin_ + rightShin_), 1.0f);
    rawKneeYSignal_ = std::clamp(
        (pose[LeftKnee].y - pose[RightKnee].y) / avgShin,
        -3.0f,
        3.0f);

    // 对信号再做一层轻量 EMA；关键点坐标本身已经由 PoseSmoother 平滑，
    // 这里主要是为了减少“相位阈值附近来回跳”。
    if (!signalFilterInitialized_) {
        filteredScaleSignal_ = rawScaleSignal_;
        filteredKneeYSignal_ = rawKneeYSignal_;
        signalFilterInitialized_ = true;
    }
    else {
        filteredScaleSignal_ =
            kSignalAlpha_ * rawScaleSignal_ +
            (1.0f - kSignalAlpha_) * filteredScaleSignal_;

        filteredKneeYSignal_ =
            kSignalAlpha_ * rawKneeYSignal_ +
            (1.0f - kSignalAlpha_) * filteredKneeYSignal_;
    }

    updateCalibration(filteredScaleSignal_, filteredKneeYSignal_);
    updateHeatValues();

    if (!calibrated_) {
        // 标定阶段不计数；让用户正常蹬几下即可自动完成。
        curProgress_ = 0.0f;
        return;
    }

    selectedSignal_ =
        (signalSource_ == CyclingSignalSource::ShinScale)
            ? filteredScaleSignal_
            : filteredKneeYSignal_;

    curProgress_ = std::clamp(
        std::abs(selectedSignal_ - signalBaseline_) /
            std::max(signalTrigger_ * 2.0f, 1e-4f),
        0.0f,
        1.0f);

    const CyclingPhase rawPhase = classifySelectedSignal();

    // 中间过渡区不切相位，但也不把已有稳定相位清掉。
    if (rawPhase == CyclingPhase::Unknown) {
        candidatePhase_ = CyclingPhase::Unknown;
        candidateFrames_ = 0;
        return;
    }

    if (rawPhase == candidatePhase_) {
        ++candidateFrames_;
    }
    else {
        candidatePhase_ = rawPhase;
        candidateFrames_ = 1;
    }

    if (candidateFrames_ < kConfirmFrames_) {
        return;
    }

    // 同一个稳定相位不重复处理。
    if (phase_ == rawPhase) {
        return;
    }

    // 防止 A/B 在几帧内因为骨骼抖动快速翻转。
    if (frameCounter_ - lastStablePhaseFrame_ < kMinHalfCycleFrames_) {
        return;
    }

    phase_ = rawPhase;
    lastStablePhaseFrame_ = frameCounter_;
    candidateFrames_ = 0;

    // 第一次稳定相位只作为起点。
    if (startPhase_ == CyclingPhase::Unknown) {
        startPhase_ = phase_;
        seenOpposite_ = false;
        state_ = ExerciseState::Ready;
        return;
    }

    // WakeAI 默认 EachPedal：每次“稳定的左右相位切换”算 1 次。
    // 但必须同时通过：
    // 1) 信号跨过 trigger；2) 连续确认帧；3) 稳定相位最小间隔；
    // 4) 计数冷却时间。四层保护共同抑制一次伸腿过程中连续计数。
    if (countMode_ == CyclingCountMode::EachPedal) {
        if (phase_ != startPhase_) {
            if (frameCounter_ - lastCountFrame_ >= kMinCountIntervalFrames_) {
                ++count_;
                lastCountFrame_ = frameCounter_;
            }

            // 不论这次是否因冷却被拒绝，都把当前稳定相位作为新的参考。
            // 这样不会在若干帧后“补计”一次已经判定为抖动的切换。
            startPhase_ = phase_;
            state_ = ExerciseState::Down;
        }
        return;
    }

    // FullCycle：A -> B -> A = 1。
    if (phase_ != startPhase_) {
        seenOpposite_ = true;
        oppositePhaseFrame_ = frameCounter_;
        state_ = ExerciseState::Down;
        return;
    }

    if (seenOpposite_) {
        if (frameCounter_ - oppositePhaseFrame_ >= kMinHalfCycleFrames_ &&
            frameCounter_ - lastCountFrame_ >= kMinCountIntervalFrames_) {
            ++count_;
            lastCountFrame_ = frameCounter_;
        }

        seenOpposite_ = false;
        state_ = ExerciseState::Ready;
    }
}

int Cycling::count() const {
    return count_;
}

double Cycling::angle() const {
    return curAngle_;
}

ExerciseState Cycling::state() const {
    return state_;
}

float Cycling::progress() const {
    return valid_ ? curProgress_ : 0.0f;
}

bool Cycling::valid() const {
    return valid_;
}

void Cycling::reset() {
    leftAngle_ = -1.0;
    rightAngle_ = -1.0;
    leftShin_ = 0.0f;
    rightShin_ = 0.0f;
    leftHeat_ = 0.5f;
    rightHeat_ = 0.5f;

    rawScaleSignal_ = 0.0f;
    rawKneeYSignal_ = 0.0f;
    filteredScaleSignal_ = 0.0f;
    filteredKneeYSignal_ = 0.0f;
    signalFilterInitialized_ = false;

    leftHipVis_ = 0.0f;
    rightHipVis_ = 0.0f;
    leftKneeVis_ = 0.0f;
    rightKneeVis_ = 0.0f;
    leftAnkleVis_ = 0.0f;
    rightAnkleVis_ = 0.0f;

    calibrated_ = false;
    validSignalFrames_ = 0;
    scaleMin_ = std::numeric_limits<float>::infinity();
    scaleMax_ = -std::numeric_limits<float>::infinity();
    kneeYMin_ = std::numeric_limits<float>::infinity();
    kneeYMax_ = -std::numeric_limits<float>::infinity();

    signalSource_ = CyclingSignalSource::Unknown;
    selectedSignal_ = 0.0f;
    signalBaseline_ = 0.0f;
    signalTrigger_ = 0.0f;

    count_ = 0;
    state_ = ExerciseState::Ready;
    valid_ = false;

    phase_ = CyclingPhase::Unknown;
    candidatePhase_ = CyclingPhase::Unknown;
    startPhase_ = CyclingPhase::Unknown;

    candidateFrames_ = 0;
    missingFrames_ = 0;
    frameCounter_ = 0;
    lastStablePhaseFrame_ = -100000;
    oppositePhaseFrame_ = -100000;
    seenOpposite_ = false;
    lastCountFrame_ = -100000;

    curAngle_ = -1.0;
    curProgress_ = 0.0f;
}

void Cycling::setThresholds(double bendAngle,
                            double extendAngle,
                            int confirmFrames) {
    if (bendAngle < extendAngle) {
        kBendAngle_ = bendAngle;
        kExtendAngle_ = extendAngle;
    }

    kConfirmFrames_ = std::max(1, confirmFrames);
}

void Cycling::setPartialBodyConfig(float visibility,
                                   int confirmFrames,
                                   int maxMissingFrames,
                                   int minHalfCycleFrames) {
    kVisibility_ = std::clamp(visibility, 0.05f, 0.90f);
    kConfirmFrames_ = std::max(1, confirmFrames);
    kMaxMissingFrames_ = std::max(0, maxMissingFrames);
    kMinHalfCycleFrames_ = std::max(1, minHalfCycleFrames);
}

void Cycling::setCalibrationConfig(int minCalibrationFrames,
                                   float minScaleRange,
                                   float minKneeYRange,
                                   float triggerFraction) {
    kMinCalibrationFrames_ = std::max(4, minCalibrationFrames);
    kMinScaleRange_ = std::max(0.03f, minScaleRange);
    kMinKneeYRange_ = std::max(0.08f, minKneeYRange);
    kTriggerFraction_ = std::clamp(triggerFraction, 0.15f, 0.45f);
}


void Cycling::setSignalTriggerFloor(float scaleTriggerFloor,
                                    float kneeYTriggerFloor) {
    kMinScaleTrigger_ = std::clamp(scaleTriggerFloor, 0.03f, 0.50f);
    kMinKneeYTrigger_ = std::clamp(kneeYTriggerFloor, 0.05f, 1.00f);
}

void Cycling::setMinCountIntervalFrames(int frames) {
    kMinCountIntervalFrames_ = std::max(1, frames);
}

void Cycling::setCountMode(CyclingCountMode mode) {
    countMode_ = mode;
}

const char* Cycling::name() const {
    return "床上蹬腿";
}

} // namespace wakeai
