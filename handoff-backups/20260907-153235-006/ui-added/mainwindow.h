#pragma once

#include <QMainWindow>
#include <QTime>
#include <QTimer>
#include <QThread>
#include <QSoundEffect>
#include <QSettings>
#include <QDateTime>

#include "motionworker.h"
namespace Ui {
class MainWindow;          // ← ① 前置声明，放 class MainWindow 之前
}
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void checkTime();
    void on_btnSetAlarm_clicked();   // 设置新闹钟
    void on_btnRecords_clicked();    // 首页 -> 记录页
    void on_btnStart_clicked();      // 响铃页 -> 准备页
    void on_btnSnooze_clicked();     // 稍后提醒（+5分钟）
    void on_btnBegin_clicked();      // 准备页 -> 进行页，启动识别
    void on_btnPause_clicked();      // 暂停/继续
    void on_btnFinish_clicked();     // 完成任务（达标后才有效）
    void on_btnHistory_clicked();    // 完成页 -> 记录页
    void on_btnHome_clicked();       // 完成页 -> 首页
    void on_btnRecordBack_clicked(); // 记录页 -> 首页
    void onCountChanged(int count);
    void onWrongMotionHint();

private:
    enum State { Idle, Ringing, Preparing, Exercising, Done };
    State state = Idle;

    Ui::MainWindow *ui;

    QTimer *clockTimer = nullptr;
    QTime  alarmTime;
    bool   alarmTriggered_ = false;
    int    targetCount_ = 20;
    bool   paused_ = false;

    QSoundEffect *ringSound = nullptr;
    QThread      *workerThread = nullptr;
    MotionWorker *worker = nullptr;

    QString modeName();        // 根据 comboMode 返回中文动作名
    void    startRing();       // 到点响铃并切到响铃页
    void    stopWorker();      // 停止识别线程（可再启动）
    void    appendRecord();    // 写入历史记录
};