#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

#include "system/DatabaseManager.h"
#include "system/AchievementEngine.h"

// 简单的"系统模块成果可视化"窗口：读取数据库，展示闹钟、早起统计、成就
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 数据库路径（默认 wakeai.db，可用命令行参数指定）
    QString dbPath = (argc > 1) ? QString(argv[1]) : QString("wakeai.db");

    wakeai::DatabaseManager db(dbPath);
    db.init();

    wakeai::Statistics st = db.queryStatistics();
    wakeai::AlarmSetting set = db.loadSettings();
    wakeai::AchievementEngine engine(db);
    engine.checkAndUnlock(st);          // 确保统计满足的成就已解锁
    QVector<QString> unlocked = db.unlockedAchievements();

    QWidget w;
    w.setWindowTitle("WakeAI · 系统模块成果可视化");
    w.resize(440, 360);
    auto* lay = new QVBoxLayout(&w);

    lay->addWidget(new QLabel(QStringLiteral("<h2>WakeAI 系统模块成果</h2>")));

    lay->addWidget(new QLabel(QStringLiteral("⏰ 当前闹钟：%1:%2 &nbsp; 动作 %3 &nbsp; 目标 %4 次")
        .arg(set.hour, 2, 10, QChar('0'))
        .arg(set.minute, 2, 10, QChar('0'))
        .arg(set.exerciseType)
        .arg(set.targetCount)));

    lay->addWidget(new QLabel(QStringLiteral("🔥 连续早起：<b style='font-size:24px'>%1</b> 天").arg(st.consecutiveDays)));
    lay->addWidget(new QLabel(QStringLiteral("✅ 累计成功唤醒：%1 次").arg(st.totalWakeCount)));
    lay->addWidget(new QLabel(QStringLiteral("🏃 累计完成动作：%1 个").arg(st.totalActions)));

    lay->addWidget(new QLabel(QStringLiteral("<h3>🏅 已解锁成就</h3>")));
    if (unlocked.isEmpty()) {
        lay->addWidget(new QLabel(QStringLiteral("（暂无，完成一次唤醒挑战即可解锁 初醒）")));
    } else {
        const auto& rules = engine.rules();
        for (const auto& id : unlocked) {
            QString name = id;
            for (const auto& r : rules)
                if (r.id == id) { name = r.name; break; }
            lay->addWidget(new QLabel(QStringLiteral("• %1").arg(name)));
        }
    }

    lay->addStretch();
    w.show();
    return app.exec();
}
