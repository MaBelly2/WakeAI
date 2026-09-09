#include "mainwindow.h"
#include "alarmcardwidget.h"
#include "ringtonedialog.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtMath>

#include <utility>

namespace {

QString databasePath(bool testMode)
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + (testMode ? "/wakeai-test.db" : "/wakeai.db");
}

QString typeFor(int index)
{
    return index == 1 ? QStringLiteral("jumping_jack")
                      : index == 2 ? QStringLiteral("cycling") : QStringLiteral("squat");
}

QString nameFor(const QString &type)
{
    if (type == QStringLiteral("jumping_jack")) return QStringLiteral("开合跳");
    if (type == QStringLiteral("cycling")) return QStringLiteral("床上蹬腿");
    if (type == QStringLiteral("squat")) return QStringLiteral("深蹲");
    return type;
}

QString greetingForNow()
{
    const int hour = QTime::currentTime().hour();
    if (hour < 6) return QStringLiteral("夜深了");
    if (hour < 11) return QStringLiteral("早上好");
    if (hour < 14) return QStringLiteral("中午好");
    if (hour < 19) return QStringLiteral("下午好");
    return QStringLiteral("晚上好");
}

QString chineseDate(const QDate &date)
{
    const QStringList weekdays = {QStringLiteral("星期一"), QStringLiteral("星期二"),
                                  QStringLiteral("星期三"), QStringLiteral("星期四"),
                                  QStringLiteral("星期五"), QStringLiteral("星期六"),
                                  QStringLiteral("星期日")};
    return QStringLiteral("%1 月 %2 日 %3 · 让明天从一次准时起床开始")
        .arg(date.month()).arg(date.day()).arg(weekdays.value(date.dayOfWeek() - 1));
}

QString timeText(int hour, int minute)
{
    return QStringLiteral("%1:%2").arg(hour, 2, 10, QLatin1Char('0'))
        .arg(minute, 2, 10, QLatin1Char('0'));
}

} // namespace

MainWindow::MainWindow(StartupOptions options, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , options_(std::move(options))
    , db_(databasePath(options_.testMode))
    , achievements_(db_)
    , alarm_(this)
    , player_(this)
    , audio_(this)
{
    ui->setupUi(this);
    setWindowTitle(options_.testMode ? QStringLiteral("WakeAI · 测试模式（独立数据）")
                                     : QStringLiteral("WakeAI · 智能运动唤醒"));
    resize(1080, 840);

    QFile styleFile(QStringLiteral(":/styles/wakeai.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    ui->lblGreeting->setText(greetingForNow());
    ui->lblDate->setText(chineseDate(QDate::currentDate()));
    ui->stackedWidget->setCurrentIndex(HomePage);
    ui->btnFinish->setEnabled(false);
    ui->cameraView->setCount(0, 1, false);
    connect(ui->checkAlarmEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        ui->checkAlarmEnabled->setText(checked ? QStringLiteral("已启用")
                                               : QStringLiteral("已停用"));
    });

    player_.setAudioOutput(&audio_);
    player_.setLoops(QMediaPlayer::Infinite);
    audio_.setVolume(0.85);
    connect(&player_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                statusBar()->showMessage(QStringLiteral("铃声播放失败：") + message, 8000);
            });
    connect(&alarm_, &wakeai::AlarmManager::alarmTriggered, this, &MainWindow::startRing);
    connect(&alarm_, &wakeai::AlarmManager::alarmStopped, &player_, &QMediaPlayer::stop);

    QString ringtoneError;
    if (!ringtones_.initialize(&ringtoneError))
        statusBar()->showMessage(ringtoneError, 10000);

    auto *toolsMenu = menuBar()->addMenu(QStringLiteral("工具"));
    toolsMenu->addAction(QStringLiteral("闹钟管理"), this, &MainWindow::on_btnManageAlarms_clicked);
    toolsMenu->addAction(QStringLiteral("成就与连续记录"), this, &MainWindow::showAchievements);
    toolsMenu->addAction(QStringLiteral("数据文件位置"), this, [this] {
        QMessageBox::information(this, QStringLiteral("数据文件"), db_.databasePath());
    });
    toolsMenu->addAction(QStringLiteral("铃声文件夹"), this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ringtones_.directory()));
    });
    toolsMenu->addAction(QStringLiteral("重试保存本次记录"), this, [this] {
        if (pending_) savePending();
        else QMessageBox::information(this, QStringLiteral("记录"), QStringLiteral("没有待保存记录"));
    });
    toolsMenu->addAction(QStringLiteral("选择摄像头"), this, [this] {
        if (state_ != State::Idle || thread_) return;
        bool accepted = false;
        const int number = QInputDialog::getInt(this, QStringLiteral("摄像头"),
                                                QStringLiteral("摄像头编号"), options_.cameraIndex,
                                                0, 20, 1, &accepted);
        if (accepted) {
            options_.cameraIndex = number;
            options_.videoPath.clear();
            statusBar()->showMessage(QStringLiteral("已选择摄像头 ") + QString::number(number), 5000);
        }
    });

    if (options_.testMode) {
        auto *testMenu = menuBar()->addMenu(QStringLiteral("测试"));
        testMenu->addAction(QStringLiteral("10 秒后响铃"), this, [this] { startTestAlarm(10); });
        testMenu->addAction(QStringLiteral("选择测试视频"), this, [this] {
            if (state_ != State::Idle || thread_) return;
            const QString file = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择固定测试视频"), {},
                QStringLiteral("视频 (*.mp4 *.avi *.mov *.mkv)"));
            if (!file.isEmpty()) {
                options_.videoPath = file;
                statusBar()->showMessage(QStringLiteral("测试视频：") + file, 5000);
            }
        });
    }

    if (!db_.init()) {
        ui->btnSetAlarm->setEnabled(false);
        ui->btnManageAlarms->setEnabled(false);
        ui->lblSetInfo->setText(QStringLiteral("数据库初始化失败"));
        QTimer::singleShot(0, this, [this] {
            QMessageBox::critical(this, QStringLiteral("数据库错误"), db_.lastError());
        });
    } else {
        refreshRecords();
        refreshAlarmList();
        scheduleNextAlarm();
        refreshDashboard();
    }
}

MainWindow::~MainWindow()
{
    if (control_) control_->stop.store(true);
    if (thread_) {
        thread_->quit();
        thread_->wait();
    }
    delete ui;
}

QString MainWindow::modeName() const
{
    return nameFor(setting_.exerciseType);
}

QString MainWindow::findModelPath() const
{
    if (!options_.modelPath.isEmpty())
        return QFileInfo(options_.modelPath).absoluteFilePath();
    const QString filename = QStringLiteral("yolov8n-pose.onnx");
    const QStringList fixed = {QCoreApplication::applicationDirPath() + "/models/" + filename,
                               QDir::currentPath() + "/models/" + filename};
    for (const auto &path : fixed)
        if (QFileInfo::exists(path)) return QFileInfo(path).absoluteFilePath();
    QDir directory(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 8; ++level) {
        const QString path = directory.filePath("models/" + filename);
        if (QFileInfo::exists(path)) return path;
        if (!directory.cdUp()) break;
    }
#ifdef WAKEAI_SOURCE_MODEL_DIR
    const QString fallback = QString::fromUtf8(WAKEAI_SOURCE_MODEL_DIR) + "/" + filename;
    if (QFileInfo::exists(fallback)) return fallback;
#endif
    return {};
}

void MainWindow::switchPage(Page page)
{
    if (ui->stackedWidget->currentIndex() == int(page)) return;
    ui->stackedWidget->setCurrentIndex(int(page));
    QWidget *current = ui->stackedWidget->currentWidget();
    auto *effect = new QGraphicsOpacityEffect(current);
    current->setGraphicsEffect(effect);
    auto *animation = new QPropertyAnimation(effect, "opacity", current);
    animation->setDuration(230);
    animation->setStartValue(0.25);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished, current, [current] {
        current->setGraphicsEffect(nullptr);
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

QString MainWindow::repeatText(int mask) const
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

QDateTime MainWindow::nextOccurrence(const wakeai::AlarmSetting &alarm,
                                     const QDateTime &from) const
{
    if (!alarm.enabled || !QTime(alarm.hour, alarm.minute).isValid()) return {};
    const QTime time(alarm.hour, alarm.minute);
    if (alarm.repeatMask == 0) {
        QDateTime candidate(from.date(), time);
        if (candidate <= from) candidate = candidate.addDays(1);
        return candidate;
    }
    for (int offset = 0; offset <= 7; ++offset) {
        const QDate date = from.date().addDays(offset);
        if (!(alarm.repeatMask & (1 << (date.dayOfWeek() - 1)))) continue;
        const QDateTime candidate(date, time);
        if (candidate > from) return candidate;
    }
    return {};
}

void MainWindow::scheduleNextAlarm()
{
    if (!db_.isOpen() || alarm_.isRinging()) return;
    alarm_.disable();
    QDateTime nearest;
    wakeai::AlarmSetting nearestAlarm;
    nearestAlarm.id = -1;
    const QDateTime now = QDateTime::currentDateTime();
    for (const auto &candidate : db_.alarms()) {
        const QDateTime occurrence = nextOccurrence(candidate, now);
        if (occurrence.isValid() && (!nearest.isValid() || occurrence < nearest)) {
            nearest = occurrence;
            nearestAlarm = candidate;
        }
    }
    scheduled_ = nearest;
    scheduledAlarmId_ = nearestAlarm.id;
    if (nearestAlarm.id >= 0) {
        setting_ = nearestAlarm;
        if (ringtones_.item(setting_.ringtoneId).id.isEmpty())
            setting_.ringtoneId = RingtoneManager::defaultId();
        alarm_.setAlarmDateTime(nearest);
        alarm_.setChallenge(setting_.targetCount);
        alarm_.enable();
    }
    refreshDashboard();
}

void MainWindow::refreshDashboard()
{
    ui->lblGreeting->setText(greetingForNow());
    ui->lblDate->setText(chineseDate(QDate::currentDate()));
    if (scheduled_.isValid() && scheduledAlarmId_ >= 0) {
        ui->lblNextTime->setText(timeText(setting_.hour, setting_.minute));
        ui->lblNextLabel->setText(setting_.label);
        ui->lblSetInfo->setText(QStringLiteral("%1 · %2 · %3 %4 次")
                                    .arg(scheduled_.toString("M 月 d 日"))
                                    .arg(repeatText(setting_.repeatMask))
                                    .arg(nameFor(setting_.exerciseType))
                                    .arg(setting_.targetCount));
    } else {
        ui->lblNextTime->setText(QStringLiteral("--:--"));
        ui->lblNextLabel->setText(QStringLiteral("暂无启用闹钟"));
        ui->lblSetInfo->setText(QStringLiteral("新建闹钟，或在闹钟列表中开启一个已有闹钟"));
    }
    updateAchievementCard();
}

void MainWindow::updateAchievementCard()
{
    const auto stats = db_.queryStatistics();
    const auto &rules = achievements_.rules();
    if (rules.isEmpty()) return;
    achievementIndex_ = (achievementIndex_ % rules.size() + rules.size()) % rules.size();
    const auto &rule = rules[achievementIndex_];
    int current = 0;
    int target = 1;
    if (rule.id == QStringLiteral("first_wake")) { current = stats.totalWakeCount; target = 1; }
    else if (rule.id == QStringLiteral("streak_3")) { current = stats.consecutiveDays; target = 3; }
    else if (rule.id == QStringLiteral("streak_7")) { current = stats.consecutiveDays; target = 7; }
    else if (rule.id == QStringLiteral("actions_100")) { current = stats.totalActions; target = 100; }
    else if (rule.id == QStringLiteral("wake_30")) { current = stats.totalWakeCount; target = 30; }
    const bool unlocked = db_.isAchievementUnlocked(rule.id);
    ui->achievementIcon->setAchievement(achievementIndex_, unlocked);
    ui->lblAchievementIndex->setText(QStringLiteral("%1 / %2").arg(achievementIndex_ + 1).arg(rules.size()));
    ui->lblAchievementName->setText(rule.name);
    ui->lblAchievementDesc->setText(rule.description);
    ui->lblAchievementProgress->setText(unlocked
        ? QStringLiteral("已解锁 · %1 / %2").arg(qMin(current, target)).arg(target)
        : QStringLiteral("进度 %1 / %2 · 未解锁").arg(qMin(current, target)).arg(target));
    ui->lblStats->setText(QStringLiteral("连续 %1 天 · 成功 %2 次 · 动作 %3 个")
                              .arg(stats.consecutiveDays).arg(stats.totalWakeCount).arg(stats.totalActions));
}

void MainWindow::on_btnAchievementPrev_clicked()
{
    --achievementIndex_;
    updateAchievementCard();
}

void MainWindow::on_btnAchievementNext_clicked()
{
    ++achievementIndex_;
    updateAchievementCard();
}

void MainWindow::showAchievements()
{
    if (state_ == State::Ringing || state_ == State::Preparing || state_ == State::Exercising)
        return;
    updateAchievementCard();
    switchPage(HomePage);
}

void MainWindow::refreshAlarmList()
{
    ui->listAlarms->clear();
    const auto alarms = db_.alarms();
    ui->lblAlarmCount->setText(QStringLiteral("%1 个闹钟").arg(alarms.size()));
    ui->lblAlarmEmpty->setVisible(alarms.isEmpty());
    ui->listAlarms->setVisible(!alarms.isEmpty());
    for (const auto &alarm : alarms) {
        auto *item = new QListWidgetItem(ui->listAlarms);
        item->setSizeHint(QSize(0, 118));
        auto *card = new AlarmCardWidget(alarm, ui->listAlarms);
        ui->listAlarms->setItemWidget(item, card);
        connect(card, &AlarmCardWidget::enabledChanged, this,
                [this](qint64 id, bool enabled) {
                    if (!db_.setAlarmEnabled(id, enabled))
                        QMessageBox::warning(this, QStringLiteral("保存失败"), db_.lastError());
                    QTimer::singleShot(0, this, [this] {
                        refreshAlarmList();
                        scheduleNextAlarm();
                    });
                });
        connect(card, &AlarmCardWidget::editRequested, this,
                [this](qint64 id) { beginEditAlarm(id); });
        connect(card, &AlarmCardWidget::deleteRequested, this, [this](qint64 id) {
            const auto selected = db_.alarm(id);
            if (selected.id < 0) return;
            if (QMessageBox::question(this, QStringLiteral("删除闹钟"),
                                      QStringLiteral("确认删除“%1”？").arg(selected.label),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                != QMessageBox::Yes) return;
            if (!db_.deleteAlarm(id)) {
                QMessageBox::warning(this, QStringLiteral("删除失败"), db_.lastError());
                return;
            }
            QTimer::singleShot(0, this, [this] {
                refreshAlarmList();
                scheduleNextAlarm();
            });
        });
    }
}

void MainWindow::beginEditAlarm(qint64 id)
{
    if (state_ == State::Ringing || state_ == State::Preparing || state_ == State::Exercising)
        return;
    if (id >= 0) {
        editingAlarm_ = db_.alarm(id);
        if (editingAlarm_.id < 0) {
            QMessageBox::warning(this, QStringLiteral("读取失败"), db_.lastError());
            return;
        }
    } else {
        editingAlarm_ = {};
        editingAlarm_.id = -1;
        const QTime proposed = QTime::currentTime().addSecs(300);
        editingAlarm_.hour = proposed.hour();
        editingAlarm_.minute = proposed.minute();
        editingAlarm_.label = QStringLiteral("起床闹钟");
        editingAlarm_.enabled = true;
        editingAlarm_.ringtoneId = RingtoneManager::defaultId();
    }
    if (ringtones_.item(editingAlarm_.ringtoneId).id.isEmpty())
        editingAlarm_.ringtoneId = RingtoneManager::defaultId();
    ui->lblEditTitle->setText(id >= 0 ? QStringLiteral("编辑闹钟") : QStringLiteral("新建闹钟"));
    ui->timeEditAlarm->setTime(QTime(editingAlarm_.hour, editingAlarm_.minute));
    ui->editAlarmLabel->setText(editingAlarm_.label);
    ui->checkAlarmEnabled->setChecked(editingAlarm_.enabled);
    ui->checkAlarmEnabled->setText(editingAlarm_.enabled ? QStringLiteral("已启用")
                                                        : QStringLiteral("已停用"));
    setEditorRepeatMask(editingAlarm_.repeatMask);
    ui->comboMode->setCurrentIndex(editingAlarm_.exerciseType == QStringLiteral("jumping_jack") ? 1
                                     : editingAlarm_.exerciseType == QStringLiteral("cycling") ? 2 : 0);
    ui->spinTarget->setValue(editingAlarm_.targetCount);
    ui->spinSnooze->setValue(editingAlarm_.snoozeMinutes);
    updateRingtoneLabel();
    switchPage(AlarmEditPage);
}

int MainWindow::editorRepeatMask() const
{
    const QCheckBox *days[] = {ui->checkMon, ui->checkTue, ui->checkWed, ui->checkThu,
                               ui->checkFri, ui->checkSat, ui->checkSun};
    int mask = 0;
    for (int i = 0; i < 7; ++i)
        if (days[i]->isChecked()) mask |= (1 << i);
    return mask;
}

void MainWindow::setEditorRepeatMask(int mask)
{
    QCheckBox *days[] = {ui->checkMon, ui->checkTue, ui->checkWed, ui->checkThu,
                         ui->checkFri, ui->checkSat, ui->checkSun};
    for (int i = 0; i < 7; ++i) days[i]->setChecked(mask & (1 << i));
}

void MainWindow::updateRingtoneLabel()
{
    RingtoneItem selected = ringtones_.item(editingAlarm_.ringtoneId);
    if (selected.id.isEmpty()) {
        editingAlarm_.ringtoneId = RingtoneManager::defaultId();
        selected = ringtones_.item(editingAlarm_.ringtoneId);
    }
    ui->lblRingtone->setText(QStringLiteral("%1 · %2%")
                                 .arg(selected.name.isEmpty() ? QStringLiteral("经典闹铃") : selected.name)
                                 .arg(qRound(editingAlarm_.volume * 100.0)));
}

void MainWindow::on_btnChooseRingtone_clicked()
{
    RingtoneDialog dialog(ringtones_, editingAlarm_.ringtoneId, editingAlarm_.volume, this);
    if (dialog.exec() != QDialog::Accepted) return;
    editingAlarm_.ringtoneId = dialog.selectedId();
    editingAlarm_.volume = dialog.volume();
    updateRingtoneLabel();
}

void MainWindow::on_btnSetAlarm_clicked()
{
    beginEditAlarm();
}

void MainWindow::on_btnManageAlarms_clicked()
{
    if (state_ == State::Ringing || state_ == State::Preparing || state_ == State::Exercising)
        return;
    refreshAlarmList();
    switchPage(AlarmListPage);
}

void MainWindow::on_btnAlarmBack_clicked()
{
    refreshDashboard();
    switchPage(HomePage);
}

void MainWindow::on_btnAddAlarm_clicked()
{
    beginEditAlarm();
}

void MainWindow::on_btnEditCancel_clicked()
{
    refreshAlarmList();
    switchPage(AlarmListPage);
}

void MainWindow::on_btnEditSave_clicked()
{
    const QString label = ui->editAlarmLabel->text().trimmed();
    editingAlarm_.hour = ui->timeEditAlarm->time().hour();
    editingAlarm_.minute = ui->timeEditAlarm->time().minute();
    editingAlarm_.label = label.isEmpty() ? QStringLiteral("起床闹钟") : label;
    editingAlarm_.enabled = ui->checkAlarmEnabled->isChecked();
    editingAlarm_.repeatMask = editorRepeatMask();
    editingAlarm_.exerciseType = typeFor(ui->comboMode->currentIndex());
    editingAlarm_.targetCount = ui->spinTarget->value();
    editingAlarm_.snoozeMinutes = ui->spinSnooze->value();
    const qint64 id = db_.saveAlarm(editingAlarm_);
    if (id < 0) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), db_.lastError());
        return;
    }
    editingAlarm_.id = id;
    refreshAlarmList();
    scheduleNextAlarm();
    switchPage(AlarmListPage);
}

void MainWindow::startTestAlarm(int seconds)
{
    if (state_ != State::Idle || thread_ || pending_ || !db_.isOpen()) {
        QMessageBox::information(this, QStringLiteral("暂不能测试"),
                                 QStringLiteral("请先完成当前任务并等待识别线程退出。"));
        return;
    }
    auto available = db_.alarms();
    if (available.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有闹钟"),
                                 QStringLiteral("请先新建一个闹钟，再使用 10 秒测试。"));
        return;
    }
    setting_ = available.first();
    for (const auto &candidate : available)
        if (candidate.id == scheduledAlarmId_) { setting_ = candidate; break; }
    scheduledAlarmId_ = setting_.id;
    alarm_.disable();
    alarm_.setChallenge(setting_.targetCount);
    alarm_.setTestAlarmInSeconds(seconds);
    alarm_.enable();
    scheduled_ = alarm_.nextTrigger();
    ui->lblNextTime->setText(scheduled_.toString("HH:mm:ss"));
    ui->lblNextLabel->setText(QStringLiteral("测试：") + setting_.label);
    ui->lblSetInfo->setText(QStringLiteral("%1 秒后响铃 · 使用独立测试数据库").arg(seconds));
}

bool MainWindow::prepareAlarmAudio()
{
    RingtoneItem selected = ringtones_.item(setting_.ringtoneId);
    if (selected.id.isEmpty()) {
        setting_.ringtoneId = RingtoneManager::defaultId();
        selected = ringtones_.item(setting_.ringtoneId);
    }
    if (selected.path.isEmpty() || !QFileInfo::exists(selected.path)) {
        statusBar()->showMessage(QStringLiteral("找不到已选择的铃声文件，请重新选择"), 8000);
        return false;
    }
    player_.stop();
    player_.setSource(QUrl::fromLocalFile(selected.path));
    player_.setLoops(QMediaPlayer::Infinite);
    audio_.setVolume(qBound(0.0, setting_.volume, 1.0));
    return true;
}

void MainWindow::startRing()
{
    if (closing_) return;
    state_ = State::Ringing;
    sessionId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (prepareAlarmAudio()) player_.play();
    ui->lblRingTime->setText(timeText(setting_.hour, setting_.minute));
    ui->lblRingTitle->setText(setting_.label);
    ui->lblTask->setText(modeName() + QStringLiteral(" ")
                         + QString::number(setting_.targetCount) + QStringLiteral(" 次"));
    ui->btnSnooze->setText(QStringLiteral("稍后提醒（%1 分钟）").arg(setting_.snoozeMinutes));
    switchPage(RingingPage);
}

void MainWindow::on_btnStart_clicked()
{
    if (state_ != State::Ringing) return;
    state_ = State::Preparing;
    ui->lblReadyTask->setText(modeName() + QStringLiteral(" ")
                              + QString::number(setting_.targetCount) + QStringLiteral(" 次"));
    ui->btnBegin->setEnabled(!thread_);
    switchPage(PreparingPage);
}

void MainWindow::on_btnSnooze_clicked()
{
    if (state_ != State::Ringing || !alarm_.snooze(setting_.snoozeMinutes * 60)) return;
    scheduled_ = alarm_.nextTrigger();
    state_ = State::Idle;
    ui->lblNextTime->setText(scheduled_.toString("HH:mm"));
    ui->lblNextLabel->setText(setting_.label + QStringLiteral(" · 已稍后提醒"));
    ui->lblSetInfo->setText(QStringLiteral("将在 %1 再次响铃").arg(scheduled_.toString("HH:mm:ss")));
    switchPage(HomePage);
}

void MainWindow::on_btnBegin_clicked()
{
    if (state_ != State::Preparing || thread_ || closing_) return;
    const QString model = findModelPath();
    if (model.isEmpty() || !QFileInfo::exists(model)) {
        QMessageBox::warning(this, QStringLiteral("找不到模型"),
                             QStringLiteral("请把 yolov8n-pose.onnx 放在仓库 models 目录，或使用 --model 指定。"));
        return;
    }

    session_.begin(setting_.targetCount);
    paused_ = false;
    poseValid_ = false;
    state_ = State::Exercising;
    ui->btnPause->setEnabled(true);
    ui->btnPause->setText(QStringLiteral("暂停"));
    ui->btnFinish->setEnabled(false);
    ui->lblExerciseTitle->setText(modeName() + QStringLiteral("识别中"));
    ui->lblExerciseSubtitle->setText(QStringLiteral("正在启动识别，请让全身保持在画面内"));
    ui->cameraView->clearFrame();
    ui->cameraView->setCount(0, session_.target(), false);
    ui->cameraView->showNotice(QStringLiteral("正在启动识别…"), CameraView::NoticeKind::Info, 1800);
    switchPage(ExercisePage);

    control_ = std::make_shared<MotionControl>();
    MotionWorker::Options workerOptions;
    workerOptions.modelPath = model;
    workerOptions.videoPath = options_.videoPath;
    workerOptions.cameraIndex = options_.cameraIndex;
    workerOptions.noProgressHintMs = options_.testMode ? 15000 : 120000;
    workerOptions.mode = setting_.exerciseType == QStringLiteral("jumping_jack")
        ? MotionWorker::Mode::JumpingJack
        : setting_.exerciseType == QStringLiteral("cycling") ? MotionWorker::Mode::Cycling
                                                               : MotionWorker::Mode::Squat;

    auto *workerThread = new QThread(this);
    auto *worker = new MotionWorker(workerOptions, control_);
    thread_ = workerThread;
    worker->moveToThread(workerThread);
    const auto control = control_;
    connect(workerThread, &QThread::started, worker, &MotionWorker::start);
    connect(worker, &MotionWorker::finished, workerThread, &QThread::quit, Qt::DirectConnection);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &MotionWorker::countChanged, this, [this, workerThread](int count) {
        if (thread_ == workerThread && !closing_) updateCount(count);
    });
    connect(worker, &MotionWorker::stateChanged, this,
            [this, workerThread](const QString &message) {
                if (thread_ == workerThread && state_ == State::Exercising && !session_.reached())
                    ui->lblExerciseSubtitle->setText(message);
            });
    connect(worker, &MotionWorker::failed, this, [this, workerThread](const QString &message) {
        if (thread_ != workerThread || closing_) return;
        ui->lblReadyTask->setText(message + QStringLiteral("；检查后点击开始识别重试"));
        ui->cameraView->showNotice(message, CameraView::NoticeKind::Warning, 6000);
        statusBar()->showMessage(message, 10000);
    });
    connect(worker, &MotionWorker::poseValidChanged, this,
            [this, workerThread](bool valid) {
                if (thread_ != workerThread || state_ != State::Exercising || session_.reached() || paused_)
                    return;
                poseValid_ = valid;
                ui->lblExerciseSubtitle->setText(valid
                    ? QStringLiteral("关键点有效，请继续完成动作")
                    : QStringLiteral("请后退一步，让全身进入画面"));
            });
    connect(worker, &MotionWorker::wrongMotionHint, this, [this, workerThread] {
        if (thread_ != workerThread || state_ != State::Exercising || session_.reached() || paused_)
            return;
        ui->cameraView->showNotice(QStringLiteral("本次未计入，请增大动作幅度"),
                                   CameraView::NoticeKind::Warning, 4200);
    });
    connect(worker, &MotionWorker::frameReady, this,
            [this, workerThread, control](const QImage &image) {
                if (thread_ == workerThread && !closing_) ui->cameraView->setFrame(image);
                control->framePending.store(false);
            });
    connect(workerThread, &QThread::finished, this, [this, workerThread] {
        if (thread_ == workerThread) {
            thread_ = nullptr;
            control_.reset();
            if (state_ == State::Exercising && !session_.reached() && !closing_) {
                state_ = State::Preparing;
                ui->lblReadyNote->setText(QStringLiteral("识别已结束；重试将从 0 重新计数"));
                ui->btnBegin->setEnabled(true);
                switchPage(PreparingPage);
            }
        }
        workerThread->deleteLater();
        if (closing_) QTimer::singleShot(0, this, &QWidget::close);
    });
    workerThread->start();
}

void MainWindow::stopWorker()
{
    if (control_) control_->stop.store(true);
    if (thread_) thread_->quit();
}

void MainWindow::updateCount(int count)
{
    if (state_ != State::Exercising) return;
    const bool justReached = session_.updateCount(count);
    ui->cameraView->setCount(session_.count(), session_.target(), true);
    if (justReached) {
        alarm_.setChallengeCompleted(true);
        ui->btnPause->setEnabled(false);
        ui->btnFinish->setEnabled(true);
        ui->lblExerciseSubtitle->setText(QStringLiteral("目标已完成，最终画面已锁定"));
        ui->cameraView->showSuccess(QStringLiteral("挑战完成！"));
        if (control_) {
            control_->freezeFrame.store(true);
            control_->stop.store(true);
        }
    }
}

void MainWindow::on_btnPause_clicked()
{
    if (state_ != State::Exercising || session_.reached() || !control_) return;
    paused_ = !paused_;
    control_->paused.store(paused_);
    ui->btnPause->setText(paused_ ? QStringLiteral("继续") : QStringLiteral("暂停"));
    ui->lblExerciseSubtitle->setText(paused_ ? QStringLiteral("识别已暂停，画面仍保持实时")
                                             : QStringLiteral("识别已继续"));
    if (paused_)
        ui->cameraView->showNotice(QStringLiteral("已暂停"), CameraView::NoticeKind::Info, 1600);
    else
        ui->cameraView->clearNotice();
}

void MainWindow::on_btnFinish_clicked()
{
    if (state_ != State::Exercising || !session_.reached() || session_.finished()) return;
    if (!alarm_.stop()) return;
    session_.finish();
    state_ = State::Done;
    stopWorker();
    const wakeai::AlarmSetting completedAlarm = setting_;
    const QDateTime now = QDateTime::currentDateTime();
    pendingRecord_ = {};
    pendingRecord_.date = now.date().toString("yyyy-MM-dd");
    pendingRecord_.alarmTime = scheduled_.toString("HH:mm");
    pendingRecord_.exerciseType = completedAlarm.exerciseType;
    pendingRecord_.targetCount = session_.target();
    pendingRecord_.actualCount = session_.count();
    pendingRecord_.success = true;
    pendingRecord_.completedAt = now.toString("yyyy-MM-dd HH:mm:ss");
    pendingRecord_.sessionId = sessionId_;
    pending_ = true;
    ui->lblSummary->setText(nameFor(completedAlarm.exerciseType) + QStringLiteral(" ")
                            + QString::number(session_.count()) + QStringLiteral(" 次 已完成"));
    ui->lblClosed->setText(QStringLiteral("闹钟已关闭"));
    if (completedAlarm.repeatMask == 0 && completedAlarm.id >= 0)
        db_.setAlarmEnabled(completedAlarm.id, false);
    switchPage(DonePage);
    savePending();
    refreshAlarmList();
    scheduleNextAlarm();
}

bool MainWindow::savePending()
{
    if (!pending_) return true;
    if (!db_.saveWakeRecord(pendingRecord_)) {
        ui->lblClosed->setText(QStringLiteral("闹钟已关闭；记录保存失败，请通过工具菜单重试"));
        QMessageBox::warning(this, QStringLiteral("保存失败"), db_.lastError());
        return false;
    }
    pending_ = false;
    const auto unlocked = achievements_.checkAndUnlock(db_.queryStatistics());
    if (!unlocked.isEmpty()) {
        const auto &rules = achievements_.rules();
        for (int i = 0; i < rules.size(); ++i)
            if (rules[i].id == unlocked.last()) achievementIndex_ = i;
    }
    ui->lblClosed->setText(unlocked.isEmpty()
        ? QStringLiteral("闹钟已关闭，记录已保存")
        : QStringLiteral("记录已保存，新解锁成就 %1 个").arg(unlocked.size()));
    refreshRecords();
    updateAchievementCard();
    return true;
}

void MainWindow::refreshRecords()
{
    ui->listRecords->clear();
    for (const auto &record : db_.recentRecords()) {
        ui->listRecords->addItem(record.completedAt + QStringLiteral(" · ")
                                 + nameFor(record.exerciseType) + QStringLiteral(" ")
                                 + QString::number(record.actualCount) + QStringLiteral("/")
                                 + QString::number(record.targetCount));
    }
    const auto statistics = db_.queryStatistics();
    ui->lblWeek->setText(QStringLiteral("成功唤醒 %1 次 · 连续 %2 天 · 累计 %3 个动作")
                             .arg(statistics.totalWakeCount)
                             .arg(statistics.consecutiveDays)
                             .arg(statistics.totalActions));
}

void MainWindow::on_btnRecords_clicked()
{
    refreshRecords();
    switchPage(RecordsPage);
}

void MainWindow::on_btnHistory_clicked()
{
    on_btnRecords_clicked();
}

void MainWindow::on_btnHome_clicked()
{
    if (pending_ && !savePending()) return;
    if (thread_) {
        QMessageBox::information(this, QStringLiteral("正在停止"),
                                 QStringLiteral("请等待摄像头线程退出后返回首页。"));
        return;
    }
    state_ = State::Idle;
    refreshDashboard();
    switchPage(HomePage);
}

void MainWindow::on_btnRecordBack_clicked()
{
    if (state_ == State::Done) switchPage(DonePage);
    else {
        refreshDashboard();
        switchPage(HomePage);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!closeApproved_) {
        if (pending_ && !savePending()) {
            if (QMessageBox::question(this, QStringLiteral("记录尚未保存"),
                                      QStringLiteral("保存仍失败。现在退出会丢失本次未保存记录，是否继续？"),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                != QMessageBox::Yes) {
                event->ignore(); return;
            }
        }
        if (alarm_.isRinging() || thread_) {
            if (QMessageBox::question(this, QStringLiteral("退出确认"),
                                      QStringLiteral("退出会结束当前任务，未完成任务不会记为成功。是否退出？"),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                != QMessageBox::Yes) {
                event->ignore(); return;
            }
        }
        closeApproved_ = true;
    }
    if (thread_) {
        closing_ = true;
        stopWorker();
        player_.stop();
        statusBar()->showMessage(QStringLiteral("正在等待摄像头线程退出…"));
        event->ignore();
        return;
    }
    event->accept();
}
