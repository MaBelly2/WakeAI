#pragma once

#include <QWidget>

class QLabel;
class QGraphicsOpacityEffect;

// 启动开屏动画：
//   1) 显示圆角应用图标（淡入，完整停留约 1 秒后淡出）
//   2) 显示铅笔风 "WakeAI" 文字（淡入，完整停留 1.5 秒后淡出）
//   3) 全部结束后发出 ready()，由 main() 构造并展示主窗口。
class StartupOverlay : public QWidget {
    Q_OBJECT
public:
    explicit StartupOverlay(QWidget *parent = nullptr);

    // 开始播放开屏动画（调用前请先 show()）。
    void start();

signals:
    // 动画全部播完，主窗口可以构造并显示；随后本窗口自动关闭。
    void ready();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // 将原图按 targetSize 缩放并裁成 radius 圆角，返回带 alpha 的图。
    static QPixmap rounded(const QPixmap &src, int targetSize, int radius);

    QLabel *m_icon = nullptr;
    QLabel *m_text = nullptr;
    QGraphicsOpacityEffect *m_iconFx = nullptr;
    QGraphicsOpacityEffect *m_textFx = nullptr;
};
