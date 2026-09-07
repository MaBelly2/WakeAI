#include "core/PoseDetector.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <utility>

namespace wakeai {

    bool PoseDetector::load(const std::string& modelPath) {
        try {
            net_ = cv::dnn::readNetFromONNX(modelPath);
            return !net_.empty();
        }
        catch (const cv::Exception& e) {
            std::cerr << "读取 ONNX 失败: " << e.what() << std::endl;
            return false;
        }
    }

    void PoseDetector::preprocess(const cv::Mat& frame, cv::Mat& blob,
        float& scale, float& padX, float& padY) {
        int w = frame.cols, h = frame.rows;

        // letterbox：等比缩放 + 灰边填充，保持宽高比（避免变形影响关键点精度）
        scale = std::min((float)inputSize_ / w, (float)inputSize_ / h);
        int newW = (int)std::round(w * scale);
        int newH = (int)std::round(h * scale);
        padX = (inputSize_ - newW) / 2.0f;
        padY = (inputSize_ - newH) / 2.0f;

        cv::Mat resized;
        cv::resize(frame, resized, cv::Size(newW, newH));
        cv::Mat letterboxed(inputSize_, inputSize_, frame.type(), cv::Scalar(114, 114, 114));
        resized.copyTo(letterboxed(cv::Rect((int)padX, (int)padY, newW, newH)));

        // 转 blob：BGR→RGB，归一化到 0~1
        blob = cv::dnn::blobFromImage(letterboxed, 1.0 / 255.0, cv::Size(), cv::Scalar(), true);
    }

    bool PoseDetector::detect(const cv::Mat& frame, PoseLandmarks& out) {
        if (frame.empty() || net_.empty()) return false;

        float scale, padX, padY;
        cv::Mat blob;
        preprocess(frame, blob, scale, padX, padY);

        net_.setInput(blob);
        cv::Mat output = net_.forward();

        return postprocess(output, frame.cols, frame.rows, scale, padX, padY, out);
    }

    bool PoseDetector::postprocess(const cv::Mat& output, int imgW, int imgH,
        float scale, float padX, float padY,
        PoseLandmarks& result) const {
        const int kNumKpts = 17;
        const int kAttr = 56;   // 4(框) + 1(置信度) + 17*3(关键点)

        // 输出形状 [1, 56, 8400] → 变成 [56, 8400]
        cv::Mat out = output.reshape(1, kAttr);

        std::vector<cv::Rect> boxes;
        std::vector<float> confs;
        std::vector<std::vector<cv::Point2f>> allKpts;
        std::vector<std::vector<float>> allKconf;

        for (int i = 0; i < out.cols; ++i) {
            float conf = out.at<float>(4, i);
            if (conf < confThreshold_) continue;

            float cx = out.at<float>(0, i);
            float cy = out.at<float>(1, i);
            float w = out.at<float>(2, i);
            float h = out.at<float>(3, i);

            boxes.push_back(cv::Rect((int)(cx - w / 2), (int)(cy - h / 2), (int)w, (int)h));
            confs.push_back(conf);

            std::vector<cv::Point2f> kpts(kNumKpts);
            std::vector<float> kconf(kNumKpts);
            for (int k = 0; k < kNumKpts; ++k) {
                float kx = out.at<float>(5 + k * 3 + 0, i);
                float ky = out.at<float>(5 + k * 3 + 1, i);
                float kc = out.at<float>(5 + k * 3 + 2, i);
                kpts[k] = cv::Point2f((kx - padX) / scale, (ky - padY) / scale);  // 转回原图坐标
                kconf[k] = kc;
            }
            allKpts.push_back(std::move(kpts));
            allKconf.push_back(std::move(kconf));
        }

        if (boxes.empty()) return false;

        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confs, confThreshold_, nmsThreshold_, indices);
        if (indices.empty()) return false;

        // 取置信度最高的人
        int best = indices[0];
        for (int idx : indices) {
            if (confs[idx] > confs[best]) best = idx;
        }

        // YOLO 的 17 个点 → MediaPipe 的 33 点索引
        static const int kYoloToMp[17] = {
            Nose,
            LeftEye, RightEye,
            LeftEar, RightEar,
            LeftShoulder, RightShoulder,
            LeftElbow, RightElbow,
            LeftWrist, RightWrist,
            LeftHip, RightHip,
            LeftKnee, RightKnee,
            LeftAnkle, RightAnkle,
        };

        result = PoseLandmarks{};   // 先清零（未映射的点可见度=0）

        for (int k = 0; k < kNumKpts; ++k) {
            int mpIdx = kYoloToMp[k];
            result[mpIdx].x = allKpts[best][k].x;
            result[mpIdx].y = allKpts[best][k].y;
            result[mpIdx].visibility = allKconf[best][k];
        }

        return true;
    }

} // namespace wakeai
