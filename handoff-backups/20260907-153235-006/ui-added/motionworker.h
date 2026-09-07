#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <QImage>
namespace wakeai { class ExerciseBase; }

class MotionWorker : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Squat, JumpingJack, Cycling };

    explicit MotionWorker(QObject *parent = nullptr);
    ~MotionWorker();

    void setModelPath(const QString &path);
    void setMode(Mode m);

public slots:
    void start();     // 线程启动时调用（内部是阻塞循环）
    void stop();      // 请求停止，线程随后退出
    void pause();     // 暂停计数（线程不退出）
    void resume();    // 继续计数
    void reset();     // 重置当前动作计数

signals:
    void countChanged(int count);
    void stateChanged(const QString &s);
    void poseValidChanged(bool ok);
    void wrongMotionHint();
    void frameReady(const QImage &image);
    void finished();

private:
    void runLoop();
    std::atomic<bool> stopFlag_{false};
    std::atomic<bool> paused_{false};
    QString modelPath_;
    Mode mode_ = Mode::Squat;
    wakeai::ExerciseBase *active_ = nullptr;
};