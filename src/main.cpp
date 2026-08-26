#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cout << "摄像头打不开" << std::endl;
        return -1;
    }
    cv::Mat frame;
    cv::namedWindow("WakeAI Test", cv::WINDOW_AUTOSIZE);
    while (true) {
        cap >> frame;
        if (frame.empty()) break;
        cv::imshow("WakeAI Test", frame);
        if (cv::waitKey(30) == 27) break;
    }
    return 0;
}
