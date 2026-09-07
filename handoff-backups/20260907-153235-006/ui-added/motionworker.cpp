#include "motionworker.h"

#include <QDebug>
#include <QThread>

#include "core/PoseDetector.h"
#include "core/PoseSmoother.h"
#include "exercise/ExerciseBase.h"
#include "exercise/Squat.h"
#include "exercise/JumpingJack.h"
#include "exercise/Cycling.h"

#include <opencv2/opencv.hpp>

using namespace wakeai;

MotionWorker::MotionWorker(QObject *parent) : QObject(parent) {}
MotionWorker::~MotionWorker() { stop(); }

void MotionWorker::setModelPath(const QString &p) { modelPath_ = p; }
void MotionWorker::setMode(Mode m) { mode_ = m; }
void MotionWorker::stop()   { stopFlag_ = true; }
void MotionWorker::pause()  { paused_ = true; }
void MotionWorker::resume() { paused_ = false; }

void MotionWorker::reset()
{
    if (active_) active_->reset();
}

void MotionWorker::start()
{
    stopFlag_ = false;
    paused_ = false;
    runLoop();
    emit finished();
}

void MotionWorker::runLoop()
{
    PoseDetector detector;
    if (!detector.load(modelPath_.toStdString())) {
        qDebug() << "[debug] 模型加载失败";
        emit stateChanged("模型加载失败");
        return;
    }
    qDebug() << "[debug] 模型加载成功";

    Squat squat; JumpingJack jack; Cycling cycling;

    if (mode_ == Mode::Squat)
        squat.setThresholds(120.0, 155.0, 3);
    else if (mode_ == Mode::JumpingJack)
        jack.setThresholds(0.45f, -0.25f, 1.25f, 0.75f, 3);
    else {
        cycling.setPartialBodyConfig(0.22f, 2, 5, 8);
        cycling.setCalibrationConfig(8, 0.10f, 0.30f, 0.28f);
        cycling.setSignalTriggerFloor(0.10f, 0.12f);
        cycling.setMinCountIntervalFrames(10);
        cycling.setCountMode(CyclingCountMode::EachPedal);
    }

    active_ = &squat;
    if (mode_ == Mode::JumpingJack) active_ = &jack;
    if (mode_ == Mode::Cycling)     active_ = &cycling;

    PoseSmoother smoother(0.35f, 0.20f);

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        qDebug() << "[debug] 摄像头打不开";
        emit stateChanged("无法打开摄像头");
        return;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    qDebug() << "[debug] 摄像头打开成功";

    int frameCount = 0;
    while (!stopFlag_) {
        cv::Mat frame;
        if (!cap.read(frame) || frame.empty()) continue;
        cv::flip(frame, frame, 1);
        cv::Mat display = frame.clone();
        cv::cvtColor(display, display, cv::COLOR_BGR2RGB);
        emit frameReady(
            QImage(display.data, display.cols, display.rows,
                   int(display.step), QImage::Format_RGB888)
                .copy());
        // 暂停时：照常读帧，但跳过检测与计数
        if (paused_) { QThread::msleep(50); continue; }

        PoseLandmarks raw{}, smooth{};
        const bool detected = detector.detect(frame, raw);
        smooth = smoother.update(detected ? raw : PoseLandmarks{});
        active_->update(smooth);

        emit poseValidChanged(active_->valid());
        emit countChanged(active_->count());

        if (++frameCount % 30 == 0)
            qDebug() << "[debug] pose:" << detected
                     << "valid:" << active_->valid()
                     << "count:" << active_->count();
    }

    cap.release();
}