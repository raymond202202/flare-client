#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

// ============================================================
// 设置面板（M6-2）
// 用户手动填写 API key（不同用户买不同模型 key）
// 读写 ~/.flare/.env —— 与 flare CLI 共享同一配置（M6-3）
// 保存后需重启客户端生效（引擎启动时读 .env）
// ============================================================
class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    // 测试接口
    QLineEdit *deepseekKeyEdit() const { return m_deepseekKey; }
    QLineEdit *modelEdit() const { return m_model; }
    QPushButton *saveButton() const { return m_saveBtn; }
    QLabel *statusLabel() const { return m_status; }

    QString envFilePath() const;

public slots:
    void load();
    void save();

private:
    QLineEdit *m_deepseekKey;
    QLineEdit *m_model;
    QPushButton *m_saveBtn;
    QLabel *m_status;
};

#endif // SETTINGSPANEL_H
