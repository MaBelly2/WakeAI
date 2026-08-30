#include "system/AlarmManager.h"

#include <QTimer>
#include <QTime>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wakeai {

AlarmManager::AlarmManager(QObject* parent)
    : QObject(parent) {
    // 每秒检查一次当前时间
    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &AlarmManager::checkTime);

    // 测试模式倒计时
    testTimer_ = new QTimer(this);
    testTimer_->setSingleShot(true);
    connect(testTimer_, &QTimer::timeout, this, &AlarmManager::startRinging);
}

AlarmManager::~AlarmManager() {
    stopRinging();
}

void AlarmManager::setAlarm(int hour, int minute) {
    hour_ = hour;
    minute_ = minute;
    qInfo() << "[Alarm] set" << hour << ":" << minute;
}

void AlarmManager::setChallenge(int targetCount) {
    targetCount_ = targetCount;
    challengeCompleted_ = false;
}

void AlarmManager::setTestAlarmInSeconds(int seconds) {
    testTimer_->start(seconds * 1000);
    qInfo() << "[Alarm] test alarm in" << seconds << "s";
}

void AlarmManager::enable() {
    enabled_ = true;
    if (!timer_->isActive()) timer_->start();
    qInfo() << "[Alarm] enabled";
}

void AlarmManager::disable() {
    enabled_ = false;
    if (timer_->isActive()) timer_->stop();
    stopRinging();
}

bool AlarmManager::isEnabled() const {
    return enabled_;
}

bool AlarmManager::isRinging() const {
    return ringing_;
}

void AlarmManager::setChallengeCompleted(bool completed) {
    challengeCompleted_ = completed;
}

bool AlarmManager::isChallengeCompleted() const {
    return challengeCompleted_;
}

void AlarmManager::checkTime() {
    if (!enabled_ || ringing_) return;
    QTime now = QTime::currentTime();
    if (now.hour() == hour_ && now.minute() == minute_) {
        startRinging();
    }
}

void AlarmManager::startRinging() {
    if (ringing_) return;
    ringing_ = true;
    challengeCompleted_ = false;   // 新一轮挑战未完成
#ifdef _WIN32
    Beep(880, 200);   // 简单提示音（第一版；正式音效由 UI 层补充）
#endif
    emit alarmTriggered();
    qInfo() << "[Alarm] RINGING";
}

bool AlarmManager::stop() {
    if (!ringing_) return true;
    if (!challengeCompleted_) {
        qWarning() << "[Alarm] 挑战未完成，不允许停止闹钟！";
        return false;
    }
    stopRinging();
    emit alarmStopped();
    return true;
}

void AlarmManager::stopRinging() {
    if (!ringing_) return;
    ringing_ = false;
}

} // namespace wakeai
