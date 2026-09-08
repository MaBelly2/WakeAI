#include "motionworker.h"
#include "core/PoseDetector.h"
#include "core/PoseSmoother.h"
#include "exercise/Squat.h"
#include "exercise/JumpingJack.h"
#include "exercise/Cycling.h"
#include "exercise/ExerciseConfig.h"
#include <opencv2/opencv.hpp>
#include <QElapsedTimer>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>
using namespace wakeai;
namespace {
void drawPose(cv::Mat& frame, const PoseLandmarks& p) {
    constexpr float threshold=0.22f;
    const auto point=[&p](int index){
        return cv::Point(cvRound(p[index].x),cvRound(p[index].y));
    };

    // 先为躯干铺一层很轻的半透明强调色，画面更接近成品运动 App。
    if(p[LeftShoulder].visible(threshold)&&p[RightShoulder].visible(threshold)
        &&p[RightHip].visible(threshold)&&p[LeftHip].visible(threshold)) {
        std::vector<cv::Point> torso={point(LeftShoulder),point(RightShoulder),
            point(RightHip),point(LeftHip)};
        cv::Mat overlay=frame.clone();
        cv::fillConvexPoly(overlay,torso,cv::Scalar(181,211,66),cv::LINE_AA);
        cv::addWeighted(overlay,0.16,frame,0.84,0.0,frame);
    }

    const std::pair<int,int> bones[]={
        {Nose,LeftShoulder},{Nose,RightShoulder},
        {LeftShoulder,RightShoulder},{LeftShoulder,LeftElbow},
        {LeftElbow,LeftWrist},{RightShoulder,RightElbow},{RightElbow,RightWrist},
        {LeftShoulder,LeftHip},{RightShoulder,RightHip},{LeftHip,RightHip},
        {LeftHip,LeftKnee},{LeftKnee,LeftAnkle},{RightHip,RightKnee},{RightKnee,RightAnkle}
    };
    for(const auto& bone:bones) {
        if(!p[bone.first].visible(threshold)||!p[bone.second].visible(threshold))
            continue;
        // 深色外描边保证白线在白墙、窗户等亮背景上仍然清楚。
        cv::line(frame,point(bone.first),point(bone.second),cv::Scalar(24,30,31),7,cv::LINE_AA);
        cv::line(frame,point(bone.first),point(bone.second),cv::Scalar(250,250,250),3,cv::LINE_AA);
    }
    const int joints[]={Nose,LeftShoulder,RightShoulder,LeftElbow,RightElbow,
        LeftWrist,RightWrist,LeftHip,RightHip,LeftKnee,RightKnee,LeftAnkle,RightAnkle};
    for(const int index:joints) {
        if(!p[index].visible(threshold))
            continue;
        cv::circle(frame,point(index),6,cv::Scalar(24,30,31),-1,cv::LINE_AA);
        cv::circle(frame,point(index),3,cv::Scalar(181,211,66),-1,cv::LINE_AA);
    }
}
}
MotionWorker::MotionWorker(Options options,std::shared_ptr<MotionControl> control)
    : options_(std::move(options)),control_(std::move(control)) {}
void MotionWorker::start() {
    try { if(!control_->stop.load()) runLoop(); }
    catch(const cv::Exception& e) { emit failed("OpenCV 错误："+QString::fromUtf8(e.what())); }
    catch(const std::exception& e) { emit failed("识别错误："+QString::fromUtf8(e.what())); }
    catch(...) { emit failed("识别线程发生未知错误"); }
    emit finished();
}
void MotionWorker::runLoop() {
    PoseDetector detector;
    emit stateChanged("正在加载模型...");
    if(!detector.load(options_.modelPath.toStdString())) {
        emit failed("模型加载失败："+options_.modelPath);return;
    }
    if(control_->stop.load()) return;
    cv::VideoCapture cap;
    const bool video=!options_.videoPath.isEmpty();
    if(video) cap.open(options_.videoPath.toStdString());
    else cap.open(options_.cameraIndex);
    if(!cap.isOpened()) {emit failed(video?"无法打开测试视频":"无法打开摄像头，请检查编号、权限或占用");return;}
    if(!video) {cap.set(cv::CAP_PROP_FRAME_WIDTH,640);cap.set(cv::CAP_PROP_FRAME_HEIGHT,480);}
    Squat squat; JumpingJack jack; Cycling cycling;
    applyDefaultExerciseConfig(squat,jack,cycling);
    ExerciseBase* active=&squat;
    if(options_.mode==Mode::JumpingJack) active=&jack;
    if(options_.mode==Mode::Cycling) active=&cycling;
    PoseSmoother smoother(0.35f,0.20f);
    int lastCount=-1, failures=0;
    bool lastValid=false;
    QElapsedTimer progressClock;progressClock.start();
    double fps=cap.get(cv::CAP_PROP_FPS);
    if(!std::isfinite(fps)||fps<1||fps>240) fps=30;
    const int interval=int(1000.0/fps);
    emit stateChanged(options_.mode==Mode::Cycling ? "请先正常蹬腿，等待自动标定" : "请先保持完整的起始站姿");
    while(!control_->stop.load()) {
        QElapsedTimer frameClock;frameClock.start();
        if(control_->reset.exchange(false)) {active->reset();smoother.reset();lastCount=-1;progressClock.restart();}
        if(video&&control_->paused.load()) {progressClock.restart();QThread::msleep(30);continue;}
        cv::Mat frame;
        if(!cap.read(frame)||frame.empty()) {
            if(video) {emit stateChanged("视频播放结束；未达标可重新开始本次测试");break;}
            if(++failures>=30) {emit failed("摄像头连续读取失败，请重新连接后重试");break;}
            QThread::msleep(30);continue;
        }
        failures=0;
        if(!video) cv::flip(frame,frame,1);
        if(!control_->paused.load()) {
            PoseLandmarks raw{};
            const bool detected=detector.detect(frame,raw);
            auto smooth=smoother.update(detected?raw:PoseLandmarks{});
            active->update(smooth);
            if(detected) drawPose(frame,smooth);
            const int count=active->count();
            if(count!=lastCount) {emit countChanged(count);lastCount=count;progressClock.restart();}
            if(active->valid()!=lastValid) {lastValid=active->valid();emit poseValidChanged(lastValid);}
            if(progressClock.elapsed()>=3000) {emit wrongMotionHint();progressClock.restart();}
        } else progressClock.restart();
        if(!control_->framePending.exchange(true)) {
            cv::Mat rgb;cv::cvtColor(frame,rgb,cv::COLOR_BGR2RGB);
            emit frameReady(QImage(rgb.data,rgb.cols,rgb.rows,int(rgb.step),QImage::Format_RGB888).copy());
        }
        if(control_->paused.load()&&!video) QThread::msleep(30);
        if(video) {
            int remain=interval-int(frameClock.elapsed());
            while(remain>0&&!control_->stop.load()) {int part=std::min(remain,20);QThread::msleep(part);remain-=part;}
        }
    }
    cap.release();
}
