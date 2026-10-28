#include <QApplication>
#include <QElapsedTimer>
#include <QPixmap>
#include <QThread>
#include <cstdio>
#include <QLabel>
#include "mainwindow.h"
#include "versionpanel.h"
#include "updatechecker.h"

// 手动运行的视觉/集成验证（不注册进 ctest）：
//   QT_QPA_PLATFORM=offscreen ./panel_capture [输出目录]
// 产出：
//   <dir>/main-status.png      主窗口状态栏（含版本文本）
//   <dir>/version-panel.png    版本与更新面板（GUI + 内核两行）
// 同时打印状态栏版本文本，便于核对真实内核版本号。
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("flare-client"));
    app.setApplicationVersion(QStringLiteral(FLARE_CLIENT_VERSION));

    const QString dir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("/tmp");

    MainWindow w;
    w.show();

    // 等内核 version 帧（真实 flare server 子进程握手）
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 4000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    fprintf(stderr, "STATUS_TEXT:%s\n", qUtf8Printable(w.versionText()));
    w.grab().save(dir + QStringLiteral("/main-status.png"));

    QMetaObject::invokeMethod(&w, "openVersionPanel", Qt::DirectConnection);
    QCoreApplication::processEvents();

    VersionPanel *panel = w.findChild<VersionPanel *>();
    if (!panel) {
        fprintf(stderr, "PANEL:not found\n");
        return 1;
    }
    fprintf(stderr, "GUI_LABEL:%s\n", qUtf8Printable(panel->guiVersionText()));
    fprintf(stderr, "KERNEL_LABEL:%s\n", qUtf8Printable(panel->kernelVersionText()));
    panel->show();
    QCoreApplication::processEvents();
    panel->grab().save(dir + QStringLiteral("/version-panel.png"));

    // 触发内核更新检查（真实 GitHub API），等结果落到标签上
    QMetaObject::invokeMethod(panel, "checkKernelUpdates", Qt::DirectConnection);
    t.restart();
    while (t.elapsed() < 6000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    auto *ks = panel->findChild<QLabel *>(QStringLiteral("kernelStatusLabel"));
    auto *gs = panel->findChild<QLabel *>(QStringLiteral("guiStatusLabel"));
    fprintf(stderr, "KERNEL_STATUS:%s\n", qUtf8Printable(ks ? ks->text() : QStringLiteral("<null>")));
    fprintf(stderr, "GUI_STATUS:%s\n", qUtf8Printable(gs ? gs->text() : QStringLiteral("<null>")));
    panel->grab().save(dir + QStringLiteral("/version-panel-checked.png"));

    // 再触发客户端更新检查：验证两条通道互不干扰（内核那行不应被改动）
    const QString kernelBefore = ks ? ks->text() : QString();
    QMetaObject::invokeMethod(panel, "checkGuiUpdates", Qt::DirectConnection);
    t.restart();
    while (t.elapsed() < 8000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    fprintf(stderr, "GUI_STATUS_AFTER:%s\n", qUtf8Printable(gs ? gs->text() : QStringLiteral("<null>")));
    fprintf(stderr, "KERNEL_UNCHANGED:%s\n",
            qUtf8Printable((ks && ks->text() == kernelBefore) ? QStringLiteral("yes") : QStringLiteral("no")));
    panel->grab().save(dir + QStringLiteral("/version-panel-both.png"));
    return 0;
}
