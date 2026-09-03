#include "system/DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QDebug>

namespace wakeai {

DatabaseManager::DatabaseManager(const QString& dbPath)
    : dbPath_(dbPath.isEmpty() ? "wakeai.db" : dbPath) {}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::init() {
    if (db_.isOpen()) return true;
    db_ = QSqlDatabase::addDatabase("QSQLITE");
    db_.setDatabaseName(dbPath_);
    if (!db_.open()) {
        qWarning() << "[DB] open failed:" << db_.lastError().text();
        return false;
    }
    return createTables();
}

void DatabaseManager::close() {
    if (db_.isOpen()) db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

bool DatabaseManager::isOpen() const {
    return db_.isOpen();
}

bool DatabaseManager::createTables() {
    QSqlQuery q(db_);
    if (!q.exec("CREATE TABLE IF NOT EXISTS alarm_settings ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "hour INTEGER NOT NULL,"
                "minute INTEGER NOT NULL,"
                "exercise_type TEXT NOT NULL,"
                "target_count INTEGER NOT NULL,"
                "enabled INTEGER NOT NULL DEFAULT 1,"
                "theme TEXT NOT NULL DEFAULT 'default')"))
        return false;
    if (!q.exec("CREATE TABLE IF NOT EXISTS wake_records ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "date TEXT NOT NULL,"
                "alarm_time TEXT NOT NULL,"
                "exercise_type TEXT NOT NULL,"
                "target_count INTEGER NOT NULL,"
                "actual_count INTEGER NOT NULL,"
                "success INTEGER NOT NULL,"
                "completed_at TEXT NOT NULL)"))
        return false;
    if (!q.exec("CREATE TABLE IF NOT EXISTS achievements ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "achievement_id TEXT NOT NULL UNIQUE,"
                "unlocked INTEGER NOT NULL DEFAULT 1,"
                "unlock_date TEXT NOT NULL)"))
        return false;
    return true;
}

// ---------------- 闹钟设置 ----------------

bool DatabaseManager::saveAlarmSetting(const AlarmSetting& s) {
    QSqlQuery q(db_);
    // 单行配置：先清空再插入，保证只有一条
    q.exec("DELETE FROM alarm_settings");
    q.prepare("INSERT INTO alarm_settings(hour, minute, exercise_type, target_count, enabled, theme)"
              " VALUES(?,?,?,?,?,?)");
    q.addBindValue(s.hour);
    q.addBindValue(s.minute);
    q.addBindValue(s.exerciseType);
    q.addBindValue(s.targetCount);
    q.addBindValue(s.enabled ? 1 : 0);
    q.addBindValue(s.theme);
    if (!q.exec()) {
        qWarning() << "[DB] saveAlarmSetting failed:" << q.lastError().text();
        return false;
    }
    return true;
}

AlarmSetting DatabaseManager::loadSettings() const {
    AlarmSetting s;
    QSqlQuery q(db_);
    if (q.exec("SELECT hour, minute, exercise_type, target_count, enabled, theme "
               "FROM alarm_settings LIMIT 1") && q.next()) {
        s.hour          = q.value(0).toInt();
        s.minute        = q.value(1).toInt();
        s.exerciseType  = q.value(2).toString();
        s.targetCount   = q.value(3).toInt();
        s.enabled       = q.value(4).toInt() != 0;
        s.theme         = q.value(5).toString();
    }
    return s;
}

// ---------------- 唤醒记录 ----------------

bool DatabaseManager::saveWakeRecord(const WakeRecord& r) {
    QSqlQuery q(db_);
    q.prepare("INSERT INTO wake_records(date, alarm_time, exercise_type, target_count, actual_count, success, completed_at)"
              " VALUES(?,?,?,?,?,?,?)");
    q.addBindValue(r.date);
    q.addBindValue(r.alarmTime);
    q.addBindValue(r.exerciseType);
    q.addBindValue(r.targetCount);
    q.addBindValue(r.actualCount);
    q.addBindValue(r.success ? 1 : 0);
    q.addBindValue(r.completedAt);
    if (!q.exec()) {
        qWarning() << "[DB] saveWakeRecord failed:" << q.lastError().text();
        return false;
    }
    return true;
}

Statistics DatabaseManager::queryStatistics() const {
    Statistics st;
    QSqlQuery q(db_);
    // 累计成功唤醒次数
    if (q.exec("SELECT COUNT(*) FROM wake_records WHERE success=1") && q.next())
        st.totalWakeCount = q.value(0).toInt();
    // 累计动作数
    if (q.exec("SELECT COALESCE(SUM(actual_count),0) FROM wake_records WHERE success=1") && q.next())
        st.totalActions = q.value(0).toInt();
    // 连续早起天数
    st.consecutiveDays = consecutiveWakeDays();
    return st;
}

int DatabaseManager::consecutiveWakeDays() const {
    QSqlQuery q(db_);
    if (!q.exec("SELECT DISTINCT date FROM wake_records WHERE success=1 "
                "ORDER BY date DESC"))
        return 0;
    int streak = 0;
    bool first = true;
    QDate prev;
    while (q.next()) {
        QDate cur = QDate::fromString(q.value(0).toString(), "yyyy-MM-dd");
        if (!cur.isValid()) continue;
        if (first) {
            streak = 1;
            prev = cur;
            first = false;
        } else {
            if (prev.addDays(-1) == cur) { streak++; prev = cur; }
            else break;
        }
    }
    return streak;
}

// ---------------- 成就 ----------------

bool DatabaseManager::unlockAchievement(const QString& id) {
    QSqlQuery q(db_);
    q.prepare("INSERT OR IGNORE INTO achievements(achievement_id, unlocked, unlock_date)"
              " VALUES(?,1,?)");
    q.addBindValue(id);
    q.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    return q.exec();
}

bool DatabaseManager::isAchievementUnlocked(const QString& id) const {
    QSqlQuery q(db_);
    q.prepare("SELECT COUNT(*) FROM achievements WHERE achievement_id=?");
    q.addBindValue(id);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

QVector<QString> DatabaseManager::unlockedAchievements() const {
    QVector<QString> result;
    QSqlQuery q(db_);
    if (q.exec("SELECT achievement_id FROM achievements ORDER BY id")) {
        while (q.next()) result.append(q.value(0).toString());
    }
    return result;
}

} // namespace wakeai
