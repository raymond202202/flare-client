#ifndef VERSIONPANEL_H
#define VERSIONPANEL_H

#include <QDialog>

class QLabel;
class QPushButton;
class UpdateChecker;

// ============================================================
// 版本与更新面板
//
// 三块信息互不混淆：
//   1. GUI 客户端版本 —— 编译期注入（CMake FLARE_CLIENT_VERSION），归属 flare-client 仓库
//   2. 内核名字 + 版本 —— 运行时经 version 协议帧取得（engine / protocol 字段），归属 flare 仓库
//   3. 两条「检查更新」互相独立：各自一个 UpdateChecker、各自一个状态标签，
//      任一失败只影响自己那行文案，不影响另一行，也不影响聊天
//
// 对象名（供测试定位）：guiVersionLabel / kernelVersionLabel /
//                      guiStatusLabel / kernelStatusLabel /
//                      guiCheckButton / kernelCheckButton
// ============================================================
class VersionPanel : public QDialog
{
    Q_OBJECT

public:
    VersionPanel(const QString &guiVersion,
                 const QString &kernelVersion,
                 const QString &protocolVersion,
                 QWidget *parent = nullptr);

    QString guiVersionText() const;
    QString kernelVersionText() const;

public slots:
    void checkGuiUpdates();
    void checkKernelUpdates();

private:
    void setStatus(QLabel *label, const QString &text, const QString &color);

    QString m_guiVersion;
    QString m_kernelVersion;
    QString m_protocolVersion;

    QLabel *m_guiVersionLabel = nullptr;
    QLabel *m_kernelVersionLabel = nullptr;
    QLabel *m_guiStatusLabel = nullptr;
    QLabel *m_kernelStatusLabel = nullptr;
    QPushButton *m_guiCheckButton = nullptr;
    QPushButton *m_kernelCheckButton = nullptr;

    UpdateChecker *m_guiChecker = nullptr;
    UpdateChecker *m_kernelChecker = nullptr;
};

#endif // VERSIONPANEL_H
