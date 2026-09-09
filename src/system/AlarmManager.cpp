#include "system/AlarmManager.h"
#include <algorithm>
namespace wakeai {
AlarmManager::AlarmManager(QObject* parent) : QObject(parent) {
    timer_.setInterval(250);
    connect(&timer_, &QTimer::timeout, this, &AlarmManager::checkTime);
}
void AlarmManager::scheduleNext() {
    const auto now = QDateTime::currentDateTime();
    next_ = QDateTime(now.date(), time_);
    if (next_ <= now) next_ = next_.addDays(1);
}
void AlarmManager::setAlarm(int h, int m) {
    if (ringing_ || !QTime(h, m).isValid()) return;
    time_ = QTime(h, m); scheduleNext();
}
void AlarmManager::setAlarmDateTime(const QDateTime& when) {
    if (ringing_ || !when.isValid()) return;
    next_ = when;
    time_ = when.time();
}
void AlarmManager::setChallenge(int) { if (!ringing_) completed_ = false; }
void AlarmManager::setTestAlarmInSeconds(int seconds) {
    if (!ringing_) next_ = QDateTime::currentDateTime().addSecs(std::max(1, seconds));
}
void AlarmManager::enable() {
    enabled_ = true;
    if (!next_.isValid()) scheduleNext();
    timer_.start();
}
void AlarmManager::disable() {
    if (ringing_) return;
    enabled_ = false; next_ = {}; timer_.stop();
}
void AlarmManager::checkTime() {
    if (!enabled_ || ringing_ || !next_.isValid()) return;
    if (QDateTime::currentDateTime() < next_) return;
    next_ = {}; ringing_ = true; completed_ = false;
    emit alarmTriggered();
}
bool AlarmManager::stop() {
    if (!ringing_) return true;
    if (!completed_) return false;
    ringing_ = false; enabled_ = false; next_ = {}; timer_.stop();
    emit alarmStopped(); return true;
}
bool AlarmManager::snooze(int seconds) {
    if (!ringing_) return false;
    ringing_ = false; completed_ = false; enabled_ = true;
    next_ = QDateTime::currentDateTime().addSecs(std::max(1, seconds));
    timer_.start(); emit alarmStopped(); return true;
}
}
