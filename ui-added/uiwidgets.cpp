#include "uiwidgets.h"

#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QtMath>

CameraView::CameraView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(640, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void CameraView::setFrame(const QImage &frame)
{
    frame_ = frame;
    update();
}

void CameraView::setCount(int count, int target, bool animate)
{
    const int oldCount = count_;
    count_ = qMax(0, count);
    target_ = qMax(1, target);

    auto *progressAnimation = new QPropertyAnimation(this, "progress", this);
    progressAnimation->setDuration(320);
    progressAnimation->setStartValue(progress_);
    progressAnimation->setEndValue(qBound(0.0, double(count_) / double(target_), 1.0));
    progressAnimation->setEasingCurve(QEasingCurve::OutCubic);
    progressAnimation->start(QAbstractAnimation::DeleteWhenStopped);

    if (animate && count_ > oldCount) {
        auto *pulse = new QPropertyAnimation(this, "countPulse", this);
        pulse->setDuration(260);
        pulse->setKeyValueAt(0.0, 1.0);
        pulse->setKeyValueAt(0.35, 1.22);
        pulse->setKeyValueAt(1.0, 1.0);
        pulse->setEasingCurve(QEasingCurve::OutBack);
        pulse->start(QAbstractAnimation::DeleteWhenStopped);
    }
    update();
}

void CameraView::setStatus(const QString &status, bool poseValid)
{
    status_ = status;
    poseValid_ = poseValid;
    update();
}

void CameraView::clearFrame()
{
    frame_ = {};
    update();
}

void CameraView::setProgress(qreal value)
{
    progress_ = qBound(0.0, value, 1.0);
    update();
}

void CameraView::setCountPulse(qreal value)
{
    countPulse_ = value;
    update();
}

QRectF CameraView::imageTargetRect() const
{
    const QRectF available = rect().adjusted(2, 2, -2, -2);
    if (frame_.isNull())
        return available;

    QSizeF size = frame_.size();
    size.scale(available.size(), Qt::KeepAspectRatio);
    return QRectF(QPointF(available.center().x() - size.width() / 2.0,
                          available.center().y() - size.height() / 2.0), size);
}

void CameraView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#111827"));

    const QRectF target = imageTargetRect();
    QPainterPath clip;
    clip.addRoundedRect(target, 22, 22);
    painter.setClipPath(clip);

    if (!frame_.isNull()) {
        painter.drawImage(target, frame_);
    } else {
        painter.fillRect(target, QColor("#172033"));
        painter.setPen(QColor("#94A3B8"));
        QFont emptyFont = font();
        emptyFont.setPointSize(16);
        emptyFont.setWeight(QFont::DemiBold);
        painter.setFont(emptyFont);
        painter.drawText(target, Qt::AlignCenter, QStringLiteral("摄像头画面将在这里显示"));
    }

    QLinearGradient topShade(target.topLeft(), QPointF(target.left(), target.top() + 135));
    topShade.setColorAt(0.0, QColor(5, 13, 25, 185));
    topShade.setColorAt(1.0, QColor(5, 13, 25, 0));
    painter.fillRect(QRectF(target.left(), target.top(), target.width(), 145), topShade);

    QLinearGradient bottomShade(QPointF(target.left(), target.bottom() - 110), target.bottomLeft());
    bottomShade.setColorAt(0.0, QColor(5, 13, 25, 0));
    bottomShade.setColorAt(1.0, QColor(5, 13, 25, 190));
    painter.fillRect(QRectF(target.left(), target.bottom() - 115, target.width(), 115), bottomShade);

    const QString countText = QStringLiteral("%1").arg(count_);
    const QString targetText = QStringLiteral(" / %1").arg(target_);
    QFont countFont = font();
    countFont.setWeight(QFont::Black);
    countFont.setPointSizeF(35.0 * countPulse_);
    QFont targetFont = font();
    targetFont.setWeight(QFont::DemiBold);
    targetFont.setPointSize(17);

    painter.setFont(countFont);
    const qreal countWidth = painter.fontMetrics().horizontalAdvance(countText);
    painter.setFont(targetFont);
    const qreal targetWidth = painter.fontMetrics().horizontalAdvance(targetText);
    const qreal pillWidth = qMax<qreal>(150, countWidth + targetWidth + 42);
    const QRectF pill(target.center().x() - pillWidth / 2.0, target.top() + 22, pillWidth, 72);
    painter.setPen(QPen(QColor(255, 255, 255, 50), 1));
    painter.setBrush(QColor(10, 20, 34, 198));
    painter.drawRoundedRect(pill, 24, 24);

    qreal x = pill.center().x() - (countWidth + targetWidth) / 2.0;
    painter.setFont(countFont);
    painter.setPen(Qt::white);
    painter.drawText(QPointF(x, pill.center().y() + painter.fontMetrics().ascent() / 2.8), countText);
    x += countWidth;
    painter.setFont(targetFont);
    painter.setPen(QColor("#C8D7E8"));
    painter.drawText(QPointF(x, pill.center().y() + painter.fontMetrics().ascent() / 2.8), targetText);

    const qreal barMargin = 34;
    const QRectF bar(target.left() + barMargin, target.bottom() - 23,
                     target.width() - 2 * barMargin, 6);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 60));
    painter.drawRoundedRect(bar, 3, 3);
    QRectF completed = bar;
    completed.setWidth(bar.width() * progress_);
    painter.setBrush(QColor("#42D3B5"));
    painter.drawRoundedRect(completed, 3, 3);

    QFont statusFont = font();
    statusFont.setPointSize(11);
    statusFont.setWeight(QFont::DemiBold);
    painter.setFont(statusFont);
    const qreal statusWidth = painter.fontMetrics().horizontalAdvance(status_) + 42;
    const QRectF statusPill(target.left() + 22, target.bottom() - 72,
                            qMin(statusWidth, target.width() - 44), 34);
    painter.setBrush(poseValid_ ? QColor(16, 111, 92, 220) : QColor(30, 41, 59, 220));
    painter.drawRoundedRect(statusPill, 17, 17);
    painter.setPen(Qt::white);
    painter.drawEllipse(QPointF(statusPill.left() + 17, statusPill.center().y()), 4, 4);
    painter.drawText(statusPill.adjusted(30, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, status_);

    painter.setClipping(false);
    painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(target, 22, 22);
}

RingPulseWidget::RingPulseWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(220, 220);
    timer_.setInterval(16);
    connect(&timer_, &QTimer::timeout, this, [this] { update(); });
    clock_.start();
    timer_.start();
}

void RingPulseWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF c = rect().center();
    const qreal base = qMin(width(), height()) * 0.29;
    const qreal phase = fmod(clock_.elapsed() / 1500.0, 1.0);

    for (int i = 0; i < 3; ++i) {
        const qreal p = fmod(phase + i / 3.0, 1.0);
        const qreal radius = base + p * base * 0.72;
        painter.setPen(QPen(QColor(65, 193, 167, int(95 * (1.0 - p))), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(c, radius, radius);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#172033"));
    painter.drawEllipse(c, base, base);
    painter.setBrush(QColor("#42D3B5"));
    painter.drawEllipse(c, base * 0.12, base * 0.12);
    painter.setPen(QPen(Qt::white, 7, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(c, c + QPointF(0, -base * 0.55));
    painter.drawLine(c, c + QPointF(base * 0.42, base * 0.18));
}
