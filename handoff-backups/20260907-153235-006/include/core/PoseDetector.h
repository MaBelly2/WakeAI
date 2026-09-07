#pragma once

#include <opencv2/opencv.hpp>
#include "core/PoseData.h"

namespace wakeai {

    // 用 OpenCV DNN 加载 YOLO-pose 的 ONNX 模型，输出人体关键点
    class PoseDetector {
    public:
        PoseDetector() = default;

        // 加载 ONNX 模型，成功返回 true
        bool load(const std::string& modelPath);

        // 从一帧图像检测关键点，结果填入 out；没检测到人返回 false
        bool detect(const cv::Mat& frame, PoseLandmarks& out);

    private:
        void preprocess(const cv::Mat& frame, cv::Mat& blob,
            float& scale, float& padX, float& padY);

        bool postprocess(const cv::Mat& output, int imgW, int imgH,
            float scale, float padX, float padY,
            PoseLandmarks& result) const;

        cv::dnn::Net net_;
        int inputSize_ = 640;        // 模型输入尺寸
        float confThreshold_ = 0.4f; // 人体检测置信度
        float nmsThreshold_ = 0.5f;  // 去重叠阈值
    };

} // namespace wakeai
