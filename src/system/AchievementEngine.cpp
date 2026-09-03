#include "system/AchievementEngine.h"

#include <QDebug>

namespace wakeai {

AchievementEngine::AchievementEngine(DatabaseManager& db) : db_(db) {
    // 第一版 5 个成就（对应《后续开发推进方案》）
    rules_ = {
        { QStringLiteral("first_wake"),  QStringLiteral("初醒"),
          QStringLiteral("首次成功完成一次唤醒挑战") },
        { QStringLiteral("streak_3"),    QStringLiteral("小有坚持"),
          QStringLiteral("连续成功早起 3 天") },
        { QStringLiteral("streak_7"),    QStringLiteral("自律新星"),
          QStringLiteral("连续成功早起 7 天") },
        { QStringLiteral("actions_100"), QStringLiteral("活力达人"),
          QStringLiteral("累计完成 100 个动作") },
        { QStringLiteral("wake_30"),     QStringLiteral("晨间大师"),
          QStringLiteral("累计成功完成 30 次唤醒") },
    };
}

QVector<QString> AchievementEngine::checkAndUnlock(const Statistics& stats) {
    QVector<QString> newlyUnlocked;
    for (const AchievementRule& r : rules_) {
        bool met = false;
        if (r.id == QStringLiteral("first_wake"))
            met = stats.totalWakeCount >= 1;
        else if (r.id == QStringLiteral("streak_3"))
            met = stats.consecutiveDays >= 3;
        else if (r.id == QStringLiteral("streak_7"))
            met = stats.consecutiveDays >= 7;
        else if (r.id == QStringLiteral("actions_100"))
            met = stats.totalActions >= 100;
        else if (r.id == QStringLiteral("wake_30"))
            met = stats.totalWakeCount >= 30;

        if (met && !db_.isAchievementUnlocked(r.id)) {
            if (db_.unlockAchievement(r.id)) {
                newlyUnlocked.append(r.id);
                qInfo() << "[Achievement] 解锁:" << r.name;
            }
        }
    }
    return newlyUnlocked;
}

const QVector<AchievementRule>& AchievementEngine::rules() const {
    return rules_;
}

} // namespace wakeai
