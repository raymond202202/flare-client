#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <QFile>
#include "mainwindow.h"

// 客户端版本由 CMake 注入（project(... VERSION x.y.z) → FLARE_CLIENT_VERSION），
// 保证「关于/版本面板」显示的版本与仓库 tag 同源，避免手改两处不一致
#ifndef FLARE_CLIENT_VERSION
#define FLARE_CLIENT_VERSION "0.0.0"
#endif

// --screenshot <path>：启动 → 渲染动画 2.5s → 抓取窗口 → 保存 PNG → 退出
// （用于无人值守验证：物理屏黑屏/盒盖时仍能拿到真实渲染画面）
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("flare-client"));
    app.setApplicationVersion(QStringLiteral(FLARE_CLIENT_VERSION));
    app.setOrganizationName(QStringLiteral("Flare"));

    QString shotPath;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QLatin1String("--screenshot") && i + 1 < argc)
            shotPath = QString::fromLocal8Bit(argv[++i]);
    }

    MainWindow w;
    w.resize(1024, 700);
    w.show();

    if (!shotPath.isEmpty()) {
        // 等 2.5s 让欢迎词呼吸动画进入可见帧，再抓取
        QTimer::singleShot(2500, &w, [&]() {
            const QPixmap pm = w.grab();
            pm.save(shotPath);
            QFile::exists(shotPath) ? app.exit(0) : app.exit(2);
        });
        return app.exec();
    }

    return app.exec();
}
