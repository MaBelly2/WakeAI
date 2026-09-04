#pragma once

#include <QString>
#include <QVector>
#include "system/DatabaseManager.h"

namespace wakeai {

// 一条成就规则
struct AchievementRule {
    QString id;
    QString name;
    QString description;
};

// 成就引擎：根据统计自动解锁成就。
// 规则独立判断，不重复写入；UI 只负责展示，不写规则。
class AchievementEngine {
public:
    explicit AchievementEngine(DatabaseManager& db);

    // 传入最新统计，检查并解锁新成就；返回本次新解锁的成就 id 列表
    QVector<QString> checkAndUnlock(const Statistics& stats);

    // 全部成就规则（第一版 5 个）
    const QVector<AchievementRule>& rules() const;

private:
    DatabaseManager& db_;
    QVector<AchievementRule> rules_;
};

} // namespace wakeai
