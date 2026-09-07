#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QUrl>
#include <QCoreApplication>
#include <QPixmap>
#include <QImage>

static QString findModelPath()
{
    const QStringList candidates = {
        "models/yolov8n-pose.onnx",
        QCoreApplication::applicationDirPath() + "/models/yolov8n-pose.onnx",
        "wakeai/models/yolov8n-pose.onnx",
    };
    for (const QString &c : candidates)
        if (QFile::exists(c)) return c;
    return "models/yolov8n-pose.onnx";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);



    this->setStyleSheet(QString::fromUtf8(R"(
    QMainWindow, QWidget#centralwidget { background: #F5F7FA; }
    QLabel { color: #333333; font-size: 16px; }
    QLabel#lblProgress { font-size: 32px; font-weight: bold; color: #4A90D9; }
    QPushButton {
        background-color: #4A90D9;
        color: white;
        font-size: 15px;
        border-radius: 8px;
        padding: 10px 20px;
    }
    QPushButton:hover   { background-color: #3A80C9; }
    QPushButton:pressed { background-color: #2F6FB8; }
    QPushButton:disabled {
        background-color: #C9D2DC;
        color: #FFFFFF;
    }
    QPushButton#btnSnooze {
        background-color: #F0F2F5;
        color: #4A90D9;
        border: 1px solid #4A90D9;
    }
    QTimeEdit, QComboBox, QSpinBox {
        background-color: white;
        border: 1px solid #D0D7DE;
        border-radius: 6px;
        padding: 4px;
        font-size: 14px;
    }
    QListWidget {
        background-color: white;
        border: 1px solid #D0D7DE;
        border-radius: 8px;
        font-size: 14px;
    }
)"));

    clockTimer = new QTimer(this);
    clockTimer->setInterval(1000);
    connect(clockTimer, &QTimer::timeout, this, &MainWindow::checkTime);
    clockTimer->start();

    ringSound = new QSoundEffect(this);
    ringSound->setSource(QUrl::fromLocalFile("C:/Windows/Media/tada.wav"));
    // 换成你机器上实际存在的 wav 路径

    worker = new MotionWorker;
    workerThread = new QThread(this);
    worker->moveToThread(workerThread);
    connect(workerThread, &QThread::started, worker, &MotionWorker::start);
    connect(worker, &MotionWorker::countChanged, this, &MainWindow::onCountChanged);
    connect(worker, &MotionWorker::wrongMotionHint, this, &MainWindow::onWrongMotionHint);
    connect(worker, &MotionWorker::finished, workerThread, &QThread::quit);
    // 注意：不 deleteLater worker —— 这样线程结束后还能再次 start
    connect(worker, &MotionWorker::frameReady, this,
            [this](const QImage &img){
                ui->lblCamera->setPixmap(QPixmap::fromImage(img));
            });
    // 读历史记录
    ui->listRecords->addItems(
        QSettings().value("records").toStringList());

    ui->stackedWidget->setCurrentIndex(0);
    ui->btnFinish->setEnabled(false);
}

MainWindow::~MainWindow()
{
    if (worker) worker->stop();
    if (workerThread && workerThread->isRunning())
        workerThread->wait(2000);
    delete ui;
}

QString MainWindow::modeName()
{
    switch (ui->comboMode->currentIndex()) {
    case 1: return "开合跳";
    case 2: return "蹬腿";
    default: return "蹲起";
    }
}

// ---- 定时器：每秒刷新 + 到点触发 ----
void MainWindow::checkTime()
{
    QTime now = QTime::currentTime();
    ui->lblRingTime->setText(now.toString("HH:mm"));

    if (state == Idle && !alarmTriggered_
        && now.hour() == alarmTime.hour()
        && now.minute() == alarmTime.minute()) {
        alarmTriggered_ = true;
        startRing();
    }
}

void MainWindow::startRing()
{
    state = Ringing;
    ringSound->setLoopCount(QSoundEffect::Infinite);
    ringSound->play();

    ui->lblRingTime->setText(alarmTime.toString("HH:mm"));
    ui->lblTask->setText("今日任务：" + modeName()
                         + " " + QString::number(targetCount_) + "个");
    ui->stackedWidget->setCurrentIndex(1);
}

// ---- 第0页 ----
void MainWindow::on_btnSetAlarm_clicked()
{
    alarmTime = ui->timeEditAlarm->time();
    targetCount_ = ui->spinTarget->value();
    alarmTriggered_ = false;
    state = Idle;
    ui->lblSetInfo->setText("已设置 " + alarmTime.toString("HH:mm"));
}

void MainWindow::on_btnRecords_clicked()
{
    ui->stackedWidget->setCurrentIndex(5);
}

// ---- 第1页 ----
void MainWindow::on_btnStart_clicked()
{
    ui->lblReadyTask->setText(modeName() + " "
                              + QString::number(targetCount_) + "个");
    state = Preparing;
    ui->stackedWidget->setCurrentIndex(2);   // 准备页
}

void MainWindow::on_btnSnooze_clicked()
{
    ringSound->stop();
    worker->stop();
    alarmTime = QTime::currentTime().addSecs(300);   // 5 分钟后
    alarmTriggered_ = false;
    state = Idle;
    ui->lblSetInfo->setText("稍后提醒：" + alarmTime.toString("HH:mm"));
    ui->stackedWidget->setCurrentIndex(0);
}

// ---- 第2页 ----
void MainWindow::on_btnBegin_clicked()
{
    MotionWorker::Mode m = MotionWorker::Mode::Squat;
    if (ui->comboMode->currentIndex() == 1)
        m = MotionWorker::Mode::JumpingJack;
    else if (ui->comboMode->currentIndex() == 2)
        m = MotionWorker::Mode::Cycling;

    worker->setMode(m);
    worker->setModelPath(findModelPath());

    if (!workerThread->isRunning())
        workerThread->start();

    state = Exercising;
    ui->lblProgress->setText("0/" + QString::number(targetCount_));
    ui->lblState->setText(modeName() + "进行中...");
    ui->btnFinish->setEnabled(false);
    ui->btnPause->setText("暂停");
    paused_ = false;
    ui->stackedWidget->setCurrentIndex(3);   // 进行页
}

// ---- 第3页 ----
void MainWindow::on_btnPause_clicked()
{
    paused_ = !paused_;
    if (paused_) { worker->pause(); ui->btnPause->setText("继续"); }
    else         { worker->resume(); ui->btnPause->setText("暂停"); }
}

void MainWindow::onCountChanged(int count)
{
    ui->lblProgress->setText(QString("%1/%2")
                                 .arg(count).arg(targetCount_));
    if (count >= targetCount_) {
        state = Done;
        ui->lblState->setText("任务完成！");
        ui->btnFinish->setEnabled(true);   // 解锁“完成任务”
    }
}

void MainWindow::onWrongMotionHint()
{
    if (state == Exercising)
        ui->lblState->setText("动作似乎不对，请按页面提示重新做！");
}

void MainWindow::on_btnFinish_clicked()
{
    if (state != Done) {                    // 未达标不允许关闭
        ui->lblState->setText("还没达标，不能关闭闹钟！");
        return;
    }

    ringSound->stop();
    worker->stop();

    appendRecord();
    ui->lblSummary->setText(modeName() + " "
                            + QString::number(targetCount_) + "个 已完成");
    ui->lblClosed->setText("闹钟已关闭");
    ui->stackedWidget->setCurrentIndex(4);  // 完成页
}

// ---- 第4页 ----
void MainWindow::on_btnHistory_clicked()
{
    ui->stackedWidget->setCurrentIndex(5);
}

void MainWindow::on_btnHome_clicked()
{
    if (worker) worker->stop();          // 让识别循环退出（线程会自动 quit）
    if (paused_) {                       // 复位暂停状态
        paused_ = false;
        ui->btnPause->setText("暂停");
    }
    state = Idle;
    alarmTriggered_ = false;
    ui->btnFinish->setEnabled(false);
    ui->stackedWidget->setCurrentIndex(0);
}
// ---- 第5页 ----
void MainWindow::on_btnRecordBack_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

// ---- 历史记录 ----
void MainWindow::appendRecord()
{
    QString s = QDateTime::currentDateTime().toString("ddd HH:mm")
    + " " + modeName() + " "
        + QString::number(targetCount_) + "个 完成";

    ui->listRecords->addItem(s);

    // 同时存到 QSettings，下次启动还能显示
    QSettings settings;
    QStringList list = settings.value("records").toStringList();
    list.prepend(s);
    settings.setValue("records", list);
}