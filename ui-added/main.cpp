#include "mainwindow.h"
#include "startupoverlay.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
int main(int argc,char* argv[]) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("WakeAITeam");
    QCoreApplication::setApplicationName("WakeAI");
    QCommandLineParser parser;
    parser.setApplicationDescription("WakeAI alarm UI");parser.addHelpOption();
    parser.addOption({"model","Explicit ONNX model path","path"});
    parser.addOption({"camera","Camera index","index","0"});
    parser.addOption({"video","Fixed video (requires --test-mode)","path"});
    parser.addOption({"test-mode","Use separate test DB and enable test tools"});
    parser.process(app);
    StartupOptions options;
    options.modelPath=parser.value("model");options.videoPath=parser.value("video");
    options.testMode=parser.isSet("test-mode");
    bool ok=false;options.cameraIndex=parser.value("camera").toInt(&ok);
    if(!ok||options.cameraIndex<0||(!options.videoPath.isEmpty()&&!options.testMode)) {
        QMessageBox::critical(nullptr,"参数错误","摄像头编号必须为非负整数；视频输入必须加 --test-mode。");return 2;
    }
    // 开屏动画：先显示圆角图标，再显示铅笔风 "WakeAI" 文字，结束后再启动主窗口。
    StartupOverlay splash;
    splash.show();
    splash.start();
    QObject::connect(&splash, &StartupOverlay::ready, &app, [&]() {
        auto* window = new MainWindow(options);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
    });
    return app.exec();
}
