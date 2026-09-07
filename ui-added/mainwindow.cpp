#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QStringList>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QPixmap>
#include <algorithm>
#include <utility>
namespace {
QString databasePath(bool test) {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        +(test?"/wakeai-test.db":"/wakeai.db");
}
QString typeFor(int index) {return index==1?"jumping_jack":index==2?"cycling":"squat";}
QString nameFor(const QString& type) {
    return type=="jumping_jack"?"开合跳":type=="cycling"?"床上蹬腿":type=="squat"?"深蹲":type;
}
}
MainWindow::MainWindow(StartupOptions options,QWidget* parent)
    :QMainWindow(parent),ui(new Ui::MainWindow),options_(std::move(options)),
     db_(databasePath(options_.testMode)),achievements_(db_),alarm_(this) {
    ui->setupUi(this);
    setWindowTitle(options_.testMode?"WakeAI · 测试模式（独立数据）":"WakeAI · 智能运动唤醒");
    resize(720,720);
    setStyleSheet("QMainWindow,QWidget#centralwidget{background:#F5F7FA;}"
        "QLabel{color:#333;font-size:16px;}QLabel#lblProgress{font-size:30px;font-weight:bold;color:#287C9A;}"
        "QPushButton{background:#368DA9;color:white;border-radius:7px;padding:8px 14px;}"
        "QPushButton:disabled{background:#BAC8CE;}QListWidget{background:white;}");
    ui->btnSnooze->setText("稍后提醒（5 分钟）");
    ui->btnBegin->setText("开始识别");
    ui->lblReadyNote->setText("达标后点击“完成任务”关闭闹钟");
    ui->lblCamera->setMinimumSize(320,240);
    ui->lblCamera->setAlignment(Qt::AlignCenter);
    ui->comboMode->setItemText(0,"深蹲");ui->comboMode->setItemText(1,"开合跳");ui->comboMode->setItemText(2,"床上蹬腿");
    ui->stackedWidget->setCurrentIndex(0);ui->btnFinish->setEnabled(false);
    sound_=new QSoundEffect(this);
    sound_->setSource(QUrl("qrc:/audio/alarm.wav"));sound_->setLoopCount(QSoundEffect::Infinite);
    connect(sound_,&QSoundEffect::statusChanged,this,[this]{
        if(sound_->status()==QSoundEffect::Error) statusBar()->showMessage("铃声加载失败，请检查 Qt Multimedia 与音频设备");
    });
    connect(&alarm_,&wakeai::AlarmManager::alarmTriggered,this,&MainWindow::startRing);
    connect(&alarm_,&wakeai::AlarmManager::alarmStopped,sound_,&QSoundEffect::stop);
    auto* menu=menuBar()->addMenu("工具");
    menu->addAction("成就",this,&MainWindow::showAchievements);
    menu->addAction("数据文件位置",this,[this]{QMessageBox::information(this,"数据文件",db_.databasePath());});
    menu->addAction("重试保存本次记录",this,[this]{if(pending_)savePending();else QMessageBox::information(this,"记录","没有待保存记录");});
    menu->addAction("选择摄像头",this,[this]{
        if(state_!=State::Idle||thread_)return;
        bool ok=false;int n=QInputDialog::getInt(this,"摄像头","摄像头编号",options_.cameraIndex,0,20,1,&ok);
        if(ok){options_.cameraIndex=n;options_.videoPath.clear();statusBar()->showMessage("已选择摄像头 "+QString::number(n));}
    });
    menu->addAction("取消待响闹钟",this,[this]{
        if(state_!=State::Idle||alarm_.isRinging())return;
        setting_.enabled=false;
        if(!db_.saveAlarmSetting(setting_)){QMessageBox::warning(this,"保存失败",db_.lastError());return;}
        alarm_.disable();ui->lblSetInfo->setText("闹钟已取消");
    });
    if(options_.testMode) {
        auto* test=menuBar()->addMenu("测试");
        test->addAction("10 秒后响铃",this,[this]{armAlarm(10);});
        test->addAction("选择测试视频",this,[this]{
            if(state_!=State::Idle||thread_)return;
            auto f=QFileDialog::getOpenFileName(this,"选择固定测试视频",{},"视频 (*.mp4 *.avi *.mov)");
            if(!f.isEmpty()){options_.videoPath=f;statusBar()->showMessage("测试视频："+f);}
        });
    }
    if(!db_.init()) {
        ui->btnSetAlarm->setEnabled(false);
        ui->lblSetInfo->setText("数据库初始化失败");
        QTimer::singleShot(0,this,[this]{QMessageBox::critical(this,"数据库错误",db_.lastError());});
    } else {
        setting_=db_.loadSettings();
        ui->timeEditAlarm->setTime(QTime(setting_.hour,setting_.minute));
        ui->spinTarget->setValue(setting_.targetCount);
        ui->comboMode->setCurrentIndex(setting_.exerciseType=="jumping_jack"?1:setting_.exerciseType=="cycling"?2:0);
        if(setting_.enabled){alarm_.setAlarm(setting_.hour,setting_.minute);alarm_.enable();scheduled_=alarm_.nextTrigger();}
        ui->lblSetInfo->setText(setting_.enabled?"待响："+scheduled_.toString("MM-dd HH:mm"):"请设置闹钟");
        refreshRecords();
    }
}
MainWindow::~MainWindow() {
    // Normal window close is asynchronous. This is a final lifetime guard.
    if(control_)control_->stop.store(true);
    if(thread_){thread_->quit();thread_->wait();}
    delete ui;
}
QString MainWindow::modeName() const {return nameFor(setting_.exerciseType);}
QString MainWindow::findModelPath() const {
    if(!options_.modelPath.isEmpty()) return QFileInfo(options_.modelPath).absoluteFilePath();
    const QString filename="yolov8n-pose.onnx";
    const QStringList fixed={QCoreApplication::applicationDirPath()+"/models/"+filename,
        QDir::currentPath()+"/models/"+filename};
    for(const auto& p:fixed)if(QFileInfo::exists(p))return QFileInfo(p).absoluteFilePath();
    QDir d(QCoreApplication::applicationDirPath());
    for(int i=0;i<8;++i){auto p=d.filePath("models/"+filename);if(QFileInfo::exists(p))return p;if(!d.cdUp())break;}
#ifdef WAKEAI_SOURCE_MODEL_DIR
    const QString fallback=QString::fromUtf8(WAKEAI_SOURCE_MODEL_DIR)+"/"+filename;
    if(QFileInfo::exists(fallback))return fallback;
#endif
    return {};
}
void MainWindow::armAlarm(int seconds) {
    if(state_!=State::Idle||thread_||pending_||!db_.isOpen()) {
        QMessageBox::information(this,"暂不能设置","请先完成当前任务、等待识别停止并保存记录。");return;
    }
    setting_.hour=ui->timeEditAlarm->time().hour();setting_.minute=ui->timeEditAlarm->time().minute();
    setting_.exerciseType=typeFor(ui->comboMode->currentIndex());setting_.targetCount=ui->spinTarget->value();
    setting_.enabled=true;
    if(!db_.saveAlarmSetting(setting_)){QMessageBox::warning(this,"保存失败",db_.lastError());return;}
    alarm_.setAlarm(setting_.hour,setting_.minute);alarm_.setChallenge(setting_.targetCount);
    if(seconds>0)alarm_.setTestAlarmInSeconds(seconds);
    alarm_.enable();scheduled_=alarm_.nextTrigger();
    ui->lblSetInfo->setText("待响："+scheduled_.toString("MM-dd HH:mm:ss")+" · "+modeName());
}
void MainWindow::on_btnSetAlarm_clicked(){armAlarm();}
void MainWindow::startRing() {
    if(closing_)return;
    state_=State::Ringing;sessionId_=QUuid::createUuid().toString(QUuid::WithoutBraces);
    sound_->play();ui->lblRingTime->setText(scheduled_.toString("HH:mm"));
    ui->lblTask->setText(modeName()+" "+QString::number(setting_.targetCount)+" 次");
    ui->stackedWidget->setCurrentIndex(1);
}
void MainWindow::on_btnStart_clicked() {
    if(state_!=State::Ringing)return;
    state_=State::Preparing;
    ui->lblReadyTask->setText(modeName()+" "+QString::number(setting_.targetCount)+" 次");
    ui->btnBegin->setEnabled(!thread_);ui->stackedWidget->setCurrentIndex(2);
}
void MainWindow::on_btnSnooze_clicked() {
    if(state_!=State::Ringing||!alarm_.snooze())return;
    scheduled_=alarm_.nextTrigger();state_=State::Idle;
    ui->lblSetInfo->setText("稍后提醒："+scheduled_.toString("MM-dd HH:mm:ss"));ui->stackedWidget->setCurrentIndex(0);
}
void MainWindow::on_btnBegin_clicked() {
    if(state_!=State::Preparing||thread_||closing_)return;
    auto model=findModelPath();
    if(model.isEmpty()||!QFileInfo::exists(model)) {
        QMessageBox::warning(this,"找不到模型","请把 yolov8n-pose.onnx 放在仓库 models 目录，或使用 --model 指定。");return;
    }
    session_.begin(setting_.targetCount);paused_=false;state_=State::Exercising;
    ui->btnPause->setEnabled(true);ui->btnPause->setText("暂停");ui->btnFinish->setEnabled(false);
    ui->lblProgress->setText("0/"+QString::number(session_.target()));
    ui->lblState->setText("正在启动识别...");ui->stackedWidget->setCurrentIndex(3);
    control_=std::make_shared<MotionControl>();
    MotionWorker::Options opts;opts.modelPath=model;opts.videoPath=options_.videoPath;opts.cameraIndex=options_.cameraIndex;
    opts.mode=setting_.exerciseType=="jumping_jack"?MotionWorker::Mode::JumpingJack:
        setting_.exerciseType=="cycling"?MotionWorker::Mode::Cycling:MotionWorker::Mode::Squat;
    auto* t=new QThread(this);auto* w=new MotionWorker(opts,control_);thread_=t;w->moveToThread(t);
    auto c=control_;
    connect(t,&QThread::started,w,&MotionWorker::start);
    // quit() is thread-safe. Direct delivery does not depend on the UI event loop.
    connect(w,&MotionWorker::finished,t,&QThread::quit,Qt::DirectConnection);
    connect(t,&QThread::finished,w,&QObject::deleteLater);
    connect(w,&MotionWorker::countChanged,this,[this,t](int count){if(thread_==t&&!closing_)updateCount(count);});
    connect(w,&MotionWorker::stateChanged,this,[this,t](const QString& s){
        if(thread_==t&&state_==State::Exercising&&!session_.reached())ui->lblState->setText(s);
    });
    connect(w,&MotionWorker::failed,this,[this,t](const QString& s){
        if(thread_!=t||closing_)return;
        ui->lblReadyTask->setText(s+"；检查后点击开始识别重试");statusBar()->showMessage(s);
    });
    connect(w,&MotionWorker::poseValidChanged,this,[this,t](bool valid){
        if(thread_==t&&state_==State::Exercising&&!session_.reached()&&!paused_)
            ui->lblState->setText(valid?"关键点有效，请继续完成动作":"关键点不足，请调整位置与光线");
    });
    connect(w,&MotionWorker::wrongMotionHint,this,[this,t]{
        if(thread_==t&&state_==State::Exercising&&!session_.reached()&&!paused_)
            ui->lblState->setText("暂未计入新次数，请检查动作幅度、站位或标定状态");
    });
    connect(w,&MotionWorker::frameReady,this,[this,t,c](const QImage& image){
        if(thread_==t&&!closing_)ui->lblCamera->setPixmap(QPixmap::fromImage(image).scaled(
            ui->lblCamera->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
        c->framePending.store(false);
    });
    connect(t,&QThread::finished,this,[this,t]{
        if(thread_==t){thread_=nullptr;control_.reset();
            if(state_==State::Exercising&&!session_.reached()&&!closing_){
                state_=State::Preparing;ui->lblReadyNote->setText("识别已结束；重试将从 0 重新计数");
                ui->btnBegin->setEnabled(true);ui->stackedWidget->setCurrentIndex(2);
            }
        }
        t->deleteLater();if(closing_)QTimer::singleShot(0,this,&QWidget::close);
    });
    t->start();
}
void MainWindow::stopWorker(){if(control_)control_->stop.store(true);if(thread_)thread_->quit();}
void MainWindow::updateCount(int count) {
    if(state_!=State::Exercising)return;
    const bool justReached=session_.updateCount(count);
    ui->lblProgress->setText(QString("%1/%2").arg(session_.count()).arg(session_.target()));
    if(justReached){
        alarm_.setChallengeCompleted(true);if(control_)control_->paused.store(true);
        ui->btnPause->setEnabled(false);ui->btnFinish->setEnabled(true);
        ui->lblState->setText("已达标，请点击“完成任务”关闭闹钟");
    }
}
void MainWindow::on_btnPause_clicked(){
    if(state_!=State::Exercising||session_.reached()||!control_)return;
    paused_=!paused_;control_->paused.store(paused_);ui->btnPause->setText(paused_?"继续":"暂停");
}
void MainWindow::on_btnFinish_clicked() {
    if(state_!=State::Exercising||!session_.reached()||session_.finished())return;
    if(!alarm_.stop())return;
    session_.finish();state_=State::Done;stopWorker();setting_.enabled=false;
    auto now=QDateTime::currentDateTime();pendingRecord_={};
    pendingRecord_.date=now.date().toString("yyyy-MM-dd");pendingRecord_.alarmTime=scheduled_.toString("HH:mm");
    pendingRecord_.exerciseType=setting_.exerciseType;pendingRecord_.targetCount=session_.target();
    pendingRecord_.actualCount=session_.count();pendingRecord_.success=true;
    pendingRecord_.completedAt=now.toString("yyyy-MM-dd HH:mm:ss");pendingRecord_.sessionId=sessionId_;pending_=true;
    ui->lblSummary->setText(modeName()+" "+QString::number(session_.count())+" 次 已完成");
    ui->lblClosed->setText("闹钟已关闭");ui->stackedWidget->setCurrentIndex(4);savePending();
}
bool MainWindow::savePending() {
    if(!pending_)return true;
    if(!db_.saveAlarmSetting(setting_)||!db_.saveWakeRecord(pendingRecord_)) {
        ui->lblClosed->setText("闹钟已关闭；记录保存失败，请通过工具菜单重试");
        QMessageBox::warning(this,"保存失败",db_.lastError());return false;
    }
    pending_=false;const auto ids=achievements_.checkAndUnlock(db_.queryStatistics());
    ui->lblClosed->setText(ids.isEmpty()?"闹钟已关闭，记录已保存":"记录已保存，新解锁成就 "+QString::number(ids.size())+" 个");
    refreshRecords();return true;
}
void MainWindow::refreshRecords() {
    ui->listRecords->clear();
    for(const auto& r:db_.recentRecords())ui->listRecords->addItem(
        r.completedAt+" · "+nameFor(r.exerciseType)+" "+QString::number(r.actualCount)+"/"+QString::number(r.targetCount));
    const auto s=db_.queryStatistics();
    ui->lblWeek->setText(QString("成功唤醒 %1 次 · 连续 %2 天 · 累计 %3 个动作").arg(s.totalWakeCount).arg(s.consecutiveDays).arg(s.totalActions));
}
void MainWindow::showAchievements() {
    QStringList lines;
    for(const auto& r:achievements_.rules())lines.append((db_.isAchievementUnlocked(r.id)?"已解锁 · ":"未解锁 · ")+r.name+"："+r.description);
    QMessageBox::information(this,"成就",lines.join("\n\n"));
}
void MainWindow::on_btnRecords_clicked(){refreshRecords();ui->stackedWidget->setCurrentIndex(5);}
void MainWindow::on_btnHistory_clicked(){on_btnRecords_clicked();}
void MainWindow::on_btnHome_clicked(){
    if(pending_&&!savePending())return;
    if(thread_){QMessageBox::information(this,"正在停止","请等待摄像头线程退出后返回首页。");return;}
    state_=State::Idle;ui->stackedWidget->setCurrentIndex(0);ui->lblSetInfo->setText("本次已完成，请设置下一次闹钟");
}
void MainWindow::on_btnRecordBack_clicked(){ui->stackedWidget->setCurrentIndex(state_==State::Done?4:0);}
void MainWindow::closeEvent(QCloseEvent* event) {
    if(!closeApproved_){
        if(pending_&&!savePending()) {
            if(QMessageBox::question(this,"记录尚未保存","保存仍失败。现在退出会丢失本次未保存记录，是否继续？",
                QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){event->ignore();return;}
        }
        if(alarm_.isRinging()||thread_){
            if(QMessageBox::question(this,"退出确认","退出会结束当前任务，未完成的任务不会记为成功。是否退出？",
                QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){event->ignore();return;}
        }
        closeApproved_=true;
    }
    if(thread_){closing_=true;stopWorker();sound_->stop();statusBar()->showMessage("正在等待摄像头线程退出...");event->ignore();return;}
    event->accept();
}
