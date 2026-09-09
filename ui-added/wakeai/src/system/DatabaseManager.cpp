#include "system/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QStringList>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSet>
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
        "target_count INTEGER NOT NULL,enabled INTEGER NOT NULL DEFAULT 0,theme TEXT NOT NULL DEFAULT 'default',"
        "ringtone_id TEXT NOT NULL DEFAULT 'builtin:classic',volume REAL NOT NULL DEFAULT 0.85)",
        "CREATE TABLE IF NOT EXISTS wake_records (id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date TEXT NOT NULL,alarm_time TEXT NOT NULL,exercise_type TEXT NOT NULL,"
        "target_count INTEGER NOT NULL,actual_count INTEGER NOT NULL,success INTEGER NOT NULL,completed_at TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS achievements (id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "achievement_id TEXT NOT NULL UNIQUE,unlocked INTEGER NOT NULL DEFAULT 1,unlock_date TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS alarms (id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "hour INTEGER NOT NULL,minute INTEGER NOT NULL,label TEXT NOT NULL DEFAULT '起床闹钟',"
        "exercise_type TEXT NOT NULL,target_count INTEGER NOT NULL,enabled INTEGER NOT NULL DEFAULT 1,"
        "repeat_mask INTEGER NOT NULL DEFAULT 0,snooze_minutes INTEGER NOT NULL DEFAULT 5,"
        "ringtone_id TEXT NOT NULL DEFAULT 'builtin:classic',volume REAL NOT NULL DEFAULT 0.85,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)",
        "CREATE TABLE IF NOT EXISTS app_meta (key TEXT PRIMARY KEY,value TEXT NOT NULL)"
    };
    for (const auto& s : sql) if (!q.exec(s)) { error_ = q.lastError().text(); return false; }
    QSet<QString> alarmColumns;
    if (!q.exec("PRAGMA table_info(alarm_settings)")) { error_ = q.lastError().text(); return false; }
    while (q.next()) alarmColumns.insert(q.value(1).toString());
    q.finish();
    if (!alarmColumns.contains("ringtone_id")
        && !q.exec("ALTER TABLE alarm_settings ADD COLUMN ringtone_id TEXT NOT NULL DEFAULT 'builtin:classic'")) {
        error_ = q.lastError().text(); return false;
    }
    if (!alarmColumns.contains("volume")
        && !q.exec("ALTER TABLE alarm_settings ADD COLUMN volume REAL NOT NULL DEFAULT 0.85")) {
        error_ = q.lastError().text(); return false;
    }

    // V3 migration: import the former single alarm once. The marker prevents a
    // deliberately emptied multi-alarm list from being recreated on restart.
    if (!q.exec("SELECT value FROM app_meta WHERE key='alarms_migrated_v3'")) {
        error_ = q.lastError().text(); return false;
    }
    const bool migrated = q.next();
    q.finish();
    if (!migrated) {
        if (!db_.transaction()) { error_ = db_.lastError().text(); return false; }
        QSqlQuery legacy(db_);
        bool ok = legacy.exec("SELECT hour,minute,exercise_type,target_count,enabled,ringtone_id,volume "
                              "FROM alarm_settings ORDER BY id DESC LIMIT 1");
        if (ok && legacy.next()) {
            const QVariant hour = legacy.value(0);
            const QVariant minute = legacy.value(1);
            const QVariant exerciseType = legacy.value(2);
            const QVariant targetCount = legacy.value(3);
            const QVariant enabled = legacy.value(4);
            const QVariant ringtoneId = legacy.value(5);
            const QVariant volume = legacy.value(6);
            legacy.finish();
            QSqlQuery insert(db_);
            insert.prepare("INSERT INTO alarms(hour,minute,label,exercise_type,target_count,enabled,repeat_mask,"
                           "snooze_minutes,ringtone_id,volume) VALUES(?,?,'起床闹钟',?,?,?,0,5,?,?)");
            insert.addBindValue(hour); insert.addBindValue(minute);
            insert.addBindValue(exerciseType); insert.addBindValue(targetCount);
            insert.addBindValue(enabled); insert.addBindValue(ringtoneId);
            insert.addBindValue(volume);
            ok = insert.exec();
            if (!ok) error_ = insert.lastError().text();
        } else if (!ok) {
            error_ = legacy.lastError().text();
        }
        legacy.finish();
        if (ok) {
            QSqlQuery marker(db_);
            ok = marker.exec("INSERT INTO app_meta(key,value) VALUES('alarms_migrated_v3','1')");
            if (!ok) error_ = marker.lastError().text();
        }
        if (!ok || !db_.commit()) {
            if (error_.isEmpty()) error_ = db_.lastError().text();
            db_.rollback(); return false;
        }
    }

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
    if (!isOpen() || s.hour < 0 || s.hour > 23 || s.minute < 0 || s.minute > 59
        || s.targetCount < 1 || s.ringtoneId.trimmed().isEmpty() || s.volume < 0.0 || s.volume > 1.0) {
        error_ = "数据库未打开或闹钟参数无效"; return false;
    }
    if (!db_.transaction()) { error_ = db_.lastError().text(); return false; }
    QSqlQuery q(db_);
    bool ok = q.exec("DELETE FROM alarm_settings");
    if (ok) {
        q.prepare("INSERT INTO alarm_settings(hour,minute,exercise_type,target_count,enabled,theme,ringtone_id,volume) "
                  "VALUES(?,?,?,?,?,?,?,?)");
        q.addBindValue(s.hour); q.addBindValue(s.minute); q.addBindValue(s.exerciseType);
        q.addBindValue(s.targetCount); q.addBindValue(s.enabled ? 1 : 0); q.addBindValue(s.theme);
        q.addBindValue(s.ringtoneId); q.addBindValue(s.volume);
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
    if (!q.exec("SELECT hour,minute,exercise_type,target_count,enabled,theme,ringtone_id,volume "
                "FROM alarm_settings ORDER BY id DESC LIMIT 1")) {
        error_ = q.lastError().text(); return s;
    }
    if (q.next()) {
        s.hour = q.value(0).toInt(); s.minute = q.value(1).toInt(); s.exerciseType = q.value(2).toString();
        s.targetCount = q.value(3).toInt(); s.enabled = q.value(4).toBool(); s.theme = q.value(5).toString();
        s.ringtoneId = q.value(6).toString(); s.volume = qBound(0.0, q.value(7).toDouble(), 1.0);
    }
    return s;
}
qint64 DatabaseManager::saveAlarm(const AlarmSetting& alarm) {
    error_.clear();
    if (!isOpen() || alarm.hour < 0 || alarm.hour > 23 || alarm.minute < 0 || alarm.minute > 59
        || alarm.label.trimmed().isEmpty() || alarm.targetCount < 1 || alarm.targetCount > 1000
        || alarm.repeatMask < 0 || alarm.repeatMask > 127
        || alarm.snoozeMinutes < 1 || alarm.snoozeMinutes > 60
        || alarm.ringtoneId.trimmed().isEmpty() || alarm.volume < 0.0 || alarm.volume > 1.0) {
        error_ = "数据库未打开或闹钟参数无效"; return -1;
    }
    QSqlQuery q(db_);
    if (alarm.id < 0) {
        q.prepare("INSERT INTO alarms(hour,minute,label,exercise_type,target_count,enabled,repeat_mask,"
                  "snooze_minutes,ringtone_id,volume) VALUES(?,?,?,?,?,?,?,?,?,?)");
    } else {
        q.prepare("UPDATE alarms SET hour=?,minute=?,label=?,exercise_type=?,target_count=?,enabled=?,"
                  "repeat_mask=?,snooze_minutes=?,ringtone_id=?,volume=? WHERE id=?");
    }
    q.addBindValue(alarm.hour); q.addBindValue(alarm.minute); q.addBindValue(alarm.label.trimmed().left(40));
    q.addBindValue(alarm.exerciseType); q.addBindValue(alarm.targetCount); q.addBindValue(alarm.enabled ? 1 : 0);
    q.addBindValue(alarm.repeatMask); q.addBindValue(alarm.snoozeMinutes);
    q.addBindValue(alarm.ringtoneId); q.addBindValue(alarm.volume);
    if (alarm.id >= 0) q.addBindValue(alarm.id);
    if (!q.exec()) { error_ = q.lastError().text(); return -1; }
    if (alarm.id >= 0 && q.numRowsAffected() == 0) {
        error_ = "要修改的闹钟不存在"; return -1;
    }
    return alarm.id >= 0 ? alarm.id : q.lastInsertId().toLongLong();
}
AlarmSetting DatabaseManager::alarm(qint64 id) const {
    AlarmSetting result; result.id = -1;
    if (!isOpen() || id < 0) return result;
    QSqlQuery q(db_);
    q.prepare("SELECT id,hour,minute,label,exercise_type,target_count,enabled,repeat_mask,snooze_minutes,"
              "ringtone_id,volume FROM alarms WHERE id=?");
    q.addBindValue(id);
    if (!q.exec()) { error_ = q.lastError().text(); return result; }
    if (q.next()) {
        result.id=q.value(0).toLongLong(); result.hour=q.value(1).toInt(); result.minute=q.value(2).toInt();
        result.label=q.value(3).toString(); result.exerciseType=q.value(4).toString();
        result.targetCount=q.value(5).toInt(); result.enabled=q.value(6).toBool();
        result.repeatMask=q.value(7).toInt(); result.snoozeMinutes=q.value(8).toInt();
        result.ringtoneId=q.value(9).toString(); result.volume=qBound(0.0,q.value(10).toDouble(),1.0);
    }
    return result;
}
QVector<AlarmSetting> DatabaseManager::alarms() const {
    QVector<AlarmSetting> result;
    if (!isOpen()) return result;
    QSqlQuery q(db_);
    if (!q.exec("SELECT id,hour,minute,label,exercise_type,target_count,enabled,repeat_mask,snooze_minutes,"
                "ringtone_id,volume FROM alarms ORDER BY hour,minute,id")) {
        error_ = q.lastError().text(); return result;
    }
    while (q.next()) {
        AlarmSetting alarm;
        alarm.id=q.value(0).toLongLong(); alarm.hour=q.value(1).toInt(); alarm.minute=q.value(2).toInt();
        alarm.label=q.value(3).toString(); alarm.exerciseType=q.value(4).toString();
        alarm.targetCount=q.value(5).toInt(); alarm.enabled=q.value(6).toBool();
        alarm.repeatMask=q.value(7).toInt(); alarm.snoozeMinutes=q.value(8).toInt();
        alarm.ringtoneId=q.value(9).toString(); alarm.volume=qBound(0.0,q.value(10).toDouble(),1.0);
        result.append(alarm);
    }
    return result;
}
bool DatabaseManager::setAlarmEnabled(qint64 id, bool enabled) {
    error_.clear();
    if (!isOpen() || id < 0) { error_ = "数据库未打开或闹钟编号无效"; return false; }
    QSqlQuery q(db_); q.prepare("UPDATE alarms SET enabled=? WHERE id=?");
    q.addBindValue(enabled ? 1 : 0); q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected()==0) {
        error_ = q.lastError().text().isEmpty() ? "要修改的闹钟不存在" : q.lastError().text(); return false;
    }
    return true;
}
bool DatabaseManager::deleteAlarm(qint64 id) {
    error_.clear();
    if (!isOpen() || id < 0) { error_ = "数据库未打开或闹钟编号无效"; return false; }
    QSqlQuery q(db_); q.prepare("DELETE FROM alarms WHERE id=?"); q.addBindValue(id);
    if (!q.exec()) { error_ = q.lastError().text(); return false; }
    if (q.numRowsAffected() == 0) { error_ = "要删除的闹钟不存在"; return false; }
    return true;
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
