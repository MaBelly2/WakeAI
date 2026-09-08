#pragma once
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QDate>
namespace wakeai {
struct AlarmSetting {
    int hour = 7, minute = 0;
    QString exerciseType = "squat";
    int targetCount = 15;
    bool enabled = false;
    QString theme = "default";
    QString ringtoneId = "builtin:classic";
    double volume = 0.85;
};
struct WakeRecord {
    QString date, alarmTime, exerciseType;
    int targetCount = 0, actualCount = 0;
    bool success = false;
    QString completedAt;
    QString sessionId; // Optional additive field; old callers remain valid.
};
struct Statistics { int totalWakeCount = 0, consecutiveDays = 0, totalActions = 0; };
// Use each instance only from the thread that created/initialized it.
class DatabaseManager {
public:
    explicit DatabaseManager(const QString& dbPath = QString());
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    bool init();
    void close();
    bool isOpen() const;
    bool saveAlarmSetting(const AlarmSetting& s);
    AlarmSetting loadSettings() const;
    bool saveWakeRecord(const WakeRecord& r);
    QVector<WakeRecord> recentRecords(int limit = 100) const;
    Statistics queryStatistics() const;
    int consecutiveWakeDays() const;
    int consecutiveWakeDaysAt(const QDate& today) const;
    bool unlockAchievement(const QString& id);
    bool isAchievementUnlocked(const QString& id) const;
    QVector<QString> unlockedAchievements() const;
    QString lastError() const { return error_; }
    QString databasePath() const { return dbPath_; }
private:
    bool createTables();
    QString dbPath_, connectionName_;
    mutable QString error_;
    QSqlDatabase db_;
};
}
