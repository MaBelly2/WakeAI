#include "system/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QStringList>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>
namespace wakeai {
DatabaseManager::DatabaseManager(const QString& path) : dbPath_(path),
    connectionName_("wakeai_" + QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    if (dbPath_.isEmpty())
        dbPath_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wakeai.db";
}
DatabaseManager::~DatabaseManager() { close(); }
bool DatabaseManager::init() {
    if (isOpen()) return true;
    error_.clear();
    if (dbPath_ != ":memory:" && !QDir().mkpath(QFileInfo(dbPath_).absolutePath())) {
        error_ = "无法创建数据库目录"; return false;
    }
    if (!db_.isValid()) db_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    db_.setDatabaseName(dbPath_);
    if (!db_.open()) { error_ = db_.lastError().text(); return false; }
    if (!createTables()) { db_.close(); return false; }
    return true;
}
void DatabaseManager::close() {
    if (db_.isValid()) db_.close();
    db_ = QSqlDatabase();
    if (QSqlDatabase::contains(connectionName_)) QSqlDatabase::removeDatabase(connectionName_);
}
bool DatabaseManager::isOpen() const { return db_.isValid() && db_.isOpen(); }
bool DatabaseManager::createTables() {
    QSqlQuery q(db_);
    const QStringList sql = {
        "CREATE TABLE IF NOT EXISTS alarm_settings (id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "hour INTEGER NOT NULL,minute INTEGER NOT NULL,exercise_type TEXT NOT NULL,"
        "target_count INTEGER NOT NULL,enabled INTEGER NOT NULL DEFAULT 0,theme TEXT NOT NULL DEFAULT 'default')",
        "CREATE TABLE IF NOT EXISTS wake_records (id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date TEXT NOT NULL,alarm_time TEXT NOT NULL,exercise_type TEXT NOT NULL,"
        "target_count INTEGER NOT NULL,actual_count INTEGER NOT NULL,success INTEGER NOT NULL,completed_at TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS achievements (id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "achievement_id TEXT NOT NULL UNIQUE,unlocked INTEGER NOT NULL DEFAULT 1,unlock_date TEXT NOT NULL)"
    };
    for (const auto& s : sql) if (!q.exec(s)) { error_ = q.lastError().text(); return false; }
    bool hasSession = false;
    if (!q.exec("PRAGMA table_info(wake_records)")) { error_ = q.lastError().text(); return false; }
    while (q.next()) if (q.value(1).toString() == "session_id") hasSession = true;
    q.finish();
    if (!hasSession && !q.exec("ALTER TABLE wake_records ADD COLUMN session_id TEXT NOT NULL DEFAULT ''")) {
        error_ = q.lastError().text(); return false;
    }
    if (!q.exec("CREATE UNIQUE INDEX IF NOT EXISTS wake_record_session ON wake_records(session_id) WHERE session_id <> ''")) {
        error_ = q.lastError().text(); return false;
    }
    return true;
}
bool DatabaseManager::saveAlarmSetting(const AlarmSetting& s) {
    error_.clear();
    if (!isOpen() || s.hour < 0 || s.hour > 23 || s.minute < 0 || s.minute > 59 || s.targetCount < 1) {
        error_ = "数据库未打开或闹钟参数无效"; return false;
    }
    if (!db_.transaction()) { error_ = db_.lastError().text(); return false; }
    QSqlQuery q(db_);
    bool ok = q.exec("DELETE FROM alarm_settings");
    if (ok) {
        q.prepare("INSERT INTO alarm_settings(hour,minute,exercise_type,target_count,enabled,theme) VALUES(?,?,?,?,?,?)");
        q.addBindValue(s.hour); q.addBindValue(s.minute); q.addBindValue(s.exerciseType);
        q.addBindValue(s.targetCount); q.addBindValue(s.enabled ? 1 : 0); q.addBindValue(s.theme);
        ok = q.exec();
    }
    if (!ok) { error_ = q.lastError().text(); db_.rollback(); return false; }
    if (!db_.commit()) { error_ = db_.lastError().text(); db_.rollback(); return false; }
    return true;
}
AlarmSetting DatabaseManager::loadSettings() const {
    AlarmSetting s;
    if (!isOpen()) return s;
    QSqlQuery q(db_);
    if (!q.exec("SELECT hour,minute,exercise_type,target_count,enabled,theme FROM alarm_settings ORDER BY id DESC LIMIT 1")) {
        error_ = q.lastError().text(); return s;
    }
    if (q.next()) {
        s.hour = q.value(0).toInt(); s.minute = q.value(1).toInt(); s.exerciseType = q.value(2).toString();
        s.targetCount = q.value(3).toInt(); s.enabled = q.value(4).toBool(); s.theme = q.value(5).toString();
    }
    return s;
}
bool DatabaseManager::saveWakeRecord(const WakeRecord& r) {
    error_.clear();
    if (!isOpen()) { error_ = "数据库未打开"; return false; }
    QSqlQuery q(db_);
    q.prepare("INSERT INTO wake_records(date,alarm_time,exercise_type,target_count,actual_count,success,completed_at,session_id) "
              "VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(session_id) WHERE session_id <> '' DO NOTHING");
    q.addBindValue(r.date); q.addBindValue(r.alarmTime.isNull() ? QStringLiteral("") : r.alarmTime);
    q.addBindValue(r.exerciseType); q.addBindValue(r.targetCount); q.addBindValue(r.actualCount);
    q.addBindValue(r.success ? 1 : 0); q.addBindValue(r.completedAt.isNull() ? QStringLiteral("") : r.completedAt);
    q.addBindValue(r.sessionId.isNull() ? QStringLiteral("") : r.sessionId);
    if (!q.exec()) { error_ = q.lastError().text(); return false; }
    return true;
}
QVector<WakeRecord> DatabaseManager::recentRecords(int limit) const {
    QVector<WakeRecord> result;
    if (!isOpen()) return result;
    QSqlQuery q(db_);
    q.prepare("SELECT date,alarm_time,exercise_type,target_count,actual_count,success,completed_at,session_id "
              "FROM wake_records ORDER BY completed_at DESC,id DESC LIMIT ?");
    q.addBindValue(std::clamp(limit, 1, 1000));
    if (!q.exec()) { error_ = q.lastError().text(); return result; }
    while (q.next()) {
        WakeRecord r;
        r.date=q.value(0).toString(); r.alarmTime=q.value(1).toString(); r.exerciseType=q.value(2).toString();
        r.targetCount=q.value(3).toInt(); r.actualCount=q.value(4).toInt(); r.success=q.value(5).toBool();
        r.completedAt=q.value(6).toString(); r.sessionId=q.value(7).toString(); result.append(r);
    }
    return result;
}
Statistics DatabaseManager::queryStatistics() const {
    Statistics s;
    if (!isOpen()) return s;
    QSqlQuery q(db_);
    if (q.exec("SELECT COUNT(*),COALESCE(SUM(actual_count),0) FROM wake_records WHERE success=1") && q.next()) {
        s.totalWakeCount=q.value(0).toInt(); s.totalActions=q.value(1).toInt();
    } else error_=q.lastError().text();
    s.consecutiveDays=consecutiveWakeDays(); return s;
}
int DatabaseManager::consecutiveWakeDays() const { return consecutiveWakeDaysAt(QDate::currentDate()); }
int DatabaseManager::consecutiveWakeDaysAt(const QDate& today) const {
    if (!isOpen() || !today.isValid()) return 0;
    QSqlQuery q(db_);
    q.prepare("SELECT DISTINCT date FROM wake_records WHERE success=1 AND date<=? ORDER BY date DESC");
    q.addBindValue(today.toString("yyyy-MM-dd"));
    if (!q.exec()) { error_=q.lastError().text(); return 0; }
    int count=0; QDate previous;
    while(q.next()) {
        auto d=QDate::fromString(q.value(0).toString(),"yyyy-MM-dd");
        if (!d.isValid()) continue;
        if (!count) {
            if (d != today && d != today.addDays(-1)) return 0;
        } else if (d != previous.addDays(-1)) break;
        ++count; previous=d;
    }
    return count;
}
bool DatabaseManager::unlockAchievement(const QString& id) {
    if (!isOpen()) { error_="数据库未打开"; return false; }
    QSqlQuery q(db_);
    q.prepare("INSERT INTO achievements(achievement_id,unlocked,unlock_date) VALUES(?,1,?) "
              "ON CONFLICT(achievement_id) DO UPDATE SET unlocked=1");
    q.addBindValue(id); q.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    if (!q.exec()) { error_=q.lastError().text(); return false; } return true;
}
bool DatabaseManager::isAchievementUnlocked(const QString& id) const {
    if (!isOpen()) return false;
    QSqlQuery q(db_); q.prepare("SELECT 1 FROM achievements WHERE achievement_id=? AND unlocked=1");
    q.addBindValue(id); if(!q.exec()) {error_=q.lastError().text();return false;} return q.next();
}
QVector<QString> DatabaseManager::unlockedAchievements() const {
    QVector<QString> ids; if(!isOpen()) return ids;
    QSqlQuery q(db_);
    if(q.exec("SELECT achievement_id FROM achievements WHERE unlocked=1 ORDER BY id"))
        while(q.next()) ids.append(q.value(0).toString());
    else error_=q.lastError().text();
    return ids;
}
}
