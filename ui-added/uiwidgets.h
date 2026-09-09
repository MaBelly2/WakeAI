#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QString>
#include <QTimer>
#include <QWidget>

class QPropertyAnimation;
class QPainter;

class CameraView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgress)
    Q_PROPERTY(qreal countPulse READ countPulse WRITE setCountPulse)
    Q_PROPERTY(qreal noticeOpacity READ noticeOpacity WRITE setNoticeOpacity)

public:
    enum class NoticeKind { Info, Warning, Success };
    explicit CameraView(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    void setCount(int count, int target, bool animate = true);
    void showNotice(const QString &text, NoticeKind kind = NoticeKind::Info,
                    int visibleMs = 3600);
    void showSuccess(const QString &text = QStringLiteral("挑战完成！"));
    void clearNotice();
    void clearFrame();

    qreal progress() const { return progress_; }
    void setProgress(qreal value);
    qreal countPulse() const { return countPulse_; }
    void setCountPulse(qreal value);
    qreal noticeOpacity() const { return noticeOpacity_; }
    void setNoticeOpacity(qreal value);

    QSize sizeHint() const override { return QSize(840, 500); }
    int heightForWidth(int width) const override { return qRound(width * 9.0 / 16.0); }
    bool hasHeightForWidth() const override { return true; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF imageTargetRect() const;
    void animateNotice(qreal endValue, int durationMs);
    void drawCelebration(QPainter &painter, const QRectF &target);

    QImage frame_;
    QString noticeText_;
    NoticeKind noticeKind_ = NoticeKind::Info;
    int count_ = 0;
    int target_ = 1;
    qreal progress_ = 0.0;
    qreal countPulse_ = 1.0;
    qreal noticeOpacity_ = 0.0;
    bool fixedSuccess_ = false;
    bool celebrating_ = false;
    QTimer noticeTimer_;
    QTimer celebrationTimer_;
    QElapsedTimer celebrationClock_;
    QPropertyAnimation *noticeAnimation_ = nullptr;
};

class AchievementIconWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal pulse READ pulse WRITE setPulse)
public:
    explicit AchievementIconWidget(QWidget *parent = nullptr);
    void setAchievement(int index, bool unlocked);
    qreal pulse() const { return pulse_; }
    void setPulse(qreal value);
    QSize sizeHint() const override { return QSize(126, 126); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int index_ = 0;
    bool unlocked_ = false;
    qreal pulse_ = 1.0;
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
