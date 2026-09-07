#pragma once
#include <QObject>
#include <QString>
#include <QImage>
#include <atomic>
#include <memory>
struct MotionControl {
    std::atomic<bool> stop{false}, paused{false}, reset{false}, framePending{false};
};
class MotionWorker : public QObject {
    Q_OBJECT
public:
    enum class Mode { Squat, JumpingJack, Cycling };
    struct Options {
        QString modelPath, videoPath;
        int cameraIndex=0;
        Mode mode=Mode::Squat;
    };
    MotionWorker(Options options, std::shared_ptr<MotionControl> control);
public slots:
    void start();
signals:
    void countChanged(int count);
    void stateChanged(const QString& message);
    void poseValidChanged(bool valid);
    void frameReady(const QImage& image);
    void wrongMotionHint();
    void failed(const QString& message);
    void finished();
private:
    void runLoop();
    Options options_;
    std::shared_ptr<MotionControl> control_;
};
