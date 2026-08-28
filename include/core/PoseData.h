#pragma once

#include <algorithm>  // std::clamp
#include <cmath>      // std::sqrt, std::acos

namespace wakeai {

    // 单个关键点：像素坐标 + 可见度
    struct Keypoint {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;           // 深度，一般用不到，保留
        float visibility = 0.0f;  // 可见度 0~1

        bool visible(float threshold = 0.5f) const {
            return visibility >= threshold;
        }
    };

    // 人体关键点索引（MediaPipe Pose 的 33 个点编号）
    enum Landmark : int {
        Nose = 0,
        LeftEyeInner = 1, LeftEye = 2, LeftEyeOuter = 3,
        RightEyeInner = 4, RightEye = 5, RightEyeOuter = 6,
        LeftEar = 7, RightEar = 8,
        MouthLeft = 9, MouthRight = 10,
        LeftShoulder = 11, RightShoulder = 12,
        LeftElbow = 13, RightElbow = 14,
        LeftWrist = 15, RightWrist = 16,
        LeftPinky = 17, RightPinky = 18,
        LeftIndex = 19, RightIndex = 20,
        LeftThumb = 21, RightThumb = 22,
        LeftHip = 23, RightHip = 24,
        LeftKnee = 25, RightKnee = 26,
        LeftAnkle = 27, RightAnkle = 28,
        LeftHeel = 29, RightHeel = 30,
        LeftFootIndex = 31, RightFootIndex = 32,
    };

    // 一帧的姿态数据：33 个关键点
    struct PoseLandmarks {
        static constexpr int kCount = 33;
        Keypoint pts[kCount];

        Keypoint& operator[](int i) { return pts[i]; }
        const Keypoint& operator[](int i) const { return pts[i]; }

        // 两点像素距离
        static float dist(const Keypoint& a, const Keypoint& b) {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        // 三点夹角，顶点为 b，单位：度（0~180）
        static float angleDeg(const Keypoint& a, const Keypoint& b, const Keypoint& c) {
            float v1x = a.x - b.x, v1y = a.y - b.y;
            float v2x = c.x - b.x, v2y = c.y - b.y;
            float dot = v1x * v2x + v1y * v2y;
            float m1 = dist(a, b);
            float m2 = dist(c, b);
            if (m1 < 1e-6f || m2 < 1e-6f) return 0.0f;
            float cosA = dot / (m1 * m2);
            cosA = std::clamp(cosA, -1.0f, 1.0f);
            return std::acos(cosA) * 180.0f / 3.14159265358979f;
        }
    };

} // namespace wakeai
