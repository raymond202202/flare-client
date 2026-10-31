#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <QFile>
#include <QTextStream>
#include <QListWidget>
#include <QScrollBar>
#include <QSystemTrayIcon>
#include <QGuiApplication>
#include <QFileInfo>
#include <QImage>
#include <QClipboard>
#include <cstdio>
#include "sessionlistwidget.h"
#include "chatwidget.h"
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
                << "      --system-check     打印桌面环境自检（托盘/会话类型/引擎）后退出，不弹窗口\n"
                << "      --screenshot <路径> 启动、渲染动画后截图保存并退出\n"
                << "      --view archived    截图前切到回收站视图\n"
                << "      --search <关键词>  截图前把关键词打进搜索框（走真实防抖与回执校对）\n"
                << "      --scroll-bottom    截图前把会话列表滚到底（验证分页加载）\n"
                << "      --close-test       按一次窗口关闭，打印「是否收进托盘」后退出\n"
                << "      --paste-test       用剪贴板图片走一次粘贴，打印挂上的附件后退出\n"
                << "      --workdir-test <d> 把工作目录切到 d，打印引擎重启前后的 pid 与真实 cwd\n";
            return 0;
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("flare-client"));
    app.setApplicationVersion(QStringLiteral(FLARE_CLIENT_VERSION));
    app.setOrganizationName(QStringLiteral("Flare"));

    QString shotPath;
    QString viewArg;
    QString searchArg;
    bool scrollBottom = false;
    bool systemCheck = false;
    bool closeTest = false;
    bool pasteTest = false;
    QString workdirTest;  // 非空 = 走一次「切换工作目录」自检
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QLatin1String("--screenshot") && i + 1 < argc)
            shotPath = QString::fromLocal8Bit(argv[++i]);
        // --view archived|recent：截图前切到指定视图（回归验证回收站用）
        else if (a == QLatin1String("--search") && i + 1 < argc)
            searchArg = QString::fromLocal8Bit(argv[++i]);
        else if (a == QLatin1String("--view") && i + 1 < argc)
            viewArg = QString::fromLocal8Bit(argv[++i]);
        else if (a == QLatin1String("--scroll-bottom"))
            scrollBottom = true;  // 截图前把会话列表滚到底，回归验证分页
        else if (a == QLatin1String("--system-check"))
            systemCheck = true;
        else if (a == QLatin1String("--close-test"))
            closeTest = true;
        else if (a == QLatin1String("--paste-test"))
            pasteTest = true;
        else if (a == QLatin1String("--workdir-test") && i + 1 < argc)
            workdirTest = QString::fromLocal8Bit(argv[++i]);
    }

    // --system-check：只查环境、不弹窗口。托盘可用性在无显示器/远程排障时
    // 光看代码判断不了（Wayland 会话里时有时无），必须真机问系统
    if (systemCheck) {
        QTextStream out(stdout);
        out << "flare-client " << FLARE_CLIENT_VERSION << "\n";
        out << "qt_platform=" << QGuiApplication::platformName() << "\n";
        out << "session_type=" << qEnvironmentVariable("XDG_SESSION_TYPE") << "\n";
        out << "desktop=" << qEnvironmentVariable("XDG_CURRENT_DESKTOP") << "\n";
        out << "tray_available=" << (QSystemTrayIcon::isSystemTrayAvailable() ? "yes" : "no") << "\n";
        out << "messages_available=" << (QSystemTrayIcon::supportsMessages() ? "yes" : "no") << "\n";
        out << "close_will=" << (QSystemTrayIcon::isSystemTrayAvailable()
                                   ? "hide_to_tray" : "minimize_in_place") << "\n";
        out << "app_dir=" << QCoreApplication::applicationDirPath() << "\n";
        const QString envEngine = qEnvironmentVariable("FLARE_ENGINE_BIN");
        out << "engine_override=" << (envEngine.isEmpty() ? "(none)" : envEngine) << "\n";
        out.flush();
        return 0;
    }

    MainWindow w;
    w.resize(1024, 700);
    w.show();

    // --paste-test：往剪贴板塞一张真图片，走真实的粘贴入口，打印挂上的附件。
    // 不靠人手 Ctrl+V，也不靠 mock：验的是「剪贴板图片真的变成了输入框里的附件」。
    if (pasteTest) {
        QTimer::singleShot(600, &w, [&w, &app]() {
            QImage img(160, 100, QImage::Format_RGB32);
            img.fill(QColor(0x6d, 0x4a, 0xff));  // 主题紫，一眼能认出是本自检造的图
            QApplication::clipboard()->setImage(img);
            const bool handled = w.chat() ? w.chat()->pasteImageFromClipboard() : false;
            const QStringList atts = w.chat() ? w.chat()->attachments() : QStringList();
            fprintf(stderr, "PASTE_HANDLED=%d ATTACH_COUNT=%d\n", handled ? 1 : 0, atts.size());
            for (const QString &a : atts) {
                const QFileInfo fi(a);
                fprintf(stderr, "ATTACH=%s exists=%d bytes=%lld\n", qPrintable(fi.fileName()),
                        fi.exists() ? 1 : 0, static_cast<long long>(fi.size()));
            }
            fflush(stderr);
            app.exit(0);
        });
        return app.exec();
    }

    // --close-test：验证「点关闭 → 收进托盘而不是退出进程」。
    // Wayland 下没有 xdotool 之类的外力能可靠发关闭事件，只能在进程内自己按一次关闭键。
    if (closeTest) {
        QTimer::singleShot(800, &w, [&w]() {
            fprintf(stderr, "BEFORE_CLOSE visible=%d tray=%d\n",
                    w.isVisible() ? 1 : 0, w.trayVisible() ? 1 : 0);
            w.close();  // 与用户点标题栏 × 走同一条 closeEvent
        });
        QTimer::singleShot(2200, &w, [&w, &app]() {
            fprintf(stderr, "AFTER_CLOSE visible=%d tray=%d still_running=1\n",
                    w.isVisible() ? 1 : 0, w.trayVisible() ? 1 : 0);
            fflush(stderr);
            app.exit(0);  // 自检结束，别把托盘图标留在用户桌面上
        });
        return app.exec();
    }

    // --workdir-test：用户要的「选择工作目录 / 读取本地文件」入口，
    // 文件对话框本身没法自动点，但真正会出错的是后半段——「目录存下来了，
    // 引擎却没换根」。所以读 /proc/<pid>/cwd 这个内核事实，而不是看界面文案。
    if (!workdirTest.isEmpty()) {
        auto procCwd = [](qint64 pid) -> QString {
            if (pid <= 0)
                return QStringLiteral("<no-engine>");
            const QString t = QFile::symLinkTarget(QStringLiteral("/proc/%1/cwd").arg(pid));
            return t.isEmpty() ? QStringLiteral("<unreadable>") : t;
        };
        QTimer::singleShot(700, &w, [&w, &workdirTest, procCwd]() {
            const qint64 pid = w.enginePid();
            fprintf(stderr, "BEFORE_WORKDIR pid=%lld engine_cwd=%s\n", static_cast<long long>(pid),
                    qPrintable(procCwd(pid)));
            w.applyWorkingDirectory(workdirTest);  // 与点状态栏 📂 按钮、拖文件夹进来同一段逻辑
        });
        QTimer::singleShot(3000, &w, [&w, &app, &workdirTest, procCwd]() {
            const qint64 pid = w.enginePid();
            fprintf(stderr, "AFTER_WORKDIR pid=%lld engine_cwd=%s want=%s\n",
                    static_cast<long long>(pid), qPrintable(procCwd(pid)),
                    qPrintable(workdirTest));
            fflush(stderr);
            app.exit(0);
        });
        return app.exec();
    }

    if (!shotPath.isEmpty()) {
        if (viewArg.compare(QLatin1String("archived"), Qt::CaseInsensitive) == 0 && w.sessions())
            w.sessions()->showArchived();
        // 搜索取证：真的走一遍输入框 → 防抖 → 引擎回执，而不是直接伪造结果列表
        if (!searchArg.isEmpty() && w.sessions())
            w.sessions()->searchBox()->setText(searchArg);
        QListWidget *lw = w.sessions() ? w.sessions()->findChild<QListWidget *>() : nullptr;

        // 取证口径：等「列表数据真的落地」再抓图，不再猜固定延时。
        // 分页模式（--scroll-bottom）每落地一次就再滚到底，直到条数不再增长。
        bool grabbed = false;
        auto snap = [&](const char *why) {
            if (grabbed)
                return;
            grabbed = true;
            if (lw) {
                fprintf(stderr, "LIST_ITEMS=%d shot_reason=%s\n", lw->count(), why);
                // 条数对了但画面空过 → 逐项把文本打出来，判断是「没数据」还是「没渲染」
                const int n = lw->count() < 25 ? lw->count() : 25;
                for (int i = 0; i < n; ++i)
                    fprintf(stderr, "ITEM[%d]=%s\n", i,
                            qPrintable(lw->item(i)->text().simplified().left(60)));
            }
            fflush(stderr);
            const QPixmap pm = w.grab();
            pm.save(shotPath);
            QFile::exists(shotPath) ? app.exit(0) : app.exit(2);
        };

        // 硬兜底：引擎不回包（崩了/卡住）也必须留下证据并退出，不能挂在这里
        QTimer::singleShot(15000, &w, [snap]() { snap("timeout"); });

        if (!w.sessions()) {
            QTimer::singleShot(2500, &w, [snap]() { snap("no-session-widget"); });
            return app.exec();
        }

        int applies = 0;
        int lastCount = -1;
        int stalls = 0;
        // 翻到最早一条后就不会再有回包了 —— 用「静默计时器」判定到底，
        // 否则只能等 15s 硬兜底（引擎快反而是浪费）。
        auto *settle = new QTimer(&w);
        settle->setSingleShot(true);
        settle->setInterval(1200);
        QObject::connect(settle, &QTimer::timeout, &w, [snap]() { snap("settled"); });
        QObject::connect(w.sessions(), &SessionListWidget::sessionsApplied, &w,
                         [&, lw, snap, settle](int n) {
                             ++applies;
                             if (scrollBottom) {
                                 // 触底会抬高分页上限并重拉；先等布局把滚动条最大值算出来
                                 QTimer::singleShot(60, &w, [lw]() {
                                     if (lw)
                                         lw->verticalScrollBar()->setValue(
                                                 lw->verticalScrollBar()->maximum());
                                 });
                                 // 条数不再增长（连续两次）说明已经翻到最早一条
                                 n == lastCount ? ++stalls : stalls = 0;
                                 lastCount = n;
                                 if (stalls >= 2 || applies >= 30)
                                     QTimer::singleShot(400, &w, [snap]() { snap("paged"); });
                                 else
                                     settle->start();
                             } else {
                                 // 非分页：数据落地后留 350ms 让欢迎词呼吸动画进入可见帧
                                 QTimer::singleShot(350, &w, [snap]() { snap("applied"); });
                             }
                         });
        return app.exec();
    }

    return app.exec();
}
