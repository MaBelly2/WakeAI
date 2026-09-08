#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QTimer>
#include <QWidget>

class CameraView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgress)
    Q_PROPERTY(qreal countPulse READ countPulse WRITE setCountPulse)

public:
    explicit CameraView(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    void setCount(int count, int target, bool animate = true);
    void setStatus(const QString &status, bool poseValid);
    void clearFrame();

    qreal progress() const { return progress_; }
    void setProgress(qreal value);
    qreal countPulse() const { return countPulse_; }
    void setCountPulse(qreal value);

    QSize sizeHint() const override { return QSize(840, 500); }
    int heightForWidth(int width) const override { return qRound(width * 9.0 / 16.0); }
    bool hasHeightForWidth() const override { return true; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF imageTargetRect() const;

    QImage frame_;
    QString status_ = QStringLiteral("等待摄像头");
    int count_ = 0;
    int target_ = 1;
    bool poseValid_ = false;
    qreal progress_ = 0.0;
    qreal countPulse_ = 1.0;
};

class RingPulseWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RingPulseWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(250, 250); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer timer_;
    QElapsedTimer clock_;
};
