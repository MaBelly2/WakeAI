#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include "core/PoseDetector.h"
#include "core/PoseData.h"
#include "exercise/Squat.h"
#include "exercise/Cycling.h"
#include "exercise/JumpingJack.h"

using namespace wakeai;

//  改成你电脑上的实际路径！
const std::string MODEL_PATH = "F:/Git/WakeAI/models/yolov8n-pose.onnx";

// 画骨架（用 MediaPipe 索引）
void drawPose(cv::Mat& img, const PoseLandmarks& p) {
    static const std::pair<int, int> bones[] = {
        {LeftShoulder, RightShoulder},
        {LeftShoulder, LeftElbow},   {LeftElbow, LeftWrist},
        {RightShoulder, RightElbow}, {RightElbow, RightWrist},
        {LeftShoulder, LeftHip},     {RightShoulder, RightHip},
        {LeftHip, RightHip},
        {LeftHip, LeftKnee},         {LeftKnee, LeftAnkle},
        {RightHip, RightKnee},       {RightKnee, RightAnkle},
    };
    for (const auto& b : bones) {
        const Keypoint& a = p[b.first];
        const Keypoint& c = p[b.second];
        if (a.visible() && c.visible()) {
            cv::line(img, cv::Point((int)a.x, (int)a.y),
                cv::Point((int)c.x, (int)c.y), cv::Scalar(0, 255, 0), 2);
        }
    }
    for (int i = 0; i < PoseLandmarks::kCount; ++i) {
        if (p[i].visible()) {
            cv::circle(img, cv::Point((int)p[i].x, (int)p[i].y),
                3, cv::Scalar(0, 0, 255), -1);
        }
    }
}

int main() {
    PoseDetector detector;
    if (!detector.load(MODEL_PATH)) {
        std::cerr << "模型加载失败！请检查路径：" << MODEL_PATH << std::endl;
        return -1;
    }
    std::cout << "模型加载成功" << std::endl;

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "摄像头打不开" << std::endl;
        return -1;
    }

    Squat squat;
    Cycling cycling;
    JumpingJack jj;

    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        PoseLandmarks pose;
        if (detector.detect(frame, pose)) {
            //  测试时只保留一个动作，注释掉另外两个！
            /*squat.update(pose);
            cycling.update(pose);*/
            jj.update(pose);
            drawPose(frame, pose);
        }

        std::string text = "Squat:" + std::to_string(squat.count())
            + "  Cycling:" + std::to_string(cycling.count())
            + "  Jack:" + std::to_string(jj.count());
        cv::putText(frame, text, cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);

        cv::imshow("WakeAI", frame);
        if (cv::waitKey(1) == 27) break;   // ESC 退出
    }
    return 0;
}
