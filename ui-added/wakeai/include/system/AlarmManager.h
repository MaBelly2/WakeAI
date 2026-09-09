#pragma once
#include <QObject>
#include <QDateTime>
#include <QTimer>
namespace wakeai {
class AlarmManager : public QObject {
    Q_OBJECT
public:
    explicit AlarmManager(QObject* parent = nullptr);
    ~AlarmManager() override = default;
    void setAlarm(int hour, int minute);
    void setAlarmDateTime(const QDateTime& when);
    void setChallenge(int targetCount);
    void setTestAlarmInSeconds(int seconds);
    void enable();
    void disable(); // Does not bypass a ringing challenge.
    bool snooze(int seconds = 300);
    bool isEnabled() const { return enabled_; }
    bool isRinging() const { return ringing_; }
    void setChallengeCompleted(bool value) { completed_ = value; }
    bool isChallengeCompleted() const { return completed_; }
    QDateTime nextTrigger() const { return next_; }
public slots:
    bool stop();
signals:
    void alarmTriggered();
    void alarmStopped();
private:
    void checkTime();
    void scheduleNext();
    QTimer timer_;
    QDateTime next_;
    QTime time_{7, 0};
    bool enabled_ = false, ringing_ = false, completed_ = false;
};
}
