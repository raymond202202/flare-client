// 端到端验收：GUI 自动发送消息 → 验证收到 AI 回复（守门测试）
//
// 历史坑（当真 bug 排查过很久）：这里曾用 w.findChild<QLineEdit *>() /
// findChild<QPushButton *>() 盲取「第一个」控件。M8/M9 新增了侧边栏搜索框
// 和「⏹ 停止」按钮之后，第一个 QLineEdit 变成搜索框、第一个 QPushButton
// 不再是发送键，于是文字被塞进搜索框、点的是别的按钮 —— 引擎根本收不到消息，
// 表现为「45s 内未收到回复」的假故障。现在一律用 objectName 精确定位。
#include <QApplication>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QDir>
#include <QFileInfo>
#include <cstdio>
#include "mainwindow.h"
#include "chatwidget.h"

static void dumpOutput(const QString &text, const char *tag)
{
    fprintf(stderr, "--- %s（输出区 %d 字）---\n%s\n", tag,
            int(text.size()), text.toUtf8().constData());
}

// 引擎自寻：这条测试以前必须在 shell 里手动 export FLARE_ENGINE_BIN 才跑得通，
// 忘了就报「45s 内未收到回复」——看着像产品 bug，其实是测试环境没引擎。
// 现在按「打包形态可能落在哪」自己找，找不到就明确跳过（exit 0 + 说清原因）。
static QString locateEngine()
{
    const QString fromEnv = QString::fromLocal8Bit(qgetenv("FLARE_ENGINE_BIN"));
    if (!fromEnv.isEmpty() && QFileInfo(fromEnv).isExecutable())
        return fromEnv;

    const QStringList candidates{
        QCoreApplication::applicationDirPath() + QStringLiteral("/flare"),
        QDir::homePath() + QStringLiteral("/.flare-client/install/flare"),
        QStringLiteral("/usr/lib64/flare-client/flare"),
        QStringLiteral("/usr/local/bin/flare"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo(c).isExecutable())
            return c;
    }
    return QString();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QString engine = locateEngine();
    if (engine.isEmpty()) {
        fprintf(stderr, "=== 跳过端到端验收：本机找不到 flare 引擎 ===\n"
                        "    查过 FLARE_ENGINE_BIN、应用同目录、~/.flare-client/install/、"
                        "/usr/lib64/flare-client/\n");
        return 0;
    }
    qputenv("FLARE_ENGINE_BIN", engine.toLocal8Bit());
    fprintf(stderr, "引擎: %s\n", engine.toUtf8().constData());

    MainWindow w;
    w.show();

    QTimer::singleShot(3000, [&w, &app]() {
        QLineEdit *input = w.findChild<QLineEdit *>(QStringLiteral("chatInput"));
        QPushButton *sendBtn = w.findChild<QPushButton *>(QStringLiteral("chatSendButton"));
        QTextEdit *output = w.findChild<QTextEdit *>(QStringLiteral("chatOutput"));
        if (!input || !sendBtn || !output) {
            fprintf(stderr, "=== 端到端验收 FAIL: 定位不到 chatInput/chatSendButton/chatOutput ===\n");
            app.exit(1);
            return;
        }

        const int baseline = output->toPlainText().size();
        // 允许用环境变量换问题：手工验证代码块/长文/表格等渲染时不必改代码重编译
        const QString question = qEnvironmentVariableIsSet("FLARE_E2E_PROMPT")
                                     ? qEnvironmentVariable("FLARE_E2E_PROMPT")
                                     : QStringLiteral("用一句话介绍你自己");
        input->setText(question);
        sendBtn->click();

        QTimer *watchdog = new QTimer(&app);
        watchdog->setInterval(1000);
        QObject::connect(watchdog, &QTimer::timeout,
                         [output, baseline, question, &w, &app]() {
            const QString full = output->toPlainText();
            const QString added = full.mid(baseline);
            // 判据：自己那句话已上屏；回复区出现新内容（远超问题本身的长度），
            // 且没有失败/未连接提示 —— 不再靠匹配 "Flare" 字样（空历史时必假阴性）
            const bool sentOk = added.contains(question);
            const bool replied = added.size() > question.size() + 30;
            const bool errored = added.contains(QStringLiteral("请求失败"))
                                 || added.contains(QStringLiteral("未连接"))
                                 || added.contains(QStringLiteral("引擎未启动"))
                                 || added.contains(QStringLiteral("引擎异常退出"))
                                 || added.contains(QStringLiteral("引擎故障"));
            if (sentOk && replied && !errored) {
                fprintf(stderr, "=== 端到端验收 PASS ===\n");
                dumpOutput(full, "对话内容");
                // 截图落在 TMPDIR（默认 ~/.hermes/cache/scratch），不污染用户桌面
                const QString shot = QString::fromLocal8Bit(qgetenv("FLARE_E2E_SHOT"));
                const QString path = shot.isEmpty()
                    ? QDir(QDir::tempPath()).filePath(QStringLiteral("flare-e2e-result.png"))
                    : shot;
                w.grab().save(path);
                fprintf(stderr, "截图: %s (%lld bytes)\n", path.toUtf8().constData(),
                        static_cast<long long>(QFileInfo(path).size()));
                app.exit(0);
            }
        });
        watchdog->start();

        QTimer::singleShot(45000, [output, &app]() {
            fprintf(stderr, "=== 端到端验收 FAIL: 45s 内未收到回复 ===\n");
            if (output)
                dumpOutput(output->toPlainText(), "超时时的输出区");
            app.exit(1);
        });
    });

    return app.exec();
}
