#include "uiwidgets.h"

#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QPolygonF>
#include <QtMath>

CameraView::CameraView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(640, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent);
    noticeAnimation_ = new QPropertyAnimation(this, "noticeOpacity", this);
    noticeAnimation_->setEasingCurve(QEasingCurve::OutCubic);
    noticeTimer_.setSingleShot(true);
    connect(&noticeTimer_, &QTimer::timeout, this, [this] {
        if (!fixedSuccess_) animateNotice(0.0, 260);
    });
    celebrationTimer_.setInterval(16);
    connect(&celebrationTimer_, &QTimer::timeout, this, [this] {
        if (celebrationClock_.elapsed() >= 2800) {
            celebrating_ = false;
            celebrationTimer_.stop();
        }
        update();
    });
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
        if (!fixedSuccess_ && noticeKind_ == NoticeKind::Warning)
            clearNotice();
    }
    update();
}

void CameraView::showNotice(const QString &text, NoticeKind kind, int visibleMs)
{
    fixedSuccess_ = false;
    celebrating_ = false;
    celebrationTimer_.stop();
    noticeText_ = text;
    noticeKind_ = kind;
    animateNotice(1.0, 180);
    noticeTimer_.start(qMax(800, visibleMs));
    update();
}

void CameraView::showSuccess(const QString &text)
{
    noticeTimer_.stop();
    fixedSuccess_ = true;
    noticeText_ = text;
    noticeKind_ = NoticeKind::Success;
    animateNotice(1.0, 220);
    celebrating_ = true;
    celebrationClock_.restart();
    celebrationTimer_.start();
    update();
}

void CameraView::clearNotice()
{
    noticeTimer_.stop();
    fixedSuccess_ = false;
    celebrating_ = false;
    celebrationTimer_.stop();
    animateNotice(0.0, 180);
}

void CameraView::clearFrame()
{
    frame_ = {};
    clearNotice();
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

void CameraView::setNoticeOpacity(qreal value)
{
    noticeOpacity_ = qBound(0.0, value, 1.0);
    update();
}

void CameraView::animateNotice(qreal endValue, int durationMs)
{
    noticeAnimation_->stop();
    noticeAnimation_->setDuration(durationMs);
    noticeAnimation_->setStartValue(noticeOpacity_);
    noticeAnimation_->setEndValue(endValue);
    noticeAnimation_->start();
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

    if (celebrating_)
        drawCelebration(painter, target);

    if (noticeOpacity_ > 0.01 && !noticeText_.isEmpty()) {
        painter.save();
        painter.setOpacity(noticeOpacity_ * 0.94);
        const bool success = noticeKind_ == NoticeKind::Success;
        const qreal noticeWidth = qMin<qreal>(success ? 520 : 600, target.width() - 54);
        const qreal noticeHeight = success ? 126 : 92;
        const QRectF notice(target.center().x() - noticeWidth / 2.0,
                            target.center().y() - noticeHeight / 2.0,
                            noticeWidth, noticeHeight);
        painter.setPen(QPen(success ? QColor(83, 235, 178, 190)
                                    : noticeKind_ == NoticeKind::Warning
                                          ? QColor(255, 174, 78, 185)
                                          : QColor(255, 255, 255, 135), 1.4));
        painter.setBrush(success ? QColor(3, 42, 34, 132)
                                 : noticeKind_ == NoticeKind::Warning
                                       ? QColor(45, 24, 5, 118)
                                       : QColor(10, 20, 34, 126));
        painter.drawRoundedRect(notice, 23, 23);

        QFont noticeFont = font();
        noticeFont.setWeight(QFont::Bold);
        noticeFont.setPointSize(success ? 29 : 22);
        painter.setFont(noticeFont);
        painter.setPen(success ? QColor(83, 235, 178)
                               : noticeKind_ == NoticeKind::Warning
                                     ? QColor(255, 174, 78)
                                     : Qt::white);
        if (success) {
            painter.drawText(notice.adjusted(18, 12, -18, -42),
                             Qt::AlignCenter | Qt::TextWordWrap, noticeText_);
            QFont subFont = font();
            subFont.setPointSize(13);
            subFont.setWeight(QFont::DemiBold);
            painter.setFont(subFont);
            painter.setPen(QColor(230, 255, 247, 220));
            painter.drawText(notice.adjusted(18, 72, -18, -10), Qt::AlignCenter,
                             QStringLiteral("点击“完成任务”关闭闹钟"));
        } else {
            painter.drawText(notice.adjusted(24, 12, -24, -12),
                             Qt::AlignCenter | Qt::TextWordWrap, noticeText_);
        }
        painter.restore();
    }

    painter.setClipping(false);
    painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(target, 22, 22);
}

void CameraView::drawCelebration(QPainter &painter, const QRectF &target)
{
    const qreal seconds = celebrationClock_.elapsed() / 1000.0;
    const QColor colors[] = {QColor("#FFD54A"), QColor("#FFB23E"), QColor("#FFF1A1"),
                             QColor("#64E1B9"), QColor("#F7C948")};
    painter.save();
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 58; ++i) {
        const qreal delay = (i % 10) * 0.045;
        const qreal t = seconds - delay;
        if (t < 0.0) continue;
        const qreal spread = (((i * 47) % 101) - 50) / 50.0;
        const qreal originX = target.center().x() + spread * target.width() * 0.23;
        const qreal velocityX = (((i * 31) % 61) - 30) * 2.8;
        const qreal velocityY = -245.0 - (i % 8) * 18.0;
        const qreal x = originX + velocityX * t;
        const qreal y = target.center().y() - 6.0 + velocityY * t + 170.0 * t * t;
        if (!target.adjusted(8, 8, -8, -8).contains(QPointF(x, y))) continue;
        painter.save();
        painter.translate(x, y);
        painter.rotate(i * 29.0 + t * (130.0 + (i % 5) * 25.0));
        painter.setBrush(colors[i % 5]);
        if (i % 3 == 0)
            painter.drawEllipse(QRectF(-4, -4, 8, 8));
        else
            painter.drawRoundedRect(QRectF(-3, -7, 6, 14), 2, 2);
        painter.restore();
    }
    painter.restore();
}

AchievementIconWidget::AchievementIconWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(108, 108);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void AchievementIconWidget::setAchievement(int index, bool unlocked)
{
    index_ = qBound(0, index, 4);
    unlocked_ = unlocked;
    auto *animation = new QPropertyAnimation(this, "pulse", this);
    animation->setDuration(320);
    animation->setKeyValueAt(0.0, 0.78);
    animation->setKeyValueAt(0.55, 1.08);
    animation->setKeyValueAt(1.0, 1.0);
    animation->setEasingCurve(QEasingCurve::OutBack);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    update();
}

void AchievementIconWidget::setPulse(qreal value)
{
    pulse_ = value;
    update();
}

void AchievementIconWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor palette[] = {QColor("#FFAE45"), QColor("#F06B57"), QColor("#8A73E8"),
                              QColor("#35B99C"), QColor("#E3AD27")};
    const QPointF center = rect().center();
    const qreal radius = qMin(width(), height()) * 0.39;
    painter.translate(center);
    painter.scale(pulse_, pulse_);
    painter.setPen(Qt::NoPen);
    painter.setBrush(unlocked_ ? palette[index_] : QColor("#E4E9EF"));
    painter.drawEllipse(QPointF(0, 0), radius, radius);
    painter.setPen(QPen(unlocked_ ? Qt::white : QColor("#98A6B7"), 5,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    const qreal r = radius;

    if (index_ == 0) { // sunrise
        painter.drawLine(QPointF(-r * .58, r * .28), QPointF(r * .58, r * .28));
        painter.drawArc(QRectF(-r * .34, -r * .10, r * .68, r * .68), 0, 180 * 16);
        for (int i = -2; i <= 2; ++i) {
            const qreal angle = -90.0 + i * 31.0;
            const qreal a = qDegreesToRadians(angle);
            painter.drawLine(QPointF(qCos(a) * r * .51, qSin(a) * r * .51),
                             QPointF(qCos(a) * r * .68, qSin(a) * r * .68));
        }
    } else if (index_ == 1) { // flame
        QPainterPath flame;
        flame.moveTo(0, r * .62);
        flame.cubicTo(-r * .58, r * .40, -r * .46, -r * .12, -r * .10, -r * .60);
        flame.cubicTo(-r * .08, -r * .22, r * .42, -r * .18, r * .27, -r * .68);
        flame.cubicTo(r * .73, -r * .18, r * .57, r * .43, 0, r * .62);
        painter.drawPath(flame);
        painter.drawArc(QRectF(-r * .18, r * .04, r * .36, r * .45), 205 * 16, 130 * 16);
    } else if (index_ == 2) { // star
        QPolygonF star;
        for (int i = 0; i < 10; ++i) {
            const qreal angle = qDegreesToRadians(-90.0 + i * 36.0);
            const qreal rr = i % 2 == 0 ? r * .66 : r * .29;
            star << QPointF(qCos(angle) * rr, qSin(angle) * rr);
        }
        painter.drawPolygon(star);
    } else if (index_ == 3) { // lightning
        QPolygonF bolt;
        bolt << QPointF(r * .05, -r * .70) << QPointF(-r * .45, r * .08)
             << QPointF(-r * .08, r * .08) << QPointF(-r * .22, r * .70)
             << QPointF(r * .50, -r * .18) << QPointF(r * .12, -r * .18);
        painter.drawPolyline(bolt);
        painter.drawLine(bolt.last(), bolt.first());
    } else { // trophy
        QPainterPath cup;
        cup.moveTo(-r * .40, -r * .52); cup.lineTo(r * .40, -r * .52);
        cup.cubicTo(r * .34, r * .02, r * .20, r * .20, 0, r * .24);
        cup.cubicTo(-r * .20, r * .20, -r * .34, r * .02, -r * .40, -r * .52);
        painter.drawPath(cup);
        painter.drawArc(QRectF(-r * .66, -r * .43, r * .44, r * .55), 90 * 16, 180 * 16);
        painter.drawArc(QRectF(r * .22, -r * .43, r * .44, r * .55), -90 * 16, 180 * 16);
        painter.drawLine(QPointF(0, r * .24), QPointF(0, r * .52));
        painter.drawLine(QPointF(-r * .28, r * .54), QPointF(r * .28, r * .54));
    }

    if (!unlocked_) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#7F8EA1"));
        painter.drawEllipse(QPointF(r * .56, r * .56), r * .25, r * .25);
        painter.setPen(QPen(Qt::white, 2.5));
        painter.drawArc(QRectF(r * .45, r * .34, r * .22, r * .26), 0, 180 * 16);
        painter.drawRoundedRect(QRectF(r * .43, r * .50, r * .27, r * .22), 3, 3);
    }
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
