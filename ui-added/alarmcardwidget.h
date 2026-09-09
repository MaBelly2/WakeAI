#pragma once

#include <QWidget>
#include "system/DatabaseManager.h"

class QCheckBox;
class QLabel;
class QPushButton;

class AlarmCardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmCardWidget(const wakeai::AlarmSetting &alarm, QWidget *parent = nullptr);
    qint64 alarmId() const { return alarm_.id; }

signals:
    void enabledChanged(qint64 id, bool enabled);
    void editRequested(qint64 id);
    void deleteRequested(qint64 id);

private:
    static QString repeatText(int mask);
    void updateEnabledAppearance(bool enabled);

    wakeai::AlarmSetting alarm_;
    QLabel *time_ = nullptr;
    QLabel *details_ = nullptr;
    QCheckBox *enabled_ = nullptr;
};
