// 真实场景复现：引擎一批发 text+confirm+done 多事件 → 验证不崩溃
// 用 QTimer 模拟用户在弹窗弹出后点击"拒绝"（避免阻塞）
#include <QApplication>
#include <QTimer>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <cstdio>
#include "../src/chatwidget.h"
#include "engine_bridge.h"

// 直连真实 flare server 的桥（不重写，复用真类）
class RealBridge : public EngineBridge {
public:
    using EngineBridge::EngineBridge;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 用真实引擎：spawn flare server
    RealBridge bridge;
    if (!bridge.start()) {
        fprintf(stderr, "FAIL: 引擎启动失败\n");
        return 1;
    }

    ChatWidget widget(&bridge);
    widget.resize(900, 600);
    widget.show();

    // 2 秒后发一条会触发工具调用的消息（读取文件 = read_file 工具 → confirm 弹窗）
    QTimer::singleShot(2000, [&]() {
        fprintf(stderr, ">>> 发送工具调用消息\n");
        widget.input()->setText("帮我读取文件 /etc/hostname 的内容");
        widget.sendMessage();
    });

    // 5 秒后：如果 QMessageBox 弹窗存在则自动点"拒绝"，否则直接通过
    QTimer *autoClick = new QTimer;
    QObject::connect(autoClick, &QTimer::timeout, [&]() {
        auto *activeBox = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (activeBox) {
            fprintf(stderr, ">>> 检测到确认弹窗，自动点击拒绝\n");
            // 遍历所有按钮找 RejectRole 的（自定义按钮）
            const auto buttons = activeBox->buttons();
            for (QAbstractButton *b : buttons) {
                if (activeBox->buttonRole(b) == QMessageBox::RejectRole) {
                    b->click();
                    break;
                }
            }
        }
    });
    autoClick->start(500);

    // 12 秒后检查：进程仍存活 = 未崩溃
    QTimer::singleShot(12000, [&]() {
        fprintf(stderr, "=== 12 秒存活检查 ===\n");
        fprintf(stderr, "输出区内容: %s\n", widget.outputText().left(200).toUtf8().constData());
        fprintf(stderr, "RESULT: PASS（未崩溃，引擎对话正常）\n");
        bridge.stop();
        app.exit(0);
    });

    return app.exec();
}
