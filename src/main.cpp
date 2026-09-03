#include <QApplication>
#include <filesystem>
#include <vector>
#include <string>

#include "ui/MainWindow.h"

static bool fileExists(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::exists(p, ec) && !ec;
}

static std::string resolveModelPath() {
    const std::vector<std::string> candidates = {
        "models/yolov8n-pose.onnx",
        "../models/yolov8n-pose.onnx",
        "../../models/yolov8n-pose.onnx",
        "../../../models/yolov8n-pose.onnx",
        "../../../../models/yolov8n-pose.onnx",
    };
    for (const auto& p : candidates) {
        if (fileExists(p)) {
            return std::filesystem::absolute(p).string();
        }
    }
    return {};
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    wakeai::MainWindow w(resolveModelPath());
    w.resize(720, 780);
    w.show();

    return app.exec();
}
