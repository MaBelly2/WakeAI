#include "database/DatabaseManager.h"
#include <cstdio>

// 数据模块功能自测：编译运行后应能创建库、写记录、读统计
int main() {
    // 用测试库文件，避免污染正式库
    wakeai::DatabaseManager db("test_wakeai.db");
    if (!db.init()) {
        std::printf("[FAIL] 数据库初始化失败\n");
        return -1;
    }
    std::printf("[OK] 数据库初始化并建表成功\n");

    // 写闹钟配置
    if (db.addAlarm("squat", 15, "07:00"))
        std::printf("[OK] addAlarm: 深蹲 15 次 @ 07:00\n");
    else
        std::printf("[FAIL] addAlarm\n");

    // 记录两次早起
    if (db.recordWake("squat", 15, 90))
        std::printf("[OK] recordWake #1\n");
    else
        std::printf("[FAIL] recordWake #1\n");
    if (db.recordWake("jumping_jack", 25, 85))
        std::printf("[OK] recordWake #2\n");
    else
        std::printf("[FAIL] recordWake #2\n");

    // 成就
    if (db.addAchievement("连续早起 3 天", "2026-08-27"))
        std::printf("[OK] addAchievement\n");
    else
        std::printf("[FAIL] addAchievement\n");

    // 读统计
    std::printf("[INFO] totalWakeCount  = %d\n", db.totalWakeCount());
    std::printf("[INFO] consecutiveDays = %d\n", db.consecutiveWakeDays());
    std::printf("[INFO] achievements    = %zu\n", db.achievements().size());
    for (const auto& a : db.achievements())
        std::printf("        - %s\n", a.c_str());

    std::printf("== 数据模块测试完成 ==\n");
    return 0;
}
