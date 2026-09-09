#pragma once

#include <QAudioOutput>
#include <QDateTime>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QThread>
#include <memory>

#include "exercise/WorkoutSession.h"
#include "motionworker.h"
#include "ringtonemanager.h"
#include "system/AchievementEngine.h"
#include "system/AlarmManager.h"
#include "system/DatabaseManager.h"

namespace Ui { class MainWindow; }

struct StartupOptions {
    QString modelPath;
    QString videoPath;
    int cameraIndex = 0;
    bool testMode = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(StartupOptions options = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_btnSetAlarm_clicked();
    void on_btnManageAlarms_clicked();
    void on_btnAlarmBack_clicked();
    void on_btnAddAlarm_clicked();
    void on_btnEditCancel_clicked();
    void on_btnEditSave_clicked();
    void on_btnChooseRingtone_clicked();
    void on_btnAchievementPrev_clicked();
    void on_btnAchievementNext_clicked();
    void on_btnRecords_clicked();
    void on_btnStart_clicked();
    void on_btnSnooze_clicked();
    void on_btnBegin_clicked();
    void on_btnPause_clicked();
    void on_btnFinish_clicked();
    void on_btnHistory_clicked();
    void on_btnHome_clicked();
    void on_btnRecordBack_clicked();

private:
    enum class State { Idle, Ringing, Preparing, Exercising, Done };
    enum Page { HomePage, AlarmListPage, AlarmEditPage, RingingPage,
                PreparingPage, ExercisePage, DonePage, RecordsPage };

    void startTestAlarm(int seconds = 10);
    void startRing();
    void scheduleNextAlarm();
    QDateTime nextOccurrence(const wakeai::AlarmSetting &alarm,
                             const QDateTime &from = QDateTime::currentDateTime()) const;
    void refreshAlarmList();
    void beginEditAlarm(qint64 id = -1);
    int editorRepeatMask() const;
    void setEditorRepeatMask(int mask);
    QString repeatText(int mask) const;
    void updateRingtoneLabel();
    void updateAchievementCard();
    void refreshDashboard();
    bool prepareAlarmAudio();
    void stopWorker();
    void updateCount(int count);
    void refreshRecords();
    void showAchievements();
    void switchPage(Page page);
    bool savePending();
    QString findModelPath() const;
    QString modeName() const;

    Ui::MainWindow *ui;
    StartupOptions options_;
    wakeai::DatabaseManager db_;
    wakeai::AchievementEngine achievements_;
    wakeai::AlarmManager alarm_;
    wakeai::AlarmSetting setting_;
    wakeai::AlarmSetting editingAlarm_;
    wakeai::WorkoutSession session_;
    wakeai::WakeRecord pendingRecord_;
    RingtoneManager ringtones_;
    QMediaPlayer player_;
    QAudioOutput audio_;
    QThread *thread_ = nullptr;
    std::shared_ptr<MotionControl> control_;
    State state_ = State::Idle;
    QString sessionId_;
    QDateTime scheduled_;
    qint64 scheduledAlarmId_ = -1;
    int achievementIndex_ = 0;
    bool pending_ = false;
    bool paused_ = false;
    bool poseValid_ = false;
    bool closing_ = false;
    bool closeApproved_ = false;
};
