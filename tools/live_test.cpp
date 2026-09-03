#include <opencv2/opencv.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/PoseData.h"
#include "core/PoseDetector.h"
#include "core/PoseSmoother.h"

#include "exercise/Cycling.h"
#include "exercise/JumpingJack.h"
#include "exercise/Squat.h"

using namespace wakeai;

namespace {

enum class ActiveMode {
    Squat,
    JumpingJack,
    Cycling,
};

struct Options {
    std::string modelPath;
    std::string videoPath;
    int cameraIndex = 0;
    ActiveMode mode = ActiveMode::Squat;
    bool mirrorCamera = true;
    bool enableCsv = true;
};

const char* modeName(ActiveMode mode) {
    switch (mode) {
    case ActiveMode::Squat: return "SQUAT";
    case ActiveMode::JumpingJack: return "JUMPING_JACK";
    case ActiveMode::Cycling: return "CYCLING";
    }
    return "UNKNOWN";
}

const char* stateName(ExerciseState state) {
    switch (state) {
    case ExerciseState::Ready: return "READY";
    case ExerciseState::Down: return "ACTIVE";
    case ExerciseState::Up: return "RETURNING";
    }
    return "UNKNOWN";
}

const char* phaseName(CyclingPhase phase) {
    switch (phase) {
    case CyclingPhase::Unknown:
        return "UNKNOWN";
    case CyclingPhase::LeftDominant:
        return "LEFT_PHASE";
    case CyclingPhase::RightDominant:
        return "RIGHT_PHASE";
    }
    return "UNKNOWN";
}

const char* signalSourceName(CyclingSignalSource source) {
    switch (source) {
    case CyclingSignalSource::Unknown:
        return "UNKNOWN";
    case CyclingSignalSource::ShinScale:
        return "SHIN_SCALE";
    case CyclingSignalSource::KneeVertical:
        return "KNEE_Y";
    }
    return "UNKNOWN";
}

bool fileExists(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::exists(p, ec) && !ec;
}

std::string resolveModelPath(const std::string& userPath) {
    std::vector<std::filesystem::path> candidates;

    if (!userPath.empty()) {
        candidates.emplace_back(userPath);
    }

    // 兼容：
    // 1) 从项目根目录运行
    // 2) VS2022 CMake 常见 out/build/x64-Debug 目录运行
    candidates.emplace_back("models/yolov8n-pose.onnx");
    candidates.emplace_back("../models/yolov8n-pose.onnx");
    candidates.emplace_back("../../models/yolov8n-pose.onnx");
    candidates.emplace_back("../../../models/yolov8n-pose.onnx");
    candidates.emplace_back("../../../../models/yolov8n-pose.onnx");

    for (const auto& p : candidates) {
        if (fileExists(p)) {
            return std::filesystem::absolute(p).string();
        }
    }

    return {};
}

ActiveMode parseMode(const std::string& s) {
    if (s == "1" || s == "squat") {
        return ActiveMode::Squat;
    }
    if (s == "2" || s == "jack" || s == "jumpingjack") {
        return ActiveMode::JumpingJack;
    }
    if (s == "3" || s == "cycling" || s == "cycle") {
        return ActiveMode::Cycling;
    }
    return ActiveMode::Squat;
}

void printHelp() {
    std::cout
        << "\nWakeAI first-round exercise tester\n"
        << "Usage examples:\n"
        << "  WakeAI.exe --mode squat\n"
        << "  WakeAI.exe --video D:\\\\WakeAI\\\\test_videos\\\\squat_01.mp4 --mode squat\n"
        << "  WakeAI.exe --video D:\\\\WakeAI\\\\test_videos\\\\jack_01.mp4 --mode jack\n"
        << "  WakeAI.exe --video D:\\\\WakeAI\\\\test_videos\\\\cycling_01.mp4 --mode cycling\n\n"
        << "Optional:\n"
        << "  --model <onnx path>\n"
        << "  --camera <index>\n"
        << "  --no-mirror\n"
        << "  --no-csv\n\n"
        << "Keyboard while running:\n"
        << "  1 = Squat\n"
        << "  2 = Jumping Jack\n"
        << "  3 = Cycling\n"
        << "  R = reset current action\n"
        << "  B = restart test video from frame 0\n"
        << "  ESC = exit\n\n";
}

Options parseArgs(int argc, char** argv) {
    Options opt;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--video" && i + 1 < argc) {
            opt.videoPath = argv[++i];
        }
        else if (arg == "--model" && i + 1 < argc) {
            opt.modelPath = argv[++i];
        }
        else if (arg == "--camera" && i + 1 < argc) {
            opt.cameraIndex = std::stoi(argv[++i]);
        }
        else if (arg == "--mode" && i + 1 < argc) {
            opt.mode = parseMode(argv[++i]);
        }
        else if (arg == "--no-mirror") {
            opt.mirrorCamera = false;
        }
        else if (arg == "--no-csv") {
            opt.enableCsv = false;
        }
        else if (arg == "--help" || arg == "-h") {
            printHelp();
        }
    }

    return opt;
}

void interactiveSetup(Options& opt) {
    std::cout << "===== WakeAI first-round tester =====\n";
    std::cout << "Input source: 0 = camera, 1 = fixed video\n";
    std::cout << "Choose: ";

    int source = 0;
    std::cin >> source;

    if (source == 1) {
        std::cout << "Video path (example D:\\WakeAI\\test_videos\\squat_01.mp4):\n> ";
        std::cin >> std::ws;
        std::getline(std::cin, opt.videoPath);
    }
    else {
        std::cout << "Camera index (usually 0): ";
        std::cin >> opt.cameraIndex;
    }

    std::cout << "\nAction mode: 1 = Squat, 2 = Jumping Jack, 3 = Cycling\n";
    std::cout << "Choose: ";
    int mode = 1;
    std::cin >> mode;

    if (mode == 2) {
        opt.mode = ActiveMode::JumpingJack;
    }
    else if (mode == 3) {
        opt.mode = ActiveMode::Cycling;
    }
    else {
        opt.mode = ActiveMode::Squat;
    }
}

ExerciseBase& activeExercise(ActiveMode mode,
                             Squat& squat,
                             JumpingJack& jack,
                             Cycling& cycling) {
    switch (mode) {
    case ActiveMode::Squat:
        return squat;
    case ActiveMode::JumpingJack:
        return jack;
    case ActiveMode::Cycling:
        return cycling;
    }
    return squat;
}

cv::Scalar heatColor(float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    // OpenCV 使用 BGR。
    // 0: 蓝 -> 0.5: 黄 -> 1: 红
    if (t <= 0.5f) {
        const float u = t / 0.5f;
        const int b = static_cast<int>(255.0f * (1.0f - u));
        const int g = static_cast<int>(255.0f * u);
        const int r = static_cast<int>(255.0f * u);
        return cv::Scalar(b, g, r);
    }

    const float u = (t - 0.5f) / 0.5f;
    const int g = static_cast<int>(255.0f * (1.0f - u));
    return cv::Scalar(0, g, 255);
}

void drawBone(cv::Mat& image,
              const PoseLandmarks& pose,
              int aIndex,
              int bIndex,
              const cv::Scalar& color,
              int thickness = 2) {
    const Keypoint& a = pose[aIndex];
    const Keypoint& b = pose[bIndex];

    if (!a.visible() || !b.visible()) {
        return;
    }

    cv::line(image,
             cv::Point(static_cast<int>(a.x), static_cast<int>(a.y)),
             cv::Point(static_cast<int>(b.x), static_cast<int>(b.y)),
             color,
             thickness,
             cv::LINE_AA);
}

void drawPose(cv::Mat& image,
              const PoseLandmarks& pose,
              ActiveMode mode,
              const Cycling& cycling) {
    const cv::Scalar normalColor(0, 220, 0);

    drawBone(image, pose, LeftShoulder, RightShoulder, normalColor);
    drawBone(image, pose, LeftShoulder, LeftElbow, normalColor);
    drawBone(image, pose, LeftElbow, LeftWrist, normalColor);
    drawBone(image, pose, RightShoulder, RightElbow, normalColor);
    drawBone(image, pose, RightElbow, RightWrist, normalColor);

    drawBone(image, pose, LeftShoulder, LeftHip, normalColor);
    drawBone(image, pose, RightShoulder, RightHip, normalColor);
    drawBone(image, pose, LeftHip, RightHip, normalColor);

    if (mode == ActiveMode::Cycling && cycling.valid()) {
        const cv::Scalar leftColor = heatColor(cycling.leftExtension());
        const cv::Scalar rightColor = heatColor(cycling.rightExtension());

        drawBone(image, pose, LeftHip, LeftKnee, leftColor, 5);
        drawBone(image, pose, LeftKnee, LeftAnkle, leftColor, 5);
        drawBone(image, pose, RightHip, RightKnee, rightColor, 5);
        drawBone(image, pose, RightKnee, RightAnkle, rightColor, 5);
    }
    else {
        drawBone(image, pose, LeftHip, LeftKnee, normalColor);
        drawBone(image, pose, LeftKnee, LeftAnkle, normalColor);
        drawBone(image, pose, RightHip, RightKnee, normalColor);
        drawBone(image, pose, RightKnee, RightAnkle, normalColor);
    }

    for (int i = 0; i < PoseLandmarks::kCount; ++i) {
        if (pose[i].visible()) {
            cv::circle(image,
                       cv::Point(static_cast<int>(pose[i].x),
                                 static_cast<int>(pose[i].y)),
                       4,
                       cv::Scalar(0, 0, 255),
                       -1,
                       cv::LINE_AA);
        }
    }
}

void putLine(cv::Mat& image,
             const std::string& text,
             int lineIndex,
             const cv::Scalar& color = cv::Scalar(0, 255, 0)) {
    const int y = 28 + lineIndex * 27;
    cv::putText(image,
                text,
                cv::Point(10, y),
                cv::FONT_HERSHEY_SIMPLEX,
                0.65,
                color,
                2,
                cv::LINE_AA);
}

std::string format1(double value) {
    if (value < 0.0) {
        return "NA";
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << value;
    return ss.str();
}

std::string format2(double value) {
    if (value < 0.0) {
        return "NA";
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << value;
    return ss.str();
}

void drawDebugOverlay(cv::Mat& frame,
                      ActiveMode mode,
                      const ExerciseBase& active,
                      const Squat& squat,
                      const JumpingJack& jack,
                      const Cycling& cycling,
                      bool poseDetected,
                      double fps) {
    putLine(frame,
            std::string("MODE: ") + modeName(mode) +
            "   COUNT: " + std::to_string(active.count()) +
            "   FPS: " + std::to_string(static_cast<int>(fps)),
            0);

    putLine(frame,
            std::string("POSE: ") + (poseDetected ? "YES" : "NO") +
            "   ACTION_VALID: " + (active.valid() ? "YES" : "NO") +
            "   STATE: " + stateName(active.state()),
            1,
            active.valid() ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 165, 255));

    if (mode == ActiveMode::Squat) {
        putLine(frame,
                "Knee selected=" + format1(squat.angle()) +
                "  L=" + format1(squat.leftKneeAngle()) +
                "  R=" + format1(squat.rightKneeAngle()),
                2);

        putLine(frame,
                std::string("ARMED: ") + (squat.armed() ? "YES" : "NO") +
                "   target: DOWN<" + format1(squat.downThreshold()) +
                "  UP>" + format1(squat.upThreshold()),
                3);
    }
    else if (mode == ActiveMode::JumpingJack) {
        putLine(frame,
                "ArmLift L=" + format2(jack.leftArmLift()) +
                "  R=" + format2(jack.rightArmLift()) +
                "  Spread=" + format2(jack.legSpread()),
                2);

        putLine(frame,
                std::string("ARMED: ") + (jack.armed() ? "YES" : "NO") +
                "   open: arm>" + format2(jack.armOpenThreshold()) +
                " spread>" + format2(jack.spreadOpenThreshold()),
                3);
    }
    else {
        putLine(frame,
                std::string("CALIB: ") + (cycling.calibrated() ? "YES" : "NO") +
                "  src=" + signalSourceName(cycling.signalSource()) +
                "  phase=" + phaseName(cycling.phase()),
                2,
                cycling.calibrated() ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 165, 255));

        putLine(frame,
                "signal=" + format2(cycling.signal()) +
                "  base=" + format2(cycling.signalBaseline()) +
                "  trig=" + format2(cycling.signalTrigger()) +
                "  frames=" + std::to_string(cycling.calibrationFrames()),
                3);

        putLine(frame,
                "shin L=" + format1(cycling.leftShinLength()) +
                " R=" + format1(cycling.rightShinLength()) +
                "  scaleSig=" + format2(cycling.rawScaleSignal()) +
                " kneeYSig=" + format2(cycling.rawKneeYSignal()),
                4);

        putLine(frame,
                "vis H=" + format2(cycling.leftHipVisibility()) + "/" + format2(cycling.rightHipVisibility()) +
                " K=" + format2(cycling.leftKneeVisibility()) + "/" + format2(cycling.rightKneeVisibility()) +
                " A=" + format2(cycling.leftAnkleVisibility()) + "/" + format2(cycling.rightAnkleVisibility()),
                5);
    }

    putLine(frame,
            "Keys: 1 Squat | 2 Jack | 3 Cycling | R Reset | B Restart video | ESC Exit",
            mode == ActiveMode::Cycling ? 6 : 4,
            cv::Scalar(255, 255, 255));
}

void writeCsvHeader(std::ofstream& csv) {
    csv
        << "frame,time_ms,mode,pose_detected,action_valid,count,state,"
        << "squat_angle,squat_left_knee,squat_right_knee,"
        << "jack_left_arm_lift,jack_right_arm_lift,jack_leg_spread,"
        << "cycling_left_knee,cycling_right_knee,"
        << "cycling_left_extension,cycling_right_extension,cycling_phase,"
        << "cycling_calibrated,cycling_signal_source,cycling_signal,"
        << "cycling_signal_baseline,cycling_signal_trigger,"
        << "cycling_scale_signal,cycling_knee_y_signal,"
        << "cycling_left_shin,cycling_right_shin,"
        << "cycling_left_hip_vis,cycling_right_hip_vis,"
        << "cycling_left_knee_vis,cycling_right_knee_vis,"
        << "cycling_left_ankle_vis,cycling_right_ankle_vis\n";
}

void writeCsvRow(std::ofstream& csv,
                 long long frameIndex,
                 double timeMs,
                 ActiveMode mode,
                 bool poseDetected,
                 const ExerciseBase& active,
                 const Squat& squat,
                 const JumpingJack& jack,
                 const Cycling& cycling) {
    csv << frameIndex << ','
        << std::fixed << std::setprecision(3) << timeMs << ','
        << modeName(mode) << ','
        << (poseDetected ? 1 : 0) << ','
        << (active.valid() ? 1 : 0) << ','
        << active.count() << ','
        << stateName(active.state()) << ','
        << squat.angle() << ','
        << squat.leftKneeAngle() << ','
        << squat.rightKneeAngle() << ','
        << jack.leftArmLift() << ','
        << jack.rightArmLift() << ','
        << jack.legSpread() << ','
        << cycling.leftKneeAngle() << ','
        << cycling.rightKneeAngle() << ','
        << cycling.leftExtension() << ','
        << cycling.rightExtension() << ','
        << phaseName(cycling.phase()) << ','
        << (cycling.calibrated() ? 1 : 0) << ','
        << signalSourceName(cycling.signalSource()) << ','
        << cycling.signal() << ','
        << cycling.signalBaseline() << ','
        << cycling.signalTrigger() << ','
        << cycling.rawScaleSignal() << ','
        << cycling.rawKneeYSignal() << ','
        << cycling.leftShinLength() << ','
        << cycling.rightShinLength() << ','
        << cycling.leftHipVisibility() << ','
        << cycling.rightHipVisibility() << ','
        << cycling.leftKneeVisibility() << ','
        << cycling.rightKneeVisibility() << ','
        << cycling.leftAnkleVisibility() << ','
        << cycling.rightAnkleVisibility() << '\n';
}

void resetForMode(ActiveMode mode,
                  Squat& squat,
                  JumpingJack& jack,
                  Cycling& cycling) {
    activeExercise(mode, squat, jack, cycling).reset();
}

} // namespace

int main(int argc, char** argv) {
    printHelp();

    Options opt = parseArgs(argc, argv);

    // 对 VS2022 初学者友好：直接 F5 且没有参数时，用控制台选择摄像头/固定视频。
    if (argc == 1) {
        interactiveSetup(opt);
    }

    const std::string resolvedModelPath = resolveModelPath(opt.modelPath);

    if (resolvedModelPath.empty()) {
        std::cerr
            << "\nCannot find models/yolov8n-pose.onnx.\n"
            << "Please keep the model in WakeAI/models/ or run with --model <path>.\n";
        return -1;
    }

    std::cout << "\nModel: " << resolvedModelPath << '\n';

    PoseDetector detector;
    if (!detector.load(resolvedModelPath)) {
        std::cerr << "Model load failed.\n";
        return -1;
    }

    cv::VideoCapture cap;

    const bool useVideo = !opt.videoPath.empty();

    if (useVideo) {
        cap.open(opt.videoPath);
        std::cout << "Input video: " << opt.videoPath << '\n';
    }
    else {
        cap.open(opt.cameraIndex);
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        std::cout << "Camera index: " << opt.cameraIndex << '\n';
    }

    if (!cap.isOpened()) {
        std::cerr << "Cannot open camera/video input.\n";
        return -1;
    }

    // ===== 第一轮集中调参区 =====
    // 后续固定视频测试时，优先只改这里，不要到各 cpp 里到处找数字。
    Squat squat;
    squat.setThresholds(
        120.0,  // 下蹲：膝角低于该值
        155.0,  // 站起：膝角高于该值
        3       // 连续确认帧数
    );

    JumpingJack jack;
    jack.setThresholds(
        0.45f,  // 双手抬升打开阈值（肩宽归一化）
        -0.25f, // 双手放下关闭阈值
        1.25f,  // 双脚打开：脚踝间距/肩宽
        0.75f,  // 双脚并拢
        3       // 连续确认帧数
    );

    Cycling cycling;

    // ===== 床上蹬腿 V2：专门针对“手机只拍胯以下”的局部人体场景 =====
    // 主计数不再依赖髋点和膝角，而是只要求左右膝+左右踝，
    // 自动从“小腿二维投影尺度差”和“左右膝相对 y 位移”中选更稳定的信号。
    cycling.setPartialBodyConfig(
        0.22f,  // 膝/踝最低置信度
        2,      // 连续 2 个有效帧确认相位
        5,      // 最多容忍约 0.17 s 的短暂丢点（30 FPS）
        8       // 两个稳定相位至少间隔 8 帧
    );

    cycling.setCalibrationConfig(
        8,      // 仍保留快速标定；V3 用 trigger floor 防止早标定阈值过小
        0.10f,
        0.30f,
        0.28f
    );

    // debug_log(2) 中旧值 baseline=0.017、trigger=0.035。
    // 真实动作 SHIN_SCALE 可达约 ±1.2，而误触发段只有约 0.05~0.14，
    // 因此把 SHIN_SCALE 的绝对 trigger 下限设为 0.10。
    cycling.setSignalTriggerFloor(0.10f, 0.12f);

    // 同一次伸腿中的骨骼抖动即使偶然形成相位，也不能短时间连续加分。
    cycling.setMinCountIntervalFrames(10);

    // WakeAI 闹钟模式：每次稳定左右相位切换算 1 次。
    cycling.setCountMode(CyclingCountMode::EachPedal);

    // 保留辅助膝角参数，仅用于髋点偶尔可见时的 debug；不作为主计数条件。
    cycling.setThresholds(115.0, 145.0, 2);
    // ===== 调参区结束 =====

    PoseSmoother smoother(
        0.35f, // alpha 越小越平稳但延迟更大；建议先在 0.25~0.50 之间试
        0.20f
    );

    ActiveMode mode = opt.mode;

    std::ofstream csv;
    if (opt.enableCsv) {
        csv.open("debug_log.csv", std::ios::out | std::ios::trunc);
        if (csv.is_open()) {
            writeCsvHeader(csv);
            std::cout
                << "CSV log: "
                << std::filesystem::absolute("debug_log.csv").string()
                << '\n';
        }
        else {
            std::cerr << "Warning: cannot create debug_log.csv\n";
        }
    }

    cv::Mat frame;

    long long frameIndex = 0;

    auto fpsStart = std::chrono::steady_clock::now();
    auto appStart = fpsStart;
    int fpsFrames = 0;
    double fps = 0.0;

    std::cout
        << "\nRunning mode: " << modeName(mode) << '\n'
        << "Tip: fixed videos should contain a known number of repetitions.\n\n";

    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            if (useVideo) {
                std::cout << "\nVideo finished. Final count = "
                          << activeExercise(mode, squat, jack, cycling).count()
                          << '\n';
            }
            break;
        }

        ++frameIndex;
        ++fpsFrames;

        if (!useVideo && opt.mirrorCamera) {
            cv::flip(frame, frame, 1);
        }

        const auto now = std::chrono::steady_clock::now();
        const double fpsElapsed =
            std::chrono::duration<double>(now - fpsStart).count();

        if (fpsElapsed >= 1.0) {
            fps = static_cast<double>(fpsFrames) / fpsElapsed;
            fpsFrames = 0;
            fpsStart = now;
        }

        PoseLandmarks rawPose{};
        PoseLandmarks smoothPose{};

        const bool poseDetected = detector.detect(frame, rawPose);

        if (poseDetected) {
            smoothPose = smoother.update(rawPose);
        }
        else {
            // 用全 0 visibility 的姿态推进一次，让动作类清空瞬时消抖计数，
            // 防止“丢人几帧后接着上一段状态继续计数”。
            PoseLandmarks emptyPose{};
            smoothPose = smoother.update(emptyPose);
        }

        ExerciseBase& active = activeExercise(mode, squat, jack, cycling);
        active.update(smoothPose);

        if (poseDetected) {
            drawPose(frame, smoothPose, mode, cycling);
        }

        drawDebugOverlay(frame,
                         mode,
                         active,
                         squat,
                         jack,
                         cycling,
                         poseDetected,
                         fps);

        double timeMs = 0.0;
        if (useVideo) {
            timeMs = cap.get(cv::CAP_PROP_POS_MSEC);
        }
        else {
            timeMs = std::chrono::duration<double, std::milli>(
                         now - appStart)
                         .count();
        }

        if (csv.is_open()) {
            writeCsvRow(csv,
                        frameIndex,
                        timeMs,
                        mode,
                        poseDetected,
                        active,
                        squat,
                        jack,
                        cycling);
        }

        // ===== 显示前缩小，避免窗口过大（保持宽高比） =====
        cv::Mat display = frame;
        const int kMaxSide = 720;                 // 显示帧的最长边不超过720像素，可按需调
        const int side = std::max(display.cols, display.rows);
        if (side > kMaxSide) {
            const double s = static_cast<double>(kMaxSide) / side;
            cv::resize(display, display,
                cv::Size(static_cast<int>(display.cols * s),
                    static_cast<int>(display.rows * s)));
        }
        cv::imshow("WakeAI - First Round Exercise Test", display);

        const int key = cv::waitKey(1) & 0xFF;

        if (key == 27) {
            break;
        }
        else if (key == '1') {
            mode = ActiveMode::Squat;
            resetForMode(mode, squat, jack, cycling);
            std::cout << "Mode -> SQUAT\n";
        }
        else if (key == '2') {
            mode = ActiveMode::JumpingJack;
            resetForMode(mode, squat, jack, cycling);
            std::cout << "Mode -> JUMPING_JACK\n";
        }
        else if (key == '3') {
            mode = ActiveMode::Cycling;
            resetForMode(mode, squat, jack, cycling);
            std::cout << "Mode -> CYCLING\n";
        }
        else if (key == 'r' || key == 'R') {
            resetForMode(mode, squat, jack, cycling);
            smoother.reset();
            std::cout << "Current action reset.\n";
        }
        else if ((key == 'b' || key == 'B') && useVideo) {
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            resetForMode(mode, squat, jack, cycling);
            smoother.reset();
            frameIndex = 0;
            std::cout << "Video restarted from frame 0.\n";
        }
    }

    if (csv.is_open()) {
        csv.flush();
        csv.close();
    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}
