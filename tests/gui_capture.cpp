// GUI 渲染验证：offscreen 渲染主窗口并保存 PNG（验收截图）
#include <QApplication>
#include <QTimer>
#include <QDir>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow w;
    w.show();

    QTimer::singleShot(3500, [&]() {
        const QPixmap pm = w.grab();
        const QString path = QDir::homePath() + "/Desktop/flare-gui-preview.png";
        if (!pm.save(path))
            qFatal("保存截图失败");
        qInfo("截图已保存: %s (%dx%d)", qPrintable(path), pm.width(), pm.height());
        qInfo("窗口标题: %s", qPrintable(w.windowTitle()));
        qInfo("引擎连接: %s", w.engine()->isRunning() ? "是" : "否");
        app.exit(0);
    });

    return app.exec();
}
