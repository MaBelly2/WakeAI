#pragma once
#include <QMainWindow>
#include <QThread>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QDateTime>
#include <memory>
#include "motionworker.h"
#include "exercise/WorkoutSession.h"
#include "system/AlarmManager.h"
#include "system/DatabaseManager.h"
#include "system/AchievementEngine.h"
#include "ringtonemanager.h"
namespace Ui { class MainWindow; }
struct StartupOptions { QString modelPath,videoPath; int cameraIndex=0; bool testMode=false; };
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(StartupOptions options={},QWidget* parent=nullptr);
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent* event) override;
private slots:
    void on_btnSetAlarm_clicked();
    void on_btnRecords_clicked();
    void on_btnStart_clicked();
    void on_btnSnooze_clicked();
    void on_btnBegin_clicked();
    void on_btnPause_clicked();
    void on_btnFinish_clicked();
    void on_btnHistory_clicked();
    void on_btnHome_clicked();
    void on_btnRecordBack_clicked();
    void on_btnChooseRingtone_clicked();
private:
    enum class State { Idle,Ringing,Preparing,Exercising,Done };
    void armAlarm(int testSeconds=0);
    void startRing();
    void stopWorker();
    void updateCount(int count);
    void refreshRecords();
    void showAchievements();
    void switchPage(int index);
    void updateRingtoneLabel();
    bool prepareAlarmAudio();
    bool savePending();
    QString findModelPath() const;
    QString modeName() const;
    Ui::MainWindow* ui;
    StartupOptions options_;
    wakeai::DatabaseManager db_;
    wakeai::AchievementEngine achievements_;
    wakeai::AlarmManager alarm_;
    wakeai::AlarmSetting setting_;
    wakeai::WorkoutSession session_;
    wakeai::WakeRecord pendingRecord_;
    RingtoneManager ringtones_;
    QMediaPlayer player_;
    QAudioOutput audio_;
    QThread* thread_=nullptr;
    std::shared_ptr<MotionControl> control_;
    State state_=State::Idle;
    QString sessionId_;
    QDateTime scheduled_;
    bool pending_=false,paused_=false,poseValid_=false,closing_=false,closeApproved_=false;
};
