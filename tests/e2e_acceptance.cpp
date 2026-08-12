// M2 端到端验收：GUI 自动发送消息 → 验证收到 AI 回复（用户今晚验收的核心场景）
#include <QApplication>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QDir>
#include <cstdio>
#include "mainwindow.h"
#include "chatwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow w;
    w.show();

    // 等引擎连接 + 渲染就绪
    QTimer::singleShot(3000, [&]() {
        // 通过 UI 控件真实操作：输入文字 → 点发送
        QLineEdit *input = w.findChild<QLineEdit *>();
        QPushButton *sendBtn = w.findChild<QPushButton *>();
        if (!input || !sendBtn) {
            qWarning("UI 控件未找到");
            app.exit(1);
            return;
        }
        input->setText(QStringLiteral("用一句话介绍你自己"));
        sendBtn->click();

        // 等待 AI 回复（最多 45s）
        QTimer *watchdog = new QTimer(&app);
        watchdog->setInterval(1000);
        QObject::connect(watchdog, &QTimer::timeout, [&]() {
            QTextEdit *output = w.findChild<QTextEdit *>();
            const QString text = output->toPlainText();
            // 输出区应包含"你"发的消息 + Flare 的回复（非空）
            if (text.contains("用一句话介绍你自己") && text.contains("Flare") && text.length() > 30) {
                fprintf(stderr, "=== 端到端验收 PASS ===\n");
                fprintf(stderr, "对话内容:\n%s\n", text.toUtf8().constData());
                // 保存最终截图
                w.grab().save(QDir::homePath() + "/Desktop/flare-gui-chat-result.png");
                app.exit(0);
            }
        });
        watchdog->start();

        // 总超时
        QTimer::singleShot(45000, [&]() {
            fprintf(stderr, "=== 端到端验收 FAIL: 45s 内未收到回复 ===\n");
            app.exit(1);
        });
    });

    return app.exec();
}
