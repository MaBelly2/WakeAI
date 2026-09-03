#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>

#include <opencv2/opencv.hpp>

#include "core/PoseData.h"
#include "core/PoseDetector.h"
#include "core/PoseSmoother.h"
#include "exercise/ExerciseBase.h"
#include "exercise/Squat.h"
#include "exercise/JumpingJack.h"
#include "exercise/Cycling.h"

namespace wakeai {

    class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(const std::string& modelPath, QWidget* parent = nullptr);

    private slots:
        void onStartStop();
        void onModeChanged();
        void tick();

    private:
        void setupUi();
        void drawPose(cv::Mat& frame, const PoseLandmarks& pose);
        QImage matToQImage(const cv::Mat& mat) const;
        ExerciseBase* currentExercise();

        // 界面控件
        QLabel* previewLabel_ = nullptr;
        QComboBox* modeBox_ = nullptr;
        QSpinBox* targetBox_ = nullptr;
        QCheckBox* mirrorBox_ = nullptr;
        QPushButton* startBtn_ = nullptr;
        QLabel* countLabel_ = nullptr;
        QProgressBar* progressBar_ = nullptr;
        QLabel* statusLabel_ = nullptr;

        // 算法组件
        cv::VideoCapture cap_;
        PoseDetector detector_;
        PoseSmoother smoother_;
        Squat squat_;
        JumpingJack jumpingJack_;
        Cycling cycling_;
        ExerciseBase* active_ = nullptr;

        QTimer* timer_ = nullptr;
        bool running_ = false;
        bool completedShown_ = false;
        std::string modelPath_;
    };

} // namespace wakeai
