#include "startupoverlay.h"

#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPauseAnimation>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSequentialAnimationGroup>

namespace {
// 与图标 / 文字图一致的暖米白纸张色。
constexpr const char *kPaperColor = "#FCEFD6";
constexpr int kWindowW = 640;
constexpr int kWindowH = 420;
constexpr int kIconSize = 220;
constexpr int kIconRadius = 36;
constexpr int kTextWidth = 420;
} // namespace

StartupOverlay::StartupOverlay(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(kWindowW, kWindowH);

    // 阶段一：圆角图标
    m_icon = new QLabel(this);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setFixedSize(kIconSize, kIconSize);
    m_icon->setPixmap(rounded(QPixmap(QStringLiteral(":/assets/app_icon.jpg")), kIconSize, kIconRadius));

    m_iconFx = new QGraphicsOpacityEffect(this);
    m_iconFx->setOpacity(0.0);
    m_icon->setGraphicsEffect(m_iconFx);

    // 阶段二：铅笔风文字（文字图本身带纸张底色，与窗口背景同色，视觉上只看到字）
    m_text = new QLabel(this);
    m_text->setAlignment(Qt::AlignCenter);
    QPixmap textPix(QStringLiteral(":/assets/wakeai_text.png"));
    const int th = textPix.height() > 0 ? textPix.height() * kTextWidth / textPix.width() : kTextWidth / 2;
    m_text->setFixedSize(kTextWidth, th);
    m_text->setPixmap(textPix.scaled(kTextWidth, th, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_text->show();

    m_textFx = new QGraphicsOpacityEffect(this);
    m_textFx->setOpacity(0.0);
    m_text->setGraphicsEffect(m_textFx);

    // 两个图层都在窗口正中
    m_icon->move((kWindowW - kIconSize) / 2, (kWindowH - kIconSize) / 2);
    m_text->move((kWindowW - kTextWidth) / 2, (kWindowH - th) / 2);

    // 屏幕居中
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        move(geo.center() - rect().center());
    }
}

void StartupOverlay::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(kPaperColor));
}

void StartupOverlay::start() {
    // 时间线（总时长约 3.25s）：
    //   0.000s  图标淡入 (150ms)
    //   0.150s  图标完整显示，停留约 850ms
    //   1.000s  图标淡出 (250ms)
    //   1.250s  文字淡入 (250ms)
    //   1.500s  文字完整显示，停留 1500ms
    //   3.000s  文字淡出 (250ms)
    //   3.250s  ready()
    auto *group = new QSequentialAnimationGroup(this);

    auto *iconIn = new QPropertyAnimation(m_iconFx, "opacity", this);
    iconIn->setDuration(150);
    iconIn->setStartValue(0.0);
    iconIn->setEndValue(1.0);
    group->addAnimation(iconIn);

    group->addPause(850); // 图标完整显示至第 1.0s

    auto *iconOut = new QPropertyAnimation(m_iconFx, "opacity", this);
    iconOut->setDuration(250);
    iconOut->setStartValue(1.0);
    iconOut->setEndValue(0.0);
    group->addAnimation(iconOut);

    auto *textIn = new QPropertyAnimation(m_textFx, "opacity", this);
    textIn->setDuration(250);
    textIn->setStartValue(0.0);
    textIn->setEndValue(1.0);
    group->addAnimation(textIn);

    group->addPause(1500); // 文字完整显示 1.5 秒

    auto *textOut = new QPropertyAnimation(m_textFx, "opacity", this);
    textOut->setDuration(250);
    textOut->setStartValue(1.0);
    textOut->setEndValue(0.0);
    group->addAnimation(textOut);

    connect(group, &QSequentialAnimationGroup::finished, this, [this]() {
        emit ready();
        close();
    });
    group->start();
}

QPixmap StartupOverlay::rounded(const QPixmap &src, int size, int radius) {
    QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap out(size, size);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, radius, radius);
    p.setClipPath(path);
    p.drawPixmap(0, 0, scaled);
    return out;
}
