#pragma once

#include <QObject>
#include <QString>

class QTimer;

namespace wakeai {

// 闹钟管理：设置、到点响铃、挑战完成后停止。
// 核心规则：挑战（WorkoutSession）未完成时不允许 stop()。
class AlarmManager : public QObject {
    Q_OBJECT
public:
    explicit AlarmManager(QObject* parent = nullptr);
    ~AlarmManager();

    // 设置闹钟时间
    void setAlarm(int hour, int minute);
    // 设置解闹动作与目标次数（由 UI 传入，供提示使用）
    void setChallenge(int targetCount);
    // 开发模式：N 秒后测试响铃（避免每次等真实时刻）
    void setTestAlarmInSeconds(int seconds);

    void enable();
    void disable();
    bool isEnabled() const;
    bool isRinging() const;

    // 挑战完成状态（由 UI / WorkoutSession 上报）
    void setChallengeCompleted(bool completed);
    bool isChallengeCompleted() const;

public slots:
    // 尝试停止响铃；仅当挑战已完成才允许，返回是否停止成功
    bool stop();

signals:
    void alarmTriggered();   // 到点响铃，UI 应切换到挑战页
    void alarmStopped();

private:
    void checkTime();
    void startRinging();
    void stopRinging();

    QTimer* timer_ = nullptr;         // 每秒检查
    QTimer* testTimer_ = nullptr;     // 测试模式倒计时

    int hour_ = 7;
    int minute_ = 0;
    int targetCount_ = 15;
    bool enabled_ = false;
    bool ringing_ = false;
    bool challengeCompleted_ = false;
};

} // namespace wakeai
