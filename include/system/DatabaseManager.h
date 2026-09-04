#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace wakeai {

// 闹钟设置（对应表 alarm_settings）
struct AlarmSetting {
    int hour = 7;
    int minute = 0;
    QString exerciseType = "squat";   // squat / jumping_jack / cycling
    int targetCount = 15;
    bool enabled = true;
    QString theme = "default";
};

// 一次唤醒挑战记录（对应表 wake_records）
struct WakeRecord {
    QString date;          // 日期 yyyy-MM-dd
    QString alarmTime;     // 闹钟时刻 HH:mm
    QString exerciseType;
    int targetCount = 0;
    int actualCount = 0;
    bool success = false;
    QString completedAt;   // 完成时刻 yyyy-MM-dd HH:mm:ss
};

// 统计数据（供 UI / AchievementEngine 使用）
struct Statistics {
    int totalWakeCount = 0;   // 累计成功唤醒次数
    int consecutiveDays = 0;  // 连续早起天数
    int totalActions = 0;     // 累计完成动作数
};

// SQLite 数据持久化（Qt SQL 驱动）
// 表：alarm_settings / wake_records / achievements
class DatabaseManager {
public:
    explicit DatabaseManager(const QString& dbPath = QString());
    ~DatabaseManager();

    bool init();            // 打开/创建数据库并建表
    void close();
    bool isOpen() const;

    // ---- 闹钟设置 ----
    bool saveAlarmSetting(const AlarmSetting& s);
    AlarmSetting loadSettings() const;

    // ---- 唤醒记录 ----
    bool saveWakeRecord(const WakeRecord& r);
    Statistics queryStatistics() const;
    int consecutiveWakeDays() const;

    // ---- 成就 ----
    bool unlockAchievement(const QString& id);
    bool isAchievementUnlocked(const QString& id) const;
    QVector<QString> unlockedAchievements() const;

private:
    bool createTables();
    QString dbPath_;
    QSqlDatabase db_;
};

} // namespace wakeai
