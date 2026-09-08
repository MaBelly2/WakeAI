#include "mainwindow.h"
#include "ringtonedialog.h"
#include "ui_mainwindow.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QInputDialog>
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
    return index == 1 ? "jumping_jack" : index == 2 ? "cycling" : "squat";
}

QString nameFor(const QString &type)
{
    if (type == "jumping_jack")
        return "开合跳";
    if (type == "cycling")
        return "床上蹬腿";
    if (type == "squat")
        return "深蹲";
    return type;
}

QString greetingForNow()
{
    const int hour = QTime::currentTime().hour();
    if (hour < 6)
        return "夜深了";
    if (hour < 11)
        return "早上好";
    if (hour < 14)
        return "中午好";
    if (hour < 19)
        return "下午好";
    return "晚上好";
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
    setWindowTitle(options_.testMode ? "WakeAI · 测试模式（独立数据）"
                                     : "WakeAI · 智能运动唤醒");
    resize(1040, 820);

    QFile styleFile(":/styles/wakeai.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    auto addShadow = [](QWidget *widget) {
        auto *shadow = new QGraphicsDropShadowEffect(widget);
        shadow->setBlurRadius(34.0);
        shadow->setOffset(0.0, 12.0);
        shadow->setColor(QColor(18, 49, 43, 28));
        widget->setGraphicsEffect(shadow);
    };
    addShadow(ui->alarmCard);
    addShadow(ui->missionCard);
    addShadow(ui->prepCard);

    ui->lblGreeting->setText(greetingForNow());
    ui->lblDate->setText(QDate::currentDate().toString("M 月 d 日 dddd · 让明天从一次准时起床开始"));
    ui->stackedWidget->setCurrentIndex(0);
    ui->btnFinish->setEnabled(false);
    ui->cameraView->setCount(0, ui->spinTarget->value());

    player_.setAudioOutput(&audio_);
    player_.setLoops(QMediaPlayer::Infinite);
    audio_.setVolume(0.85);
    connect(&player_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                statusBar()->showMessage("铃声播放失败：" + message, 8000);
            });
    connect(&alarm_, &wakeai::AlarmManager::alarmTriggered, this, &MainWindow::startRing);
    connect(&alarm_, &wakeai::AlarmManager::alarmStopped, &player_, &QMediaPlayer::stop);

    QString ringtoneError;
    if (!ringtones_.initialize(&ringtoneError))
        statusBar()->showMessage(ringtoneError, 10000);

    auto *toolsMenu = menuBar()->addMenu("工具");
    toolsMenu->addAction("成就", this, &MainWindow::showAchievements);
    toolsMenu->addAction("数据文件位置", this, [this] {
        QMessageBox::information(this, "数据文件", db_.databasePath());
    });
    toolsMenu->addAction("铃声文件夹", this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ringtones_.directory()));
    });
    toolsMenu->addAction("重试保存本次记录", this, [this] {
        if (pending_)
            savePending();
        else
            QMessageBox::information(this, "记录", "没有待保存记录");
    });
    toolsMenu->addAction("选择摄像头", this, [this] {
        if (state_ != State::Idle || thread_)
            return;
        bool accepted = false;
        const int number = QInputDialog::getInt(this, "摄像头", "摄像头编号",
                                                options_.cameraIndex, 0, 20, 1, &accepted);
        if (accepted) {
            options_.cameraIndex = number;
            options_.videoPath.clear();
            statusBar()->showMessage("已选择摄像头 " + QString::number(number), 5000);
        }
    });
    toolsMenu->addAction("取消待响闹钟", this, [this] {
        if (state_ != State::Idle || alarm_.isRinging())
            return;
        setting_.enabled = false;
        if (!db_.saveAlarmSetting(setting_)) {
            QMessageBox::warning(this, "保存失败", db_.lastError());
            return;
        }
        alarm_.disable();
        ui->lblSetInfo->setText("闹钟已取消");
    });

    if (options_.testMode) {
        auto *testMenu = menuBar()->addMenu("测试");
        testMenu->addAction("10 秒后响铃", this, [this] { armAlarm(10); });
        testMenu->addAction("选择测试视频", this, [this] {
            if (state_ != State::Idle || thread_)
                return;
            const QString file = QFileDialog::getOpenFileName(
                this, "选择固定测试视频", {}, "视频 (*.mp4 *.avi *.mov *.mkv)");
            if (!file.isEmpty()) {
                options_.videoPath = file;
                statusBar()->showMessage("测试视频：" + file, 5000);
            }
        });
    }

    if (!db_.init()) {
        ui->btnSetAlarm->setEnabled(false);
        ui->lblSetInfo->setText("数据库初始化失败");
        QTimer::singleShot(0, this, [this] {
            QMessageBox::critical(this, "数据库错误", db_.lastError());
        });
    } else {
        setting_ = db_.loadSettings();
        if (ringtones_.item(setting_.ringtoneId).id.isEmpty())
            setting_.ringtoneId = ringtones_.defaultId();
        ui->timeEditAlarm->setTime(QTime(setting_.hour, setting_.minute));
        ui->spinTarget->setValue(setting_.targetCount);
        ui->comboMode->setCurrentIndex(setting_.exerciseType == "jumping_jack" ? 1
                                         : setting_.exerciseType == "cycling" ? 2 : 0);
        updateRingtoneLabel();
        if (setting_.enabled) {
            alarm_.setAlarm(setting_.hour, setting_.minute);
            alarm_.setChallenge(setting_.targetCount);
            alarm_.enable();
            scheduled_ = alarm_.nextTrigger();
        }
        ui->lblSetInfo->setText(setting_.enabled
                                   ? "待响：" + scheduled_.toString("MM-dd HH:mm")
                                   : "请设置闹钟");
        refreshRecords();
    }
}

MainWindow::~MainWindow()
{
    if (control_)
        control_->stop.store(true);
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

    const QString filename = "yolov8n-pose.onnx";
    const QStringList fixed = {
        QCoreApplication::applicationDirPath() + "/models/" + filename,
        QDir::currentPath() + "/models/" + filename,
    };
    for (const auto &path : fixed)
        if (QFileInfo::exists(path))
            return QFileInfo(path).absoluteFilePath();

    QDir directory(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 8; ++level) {
        const QString path = directory.filePath("models/" + filename);
        if (QFileInfo::exists(path))
            return path;
        if (!directory.cdUp())
            break;
    }
#ifdef WAKEAI_SOURCE_MODEL_DIR
    const QString fallback = QString::fromUtf8(WAKEAI_SOURCE_MODEL_DIR) + "/" + filename;
    if (QFileInfo::exists(fallback))
        return fallback;
#endif
    return {};
}

void MainWindow::switchPage(int index)
{
    if (ui->stackedWidget->currentIndex() == index)
        return;
    ui->stackedWidget->setCurrentIndex(index);
    QWidget *page = ui->stackedWidget->currentWidget();
    auto *opacity = new QGraphicsOpacityEffect(page);
    page->setGraphicsEffect(opacity);
    auto *animation = new QPropertyAnimation(opacity, "opacity", page);
    animation->setDuration(230);
    animation->setStartValue(0.25);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished, page, [page] {
        page->setGraphicsEffect(nullptr);
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::updateRingtoneLabel()
{
    RingtoneItem selected = ringtones_.item(setting_.ringtoneId);
    if (selected.id.isEmpty()) {
        setting_.ringtoneId = ringtones_.defaultId();
        selected = ringtones_.item(setting_.ringtoneId);
    }
    ui->lblRingtone->setText(
        QString("%1 · %2%").arg(selected.name.isEmpty() ? "经典闹铃" : selected.name)
            .arg(qRound(setting_.volume * 100.0)));
}

bool MainWindow::prepareAlarmAudio()
{
    RingtoneItem selected = ringtones_.item(setting_.ringtoneId);
    if (selected.id.isEmpty()) {
        setting_.ringtoneId = ringtones_.defaultId();
        selected = ringtones_.item(setting_.ringtoneId);
    }
    if (selected.path.isEmpty() || !QFileInfo::exists(selected.path)) {
        statusBar()->showMessage("找不到已选择的铃声文件，请重新选择", 8000);
        return false;
    }
    player_.stop();
    player_.setSource(QUrl::fromLocalFile(selected.path));
    player_.setLoops(QMediaPlayer::Infinite);
    audio_.setVolume(qBound(0.0, setting_.volume, 1.0));
    return true;
}

void MainWindow::on_btnChooseRingtone_clicked()
{
    RingtoneDialog dialog(ringtones_, setting_.ringtoneId, setting_.volume, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    setting_.ringtoneId = dialog.selectedId();
    setting_.volume = dialog.volume();
    updateRingtoneLabel();
    if (db_.isOpen() && !db_.saveAlarmSetting(setting_))
        QMessageBox::warning(this, "保存失败", db_.lastError());
}

void MainWindow::armAlarm(int seconds)
{
    if (state_ != State::Idle || thread_ || pending_ || !db_.isOpen()) {
        QMessageBox::information(this, "暂不能设置", "请先完成当前任务、等待识别停止并保存记录。");
        return;
    }
    setting_.hour = ui->timeEditAlarm->time().hour();
    setting_.minute = ui->timeEditAlarm->time().minute();
    setting_.exerciseType = typeFor(ui->comboMode->currentIndex());
    setting_.targetCount = ui->spinTarget->value();
    setting_.enabled = true;
    if (ringtones_.item(setting_.ringtoneId).id.isEmpty())
        setting_.ringtoneId = ringtones_.defaultId();
    if (!db_.saveAlarmSetting(setting_)) {
        QMessageBox::warning(this, "保存失败", db_.lastError());
        return;
    }
    alarm_.setAlarm(setting_.hour, setting_.minute);
    alarm_.setChallenge(setting_.targetCount);
    if (seconds > 0)
        alarm_.setTestAlarmInSeconds(seconds);
    alarm_.enable();
    scheduled_ = alarm_.nextTrigger();
    ui->lblSetInfo->setText("待响：" + scheduled_.toString("MM-dd HH:mm:ss") + " · " + modeName());
}

void MainWindow::on_btnSetAlarm_clicked()
{
    armAlarm();
}

void MainWindow::startRing()
{
    if (closing_)
        return;
    state_ = State::Ringing;
    sessionId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (prepareAlarmAudio())
        player_.play();
    ui->lblRingTime->setText(scheduled_.toString("HH:mm"));
    ui->lblTask->setText(modeName() + " " + QString::number(setting_.targetCount) + " 次");
    switchPage(1);
}

void MainWindow::on_btnStart_clicked()
{
    if (state_ != State::Ringing)
        return;
    state_ = State::Preparing;
    ui->lblReadyTask->setText(modeName() + " " + QString::number(setting_.targetCount) + " 次");
    ui->btnBegin->setEnabled(!thread_);
    switchPage(2);
}

void MainWindow::on_btnSnooze_clicked()
{
    if (state_ != State::Ringing || !alarm_.snooze())
        return;
    scheduled_ = alarm_.nextTrigger();
    state_ = State::Idle;
    ui->lblSetInfo->setText("稍后提醒：" + scheduled_.toString("MM-dd HH:mm:ss"));
    switchPage(0);
}

void MainWindow::on_btnBegin_clicked()
{
    if (state_ != State::Preparing || thread_ || closing_)
        return;
    const QString model = findModelPath();
    if (model.isEmpty() || !QFileInfo::exists(model)) {
        QMessageBox::warning(this, "找不到模型",
                             "请把 yolov8n-pose.onnx 放在仓库 models 目录，或使用 --model 指定。");
        return;
    }

    session_.begin(setting_.targetCount);
    paused_ = false;
    poseValid_ = false;
    state_ = State::Exercising;
    ui->btnPause->setEnabled(true);
    ui->btnPause->setText("暂停");
    ui->btnFinish->setEnabled(false);
    ui->lblExerciseTitle->setText(modeName() + "识别中");
    ui->lblState->setText("正在启动识别…");
    ui->cameraView->clearFrame();
    ui->cameraView->setCount(0, session_.target());
    ui->cameraView->setStatus("正在启动识别…", false);
    switchPage(3);

    control_ = std::make_shared<MotionControl>();
    MotionWorker::Options workerOptions;
    workerOptions.modelPath = model;
    workerOptions.videoPath = options_.videoPath;
    workerOptions.cameraIndex = options_.cameraIndex;
    workerOptions.mode = setting_.exerciseType == "jumping_jack"
        ? MotionWorker::Mode::JumpingJack
        : setting_.exerciseType == "cycling" ? MotionWorker::Mode::Cycling
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
        if (thread_ == workerThread && !closing_)
            updateCount(count);
    });
    connect(worker, &MotionWorker::stateChanged, this,
            [this, workerThread](const QString &message) {
                if (thread_ != workerThread || state_ != State::Exercising || session_.reached())
                    return;
                ui->lblState->setText(message);
                ui->cameraView->setStatus(message, poseValid_);
            });
    connect(worker, &MotionWorker::failed, this, [this, workerThread](const QString &message) {
        if (thread_ != workerThread || closing_)
            return;
        ui->lblReadyTask->setText(message + "；检查后点击开始识别重试");
        ui->cameraView->setStatus(message, false);
        statusBar()->showMessage(message, 10000);
    });
    connect(worker, &MotionWorker::poseValidChanged, this,
            [this, workerThread](bool valid) {
                if (thread_ != workerThread || state_ != State::Exercising || session_.reached() || paused_)
                    return;
                poseValid_ = valid;
                const QString message = valid ? "站位清晰，请继续完成动作"
                                              : "请后退一步，让全身进入画面";
                ui->lblState->setText(message);
                ui->cameraView->setStatus(message, valid);
            });
    connect(worker, &MotionWorker::wrongMotionHint, this, [this, workerThread] {
        if (thread_ != workerThread || state_ != State::Exercising || session_.reached() || paused_)
            return;
        const QString message = "本次未计入，请增大动作幅度";
        ui->lblState->setText(message);
        ui->cameraView->setStatus(message, false);
    });
    connect(worker, &MotionWorker::frameReady, this,
            [this, workerThread, control](const QImage &image) {
                if (thread_ == workerThread && !closing_)
                    ui->cameraView->setFrame(image);
                control->framePending.store(false);
            });
    connect(workerThread, &QThread::finished, this, [this, workerThread] {
        if (thread_ == workerThread) {
            thread_ = nullptr;
            control_.reset();
            if (state_ == State::Exercising && !session_.reached() && !closing_) {
                state_ = State::Preparing;
                ui->lblReadyNote->setText("识别已结束；重试将从 0 重新计数");
                ui->btnBegin->setEnabled(true);
                switchPage(2);
            }
        }
        workerThread->deleteLater();
        if (closing_)
            QTimer::singleShot(0, this, &QWidget::close);
    });
    workerThread->start();
}

void MainWindow::stopWorker()
{
    if (control_)
        control_->stop.store(true);
    if (thread_)
        thread_->quit();
}

void MainWindow::updateCount(int count)
{
    if (state_ != State::Exercising)
        return;
    const bool justReached = session_.updateCount(count);
    ui->cameraView->setCount(session_.count(), session_.target(), true);
    if (justReached) {
        alarm_.setChallengeCompleted(true);
        if (control_)
            control_->paused.store(true);
        ui->btnPause->setEnabled(false);
        ui->btnFinish->setEnabled(true);
        ui->lblState->setText("目标完成");
        ui->cameraView->setStatus("已达标，点击完成任务关闭闹钟", true);
    }
}

void MainWindow::on_btnPause_clicked()
{
    if (state_ != State::Exercising || session_.reached() || !control_)
        return;
    paused_ = !paused_;
    control_->paused.store(paused_);
    ui->btnPause->setText(paused_ ? "继续" : "暂停");
    ui->lblState->setText(paused_ ? "已暂停" : "识别中");
    ui->cameraView->setStatus(paused_ ? "已暂停" : "识别已继续", !paused_ && poseValid_);
}

void MainWindow::on_btnFinish_clicked()
{
    if (state_ != State::Exercising || !session_.reached() || session_.finished())
        return;
    if (!alarm_.stop())
        return;

    session_.finish();
    state_ = State::Done;
    stopWorker();
    setting_.enabled = false;
    const QDateTime now = QDateTime::currentDateTime();
    pendingRecord_ = {};
    pendingRecord_.date = now.date().toString("yyyy-MM-dd");
    pendingRecord_.alarmTime = scheduled_.toString("HH:mm");
    pendingRecord_.exerciseType = setting_.exerciseType;
    pendingRecord_.targetCount = session_.target();
    pendingRecord_.actualCount = session_.count();
    pendingRecord_.success = true;
    pendingRecord_.completedAt = now.toString("yyyy-MM-dd HH:mm:ss");
    pendingRecord_.sessionId = sessionId_;
    pending_ = true;
    ui->lblSummary->setText(modeName() + " " + QString::number(session_.count()) + " 次 已完成");
    ui->lblClosed->setText("闹钟已关闭");
    switchPage(4);
    savePending();
}

bool MainWindow::savePending()
{
    if (!pending_)
        return true;
    if (!db_.saveAlarmSetting(setting_) || !db_.saveWakeRecord(pendingRecord_)) {
        ui->lblClosed->setText("闹钟已关闭；记录保存失败，请通过工具菜单重试");
        QMessageBox::warning(this, "保存失败", db_.lastError());
        return false;
    }
    pending_ = false;
    const auto unlocked = achievements_.checkAndUnlock(db_.queryStatistics());
    ui->lblClosed->setText(unlocked.isEmpty()
                               ? "闹钟已关闭，记录已保存"
                               : "记录已保存，新解锁成就 " + QString::number(unlocked.size()) + " 个");
    refreshRecords();
    return true;
}

void MainWindow::refreshRecords()
{
    ui->listRecords->clear();
    for (const auto &record : db_.recentRecords()) {
        ui->listRecords->addItem(record.completedAt + " · " + nameFor(record.exerciseType)
                                 + " " + QString::number(record.actualCount) + "/"
                                 + QString::number(record.targetCount));
    }
    const auto statistics = db_.queryStatistics();
    ui->lblWeek->setText(QString("成功唤醒 %1 次 · 连续 %2 天 · 累计 %3 个动作")
                             .arg(statistics.totalWakeCount)
                             .arg(statistics.consecutiveDays)
                             .arg(statistics.totalActions));
    ui->lblStats->setText(QString("连续 %1 天 · %2 次成功唤醒")
                              .arg(statistics.consecutiveDays)
                              .arg(statistics.totalWakeCount));
}

void MainWindow::showAchievements()
{
    QStringList lines;
    for (const auto &rule : achievements_.rules()) {
        lines.append((db_.isAchievementUnlocked(rule.id) ? "已解锁 · " : "未解锁 · ")
                     + rule.name + "：" + rule.description);
    }
    QMessageBox::information(this, "成就", lines.join("\n\n"));
}

void MainWindow::on_btnRecords_clicked()
{
    refreshRecords();
    switchPage(5);
}

void MainWindow::on_btnHistory_clicked()
{
    on_btnRecords_clicked();
}

void MainWindow::on_btnHome_clicked()
{
    if (pending_ && !savePending())
        return;
    if (thread_) {
        QMessageBox::information(this, "正在停止", "请等待摄像头线程退出后返回首页。");
        return;
    }
    state_ = State::Idle;
    ui->lblSetInfo->setText("本次已完成，请设置下一次闹钟");
    switchPage(0);
}

void MainWindow::on_btnRecordBack_clicked()
{
    switchPage(state_ == State::Done ? 4 : 0);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!closeApproved_) {
        if (pending_ && !savePending()) {
            if (QMessageBox::question(this, "记录尚未保存",
                                      "保存仍失败。现在退出会丢失本次未保存记录，是否继续？",
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                != QMessageBox::Yes) {
                event->ignore();
                return;
            }
        }
        if (alarm_.isRinging() || thread_) {
            if (QMessageBox::question(this, "退出确认",
                                      "退出会结束当前任务，未完成的任务不会记为成功。是否退出？",
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                != QMessageBox::Yes) {
                event->ignore();
                return;
            }
        }
        closeApproved_ = true;
    }
    if (thread_) {
        closing_ = true;
        stopWorker();
        player_.stop();
        statusBar()->showMessage("正在等待摄像头线程退出…");
        event->ignore();
        return;
    }
    event->accept();
}
