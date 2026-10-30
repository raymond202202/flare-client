#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <QFile>
#include <QTextStream>
#include "sessionlistwidget.h"
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
    // --version / --help 必须在 QApplication 之前处理：
    //   1) 没有 DISPLAY/Wayland 时 QApplication 会直接 abort，用户连版本号都问不出来
    //      —— 而排障时第一个要的就是版本号
    //   2) 打印版本再建 GUI 纯属浪费，无显示器机器（SSH）上也该能查
    for (int i = 1; i < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "--version" || a == "-v") {
            QTextStream out(stdout);
            out << "flare-client " << FLARE_CLIENT_VERSION << "\n";
            out << "Qt " << QT_VERSION_STR << "\n";
            out << "内核 flare 版本请见客户端状态栏（引擎随包内置）\n";
            return 0;
        }
        if (a == "--help" || a == "-h") {
            QTextStream out(stdout);
            out << "用法: flare-client [选项]\n\n"
                << "选项:\n"
                << "  -v, --version          打印版本后退出（不需要图形环境）\n"
                << "  -h, --help             打印本帮助后退出\n"
                << "      --screenshot <路径> 启动、渲染动画后截图保存并退出\n"
                << "      --view archived    截图前切到回收站视图\n";
            return 0;
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("flare-client"));
    app.setApplicationVersion(QStringLiteral(FLARE_CLIENT_VERSION));
    app.setOrganizationName(QStringLiteral("Flare"));

    QString shotPath;
    QString viewArg;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QLatin1String("--screenshot") && i + 1 < argc)
            shotPath = QString::fromLocal8Bit(argv[++i]);
        // --view archived|recent：截图前切到指定视图（回归验证回收站用）
        else if (a == QLatin1String("--view") && i + 1 < argc)
            viewArg = QString::fromLocal8Bit(argv[++i]);
    }

    MainWindow w;
    w.resize(1024, 700);
    w.show();

    if (!shotPath.isEmpty()) {
        if (viewArg.compare(QLatin1String("archived"), Qt::CaseInsensitive) == 0 && w.sessions())
            w.sessions()->showArchived();
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
