#pragma once

#include <QDialog>
#include <QMediaPlayer>
#include <QAudioOutput>

#include "ringtonemanager.h"

class QListWidget;
class QPushButton;
class QSlider;

class RingtoneDialog : public QDialog
{
    Q_OBJECT
public:
    RingtoneDialog(RingtoneManager &manager, const QString &selectedId,
                   double volume, QWidget *parent = nullptr);
    ~RingtoneDialog() override;

    QString selectedId() const;
    double volume() const;

private:
    void reload(const QString &selectId = {});
    void togglePreview();
    void importRingtone();
    void deleteSelected();
    void updateButtons();

    RingtoneManager &manager_;
    QListWidget *list_ = nullptr;
    QPushButton *preview_ = nullptr;
    QPushButton *remove_ = nullptr;
    QSlider *volume_ = nullptr;
    QMediaPlayer player_;
    QAudioOutput audio_;
};
