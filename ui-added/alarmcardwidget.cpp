#include "alarmcardwidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QStringList>
#include <QVBoxLayout>

namespace {
QString exerciseName(const QString &type)
{
    if (type == QStringLiteral("jumping_jack")) return QStringLiteral("开合跳");
    if (type == QStringLiteral("cycling")) return QStringLiteral("床上蹬腿");
    return QStringLiteral("深蹲");
}
}

AlarmCardWidget::AlarmCardWidget(const wakeai::AlarmSetting &alarm, QWidget *parent)
    : QWidget(parent), alarm_(alarm)
{
    setProperty("uiRole", QStringLiteral("alarmListCard"));
    setMinimumHeight(112);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(22, 15, 18, 15);
    root->setSpacing(18);

    auto *text = new QVBoxLayout;
    text->setSpacing(3);
    time_ = new QLabel(QStringLiteral("%1:%2")
                           .arg(alarm.hour, 2, 10, QLatin1Char('0'))
                           .arg(alarm.minute, 2, 10, QLatin1Char('0')), this);
    time_->setProperty("uiRole", QStringLiteral("alarmCardTime"));
    auto *title = new QLabel(alarm.label, this);
    title->setProperty("uiRole", QStringLiteral("alarmCardTitle"));
    details_ = new QLabel(repeatText(alarm.repeatMask) + QStringLiteral(" · ")
                              + exerciseName(alarm.exerciseType) + QStringLiteral(" ")
                              + QString::number(alarm.targetCount) + QStringLiteral(" 次"), this);
    details_->setProperty("uiRole", QStringLiteral("mutedLabel"));
    text->addWidget(time_);
    text->addWidget(title);
    text->addWidget(details_);
    root->addLayout(text, 1);

    auto *actions = new QVBoxLayout;
    actions->setSpacing(8);
    enabled_ = new QCheckBox(this);
    enabled_->setProperty("uiRole", QStringLiteral("switch"));
    enabled_->setChecked(alarm.enabled);
    updateEnabledAppearance(alarm.enabled);
    auto *row = new QHBoxLayout;
    row->setSpacing(7);
    auto *edit = new QPushButton(QStringLiteral("编辑"), this);
    auto *remove = new QPushButton(QStringLiteral("删除"), this);
    edit->setProperty("uiRole", QStringLiteral("smallButton"));
    remove->setProperty("uiRole", QStringLiteral("dangerButton"));
    row->addWidget(edit);
    row->addWidget(remove);
    actions->addWidget(enabled_, 0, Qt::AlignRight);
    actions->addLayout(row);
    root->addLayout(actions);

    connect(enabled_, &QCheckBox::toggled, this, [this](bool checked) {
        alarm_.enabled = checked;
        updateEnabledAppearance(checked);
        emit enabledChanged(alarm_.id, checked);
    });
    connect(edit, &QPushButton::clicked, this, [this] { emit editRequested(alarm_.id); });
    connect(remove, &QPushButton::clicked, this, [this] { emit deleteRequested(alarm_.id); });
}

QString AlarmCardWidget::repeatText(int mask)
{
    if (mask == 0) return QStringLiteral("单次");
    if (mask == 0x7F) return QStringLiteral("每天");
    if (mask == 0x1F) return QStringLiteral("工作日");
    if (mask == 0x60) return QStringLiteral("周末");
    const QStringList days = {QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
                              QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
                              QStringLiteral("日")};
    QStringList selected;
    for (int i = 0; i < 7; ++i)
        if (mask & (1 << i)) selected.append(days[i]);
    return QStringLiteral("周") + selected.join(QStringLiteral("、"));
}

void AlarmCardWidget::updateEnabledAppearance(bool enabled)
{
    enabled_->setText(enabled ? QStringLiteral("已启用") : QStringLiteral("已停用"));
    setProperty("activeAlarm", enabled);
    style()->unpolish(this);
    style()->polish(this);
    update();
}
