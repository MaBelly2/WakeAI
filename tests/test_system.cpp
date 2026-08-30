#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <cstdio>

#include "system/DatabaseManager.h"
#include "system/AchievementEngine.h"
#include "system/AlarmManager.h"

using namespace wakeai;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // 用测试库，避免污染正式库
    DatabaseManager db("test_wakeai.db");
    if (!db.init()) {
        std::printf("[FAIL] 数据库初始化失败\n");
        return -1;
    }
    std::printf("[OK] 数据库初始化并建表成功（QtSql）\n");

    // ---- 闹钟设置 ----
    AlarmSetting s;
    s.hour = 7; s.minute = 0;
    s.exerciseType = "squat"; s.targetCount = 15; s.enabled = true;
    if (db.saveAlarmSetting(s))
        std::printf("[OK] saveAlarmSetting: 7:00 深蹲 15 次\n");
    AlarmSetting loaded = db.loadSettings();
    std::printf("[INFO] loadSettings: %02d:%02d %s x%d enabled=%d\n",
                loaded.hour, loaded.minute, loaded.exerciseType.toUtf8().constData(),
                loaded.targetCount, loaded.enabled ? 1 : 0);

    // ---- 唤醒记录 ----
    WakeRecord r;
    r.date = QDate::currentDate().toString("yyyy-MM-dd");
    r.alarmTime = "07:00";
    r.exerciseType = "squat"; r.targetCount = 15; r.actualCount = 15;
    r.success = true;
    r.completedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    if (db.saveWakeRecord(r)) std::printf("[OK] saveWakeRecord #1\n");
    r.exerciseType = "jumping_jack"; r.targetCount = 25; r.actualCount = 25;
    if (db.saveWakeRecord(r)) std::printf("[OK] saveWakeRecord #2\n");

    Statistics st = db.queryStatistics();
    std::printf("[INFO] statistics: 累计唤醒=%d 连续天数=%d 累计动作=%d\n",
                st.totalWakeCount, st.consecutiveDays, st.totalActions);

    // ---- 成就 ----
    AchievementEngine engine(db);
    auto newly = engine.checkAndUnlock(st);
    std::printf("[INFO] 本次解锁成就数=%d\n", (int)newly.size());
    for (const auto& id : newly)
        std::printf("        + %s\n", id.toUtf8().constData());
    auto again = engine.checkAndUnlock(st);   // 再次检查不应重复解锁
    std::printf("[%s] 重复检查不重复解锁（%d）\n",
                again.isEmpty() ? "OK" : "FAIL", (int)again.size());

    // ---- 闹钟 ----
    AlarmManager alarm;
    alarm.setAlarm(7, 0);
    alarm.setChallenge(15);
    alarm.setTestAlarmInSeconds(1);
    alarm.enable();
    // 运行事件循环 1.2 秒，让测试闹钟真正触发
    QEventLoop loop;
    QTimer::singleShot(1200, &loop, &QEventLoop::quit);
    loop.exec();
    std::printf("[%s] 测试闹钟已触发响铃（%d）\n",
                alarm.isRinging() ? "OK" : "FAIL", alarm.isRinging() ? 1 : 0);
    // 未完成挑战不允许停止
    bool stoppedBefore = alarm.stop();
    std::printf("[%s] 挑战未完成时 stop() 被拒绝（%d）\n",
                stoppedBefore ? "FAIL" : "OK", stoppedBefore ? 1 : 0);
    // 挑战完成后允许停止
    alarm.setChallengeCompleted(true);
    bool stoppedAfter = alarm.stop();
    std::printf("[%s] 挑战完成后 stop() 成功（%d）\n",
                stoppedAfter ? "OK" : "FAIL", stoppedAfter ? 1 : 0);

    std::printf("== 系统模块测试完成 ==\n");
    return 0;
}
