#include "database/DatabaseManager.h"

#include <sqlite3.h>
#include <cstdio>
#include <ctime>

namespace wakeai {

DatabaseManager::DatabaseManager(const std::string& dbPath)
    : dbPath_(dbPath.empty() ? "wakeai.db" : dbPath) {}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::init() {
    if (db_) return true;  // already open
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
        std::fprintf(stderr, "[DB] open failed: %s\n",
                     db_ ? sqlite3_errmsg(db_) : "unknown");
        return false;
    }
    return createTables();
}

void DatabaseManager::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool DatabaseManager::isOpen() const {
    return db_ != nullptr;
}

bool DatabaseManager::createTables() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS alarms ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  exercise TEXT NOT NULL,"
        "  required_count INTEGER NOT NULL,"
        "  alarm_time TEXT NOT NULL);"

        "CREATE TABLE IF NOT EXISTS wake_records ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  wake_date TEXT NOT NULL,"
        "  exercise TEXT NOT NULL,"
        "  count INTEGER NOT NULL,"
        "  score INTEGER NOT NULL DEFAULT 0,"
        "  created_at TEXT NOT NULL);"

        "CREATE TABLE IF NOT EXISTS achievements ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  achieved_date TEXT NOT NULL);";
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "[DB] createTables: %s\n", err ? err : "?");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool DatabaseManager::addAlarm(const std::string& exercise, int requiredCount,
                               const std::string& alarmTime) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO alarms(exercise, required_count, alarm_time) VALUES(?,?,?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, exercise.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, requiredCount);
    sqlite3_bind_text(stmt, 3, alarmTime.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DatabaseManager::recordWake(const std::string& exercise, int count, int score) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO wake_records(wake_date, exercise, count, score, created_at) "
        "VALUES(?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    // today date
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char date[16] = {0};
    std::strftime(date, sizeof(date), "%Y-%m-%d", &tm);
    char created[32] = {0};
    std::strftime(created, sizeof(created), "%Y-%m-%d %H:%M:%S", &tm);

    sqlite3_bind_text(stmt, 1, date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, exercise.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, count);
    sqlite3_bind_int(stmt, 4, score);
    sqlite3_bind_text(stmt, 5, created, -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

int DatabaseManager::totalWakeCount() const {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM wake_records", -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    int n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) n = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

int DatabaseManager::consecutiveWakeDays() const {
    // 从今天向前统计连续早起天数（wake_date 唯一去重）
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT DISTINCT wake_date FROM wake_records ORDER BY wake_date DESC";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    int streak = 0;
    // 上一记录日（用秒数比较），从 0 开始表示"第一项"
    long long prev = 0;
    bool first = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* d = (const char*)sqlite3_column_text(stmt, 0);
        if (!d) break;
        // 解析成天数戳
        int y, m, day;
        if (std::sscanf(d, "%d-%d-%d", &y, &m, &day) != 3) continue;
        long long cur = (long long)y * 10000 + m * 100 + day;
        if (first) {
            streak = 1;
            prev = cur;
            first = false;
        } else {
            if (prev - cur == 1) { streak++; prev = cur; }
            else break;  // 中断
        }
    }
    sqlite3_finalize(stmt);
    return streak;
}

bool DatabaseManager::addAchievement(const std::string& name, const std::string& achievedDate) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO achievements(name, achieved_date) VALUES(?,?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, achievedDate.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<std::string> DatabaseManager::achievements() const {
    std::vector<std::string> result;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT name, achieved_date FROM achievements ORDER BY id";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* n = (const char*)sqlite3_column_text(stmt, 0);
        const char* d = (const char*)sqlite3_column_text(stmt, 1);
        result.push_back(std::string(n ? n : "") + " (" + (d ? d : "") + ")");
    }
    sqlite3_finalize(stmt);
    return result;
}

} // namespace wakeai
