#include "ui/MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QImage>
#include <QPixmap>

namespace wakeai {

    MainWindow::MainWindow(const std::string& modelPath, QWidget* parent)
        : QMainWindow(parent), modelPath_(modelPath) {
        setupUi();

        if (!detector_.load(modelPath_)) {
            statusLabel_->setText("模型加载失败！请检查 models/yolov8n-pose.onnx");
            startBtn_->setEnabled(false);
        }

        // 三个动作的默认参数（与测试工具一致，以后可在这里调）
        squat_.setThresholds(140.0, 155.0, 2);
        jumpingJack_.setThresholds(0.45f, -0.25f, 1.25f, 0.75f, 3);
        cycling_.setPartialBodyConfig(0.22f, 2, 5, 4);
        cycling_.setCalibrationConfig(15, 0.10f, 0.30f, 0.30f);
        cycling_.setCountMode(CyclingCountMode::EachPedal);

        active_ = &squat_;  // 默认深蹲

        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &MainWindow::tick);
    }

    void MainWindow::setupUi() {
        setWindowTitle("WakeAI - 智能运动唤醒");

        auto* central = new QWidget(this);
        auto* vbox = new QVBoxLayout(central);

        // 摄像头预览
        previewLabel_ = new QLabel("摄像头未开启", central);
        previewLabel_->setAlignment(Qt::AlignCenter);
        previewLabel_->setMinimumSize(640, 480);
        previewLabel_->setStyleSheet("background-color: black; color: white;");
        vbox->addWidget(previewLabel_);

        // 控制行
        auto* hbox = new QHBoxLayout();
        hbox->addWidget(new QLabel("动作:", central));

        modeBox_ = new QComboBox(central);
        modeBox_->addItem("深蹲");
        modeBox_->addItem("开合跳");
        modeBox_->addItem("床上蹬腿");
        hbox->addWidget(modeBox_);

        hbox->addWidget(new QLabel("目标次数:", central));
        targetBox_ = new QSpinBox(central);
        targetBox_->setRange(1, 1000);
        targetBox_->setValue(20);
        hbox->addWidget(targetBox_);

        mirrorBox_ = new QCheckBox("镜像", central);
        mirrorBox_->setChecked(true);
        hbox->addWidget(mirrorBox_);

        startBtn_ = new QPushButton("开始", central);
        hbox->addWidget(startBtn_);

        hbox->addStretch();
        vbox->addLayout(hbox);

        // 计数 + 进度
        countLabel_ = new QLabel("0 / 20", central);
        countLabel_->setStyleSheet("font-size: 24px; font-weight: bold;");
        vbox->addWidget(countLabel_);

        progressBar_ = new QProgressBar(central);
        progressBar_->setRange(0, 100);
        progressBar_->setValue(0);
        vbox->addWidget(progressBar_);

        statusLabel_ = new QLabel("点击「开始」启动识别", central);
        vbox->addWidget(statusLabel_);

        setCentralWidget(central);

        connect(startBtn_, &QPushButton::clicked, this, &MainWindow::onStartStop);
        connect(modeBox_, &QComboBox::currentIndexChanged, this, &MainWindow::onModeChanged);
    }

    ExerciseBase* MainWindow::currentExercise() {
        switch (modeBox_->currentIndex()) {
        case 0: return &squat_;
        case 1: return &jumpingJack_;
        case 2: return &cycling_;
        }
        return &squat_;
    }

    void MainWindow::onModeChanged() {
        active_ = currentExercise();
        active_->reset();
        completedShown_ = false;
        countLabel_->setStyleSheet("font-size: 24px; font-weight: bold;");
        countLabel_->setText("0 / " + QString::number(targetBox_->value()));
        progressBar_->setValue(0);
        statusLabel_->setText("已切换动作");
    }

    void MainWindow::onStartStop() {
        if (!running_) {
            if (!cap_.isOpened()) {
                cap_.open(0);
            }
            if (!cap_.isOpened()) {
                QMessageBox::warning(this, "错误", "无法打开摄像头");
                return;
            }
            active_ = currentExercise();
            active_->reset();
            completedShown_ = false;
            running_ = true;
            startBtn_->setText("停止");
            statusLabel_->setText("识别中...");
            timer_->start(30);
        }
        else {
            running_ = false;
            timer_->stop();
            startBtn_->setText("开始");
            statusLabel_->setText("已停止");
        }
    }

    void MainWindow::tick() {
        cv::Mat frame;
        cap_ >> frame;
        if (frame.empty()) return;

        if (mirrorBox_->isChecked()) {
            cv::flip(frame, frame, 1);
        }

        PoseLandmarks rawPose;
        const bool detected = detector_.detect(frame, rawPose);

        PoseLandmarks smoothPose;
        if (detected) {
            smoothPose = smoother_.update(rawPose);
        }
        else {
            smoother_.update(PoseLandmarks{});   // 关键点丢失，喂空帧
        }

        active_->update(smoothPose);

        if (detected) {
            drawPose(frame, smoothPose);
        }

        const int count = active_->count();
        const int target = targetBox_->value();
        const float progress = active_->progress();

        countLabel_->setText(QString("%1 / %2").arg(count).arg(target));
        progressBar_->setValue(static_cast<int>(progress * 100.0f));

        if (count >= target && !completedShown_) {
            completedShown_ = true;
            countLabel_->setStyleSheet("font-size: 24px; font-weight: bold; color: red;");
            statusLabel_->setText("已完成目标！");
            QMessageBox::information(this, "完成", "太棒了，你完成了目标！");
        }

        previewLabel_->setPixmap(
            QPixmap::fromImage(matToQImage(frame))
            .scaled(previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QImage MainWindow::matToQImage(const cv::Mat& mat) const {
        if (mat.empty()) return QImage();
        if (mat.type() == CV_8UC3) {
            cv::Mat rgb;
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
            return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
        }
        if (mat.type() == CV_8UC1) {
            return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
        }
        return QImage();
    }

    void MainWindow::drawPose(cv::Mat& frame, const PoseLandmarks& pose) {
        static const std::pair<int, int> bones[] = {
            {LeftShoulder, RightShoulder},
            {LeftShoulder, LeftElbow},   {LeftElbow, LeftWrist},
            {RightShoulder, RightElbow}, {RightElbow, RightWrist},
            {LeftShoulder, LeftHip},     {RightShoulder, RightHip},
            {LeftHip, RightHip},
            {LeftHip, LeftKnee},         {LeftKnee, LeftAnkle},
            {RightHip, RightKnee},       {RightKnee, RightAnkle},
        };
        for (const auto& b : bones) {
            const Keypoint& a = pose[b.first];
            const Keypoint& c = pose[b.second];
            if (a.visible() && c.visible()) {
                cv::line(frame, cv::Point((int)a.x, (int)a.y),
                    cv::Point((int)c.x, (int)c.y), cv::Scalar(0, 255, 0), 2);
            }
        }
        for (int i = 0; i < PoseLandmarks::kCount; ++i) {
            if (pose[i].visible()) {
                cv::circle(frame, cv::Point((int)pose[i].x, (int)pose[i].y),
                    3, cv::Scalar(0, 0, 255), -1);
            }
        }
    }

} // namespace wakeai
