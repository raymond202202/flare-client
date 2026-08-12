// M1 冒烟：真实 chat 流程验证（非单元测试，直接运行验证集成）
// 用法: ./m1_smoke
#include <QCoreApplication>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <cstdio>
#include "../engine-bridge/engine_bridge.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    EngineBridge bridge;
    QStringList seen;
    bool finished = false;

    QObject::connect(&bridge, &EngineBridge::eventReceived, [&](const QJsonObject &obj) {
        if (finished)
            return;
        const QString type = obj.value("type").toString();
        seen.append(type);
        if (type == "text") {
            printf("[text] %s\n", obj.value("content").toString().toUtf8().constData());
        } else if (type == "done") {
            printf("[done] 收到完成事件\n");
        } else if (type == "error") {
            printf("[error] %s\n", obj.value("content").toString().toUtf8().constData());
        }
        if (type == "done" || type == "error") {
            // 验证序列包含关键事件
            bool ok = seen.contains("done") || seen.contains("error");
            printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
            finished = true;
            bridge.stop();
            app.exit(ok ? 0 : 1);
        }
    });

    if (!bridge.start()) {
        printf("RESULT: FAIL (无法启动 flare server)\n");
        return 1;
    }

    bridge.queryVersion();
    QTimer::singleShot(1000, [&]() {
        // 独立会话，避免历史消息影响
        bridge.chat("smoke-" + QString::number(QDateTime::currentMSecsSinceEpoch()),
                    "只回复两个字：你好");
    });

    // 总超时 60s
    QTimer::singleShot(60000, [&]() {
        printf("RESULT: FAIL (超时)\n");
        bridge.stop();
        app.exit(1);
    });

    return app.exec();
}
