#pragma once

#include <string>
#include <vector>

struct sqlite3;  // forward declaration (defined in sqlite3.h)

namespace wakeai {

    // SQLite 数据存储封装：
    //   - 闹钟配置表 alarms
    //   - 早起/锻炼记录表 wake_records
    //   - 成就/连续打卡表 achievements
    class DatabaseManager {
    public:
        // dbPath 为空则使用默认文件 wakeai.db
        explicit DatabaseManager(const std::string& dbPath = "");
        ~DatabaseManager();

        // 打开（或创建）数据库并建表，成功返回 true
        bool init();
        void close();
        bool isOpen() const;

        // ---- 闹钟配置 ----
        bool addAlarm(const std::string& exercise, int requiredCount,
                      const std::string& alarmTime);   // alarmTime 形如 "07:00"

        // ---- 早起/锻炼记录 ----
        bool recordWake(const std::string& exercise, int count, int score);
        int  totalWakeCount() const;         // 累计成功解闹/早起次数
        int  consecutiveWakeDays() const;    // 连续早起天数

        // ---- 成就/徽章 ----
        bool addAchievement(const std::string& name, const std::string& achievedDate);
        std::vector<std::string> achievements() const;

    private:
        bool createTables();
        std::string dbPath_;
        sqlite3* db_ = nullptr;
    };

} // namespace wakeai
