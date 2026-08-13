#include "settingspanel.h"
#include "flametheme.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>

// ============================================================
// 设置面板（M6-2）：手动填写 API key
// 读写 ~/.flare/.env（与 flare CLI 共享，M6-3）
// 安全：key 输入框用 Password 掩码显示；保存后 chmod 600
// ============================================================

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
    , m_deepseekKey(new QLineEdit(this))
    , m_model(new QLineEdit(this))
    , m_saveBtn(new QPushButton(QStringLiteral("保存"), this))
    , m_status(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("设置"), this);
    title->setStyleSheet(QStringLiteral("font-size:18px; font-weight:600; color:#f97316;"));
    layout->addWidget(title);

    auto *hint = new QLabel(
        QStringLiteral("填写你购买的大模型 API key。配置保存在 ~/.flare/.env，"
                       "命令行版 flare 与图形版客户端共用（一次配置，两端通用）。"
                       "保存后重启客户端生效。"),
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#8a6a3b; font-size:13px;"));
    layout->addWidget(hint);

    auto *form = new QFormLayout;
    form->setSpacing(10);

    m_deepseekKey->setEchoMode(QLineEdit::Password); // 掩码显示，防窥屏
    m_deepseekKey->setPlaceholderText(QStringLiteral("sk-...（DeepSeek API key）"));
    m_deepseekKey->setStyleSheet(QStringLiteral(
        "QLineEdit { border:1px solid #fde6bf; border-radius:6px; padding:8px;"
        " font-size:14px; background:#fffbf0; }"
        "QLineEdit:focus { border-color:#f97316; }"));
    form->addRow(QStringLiteral("DeepSeek API Key:"), m_deepseekKey);

    m_model->setPlaceholderText(QStringLiteral("deepseek-chat"));
    m_model->setStyleSheet(QStringLiteral(
        "QLineEdit { border:1px solid #fde6bf; border-radius:6px; padding:8px;"
        " font-size:14px; background:#fffbf0; }"
        "QLineEdit:focus { border-color:#f97316; }"));
    form->addRow(QStringLiteral("默认模型:"), m_model);

    layout->addLayout(form);

    m_saveBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#f97316; color:white; border:none; border-radius:8px;"
        " padding:10px 24px; font-size:14px; font-weight:600; }"
        "QPushButton:hover { background:#e0630f; }"));
    layout->addWidget(m_saveBtn, 0, Qt::AlignRight);

    m_status->setStyleSheet(QStringLiteral("color:#4a2e0d; font-size:12px;"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsPanel::save);
    load();
}

QString SettingsPanel::envFilePath() const
{
    return QDir::homePath() + QStringLiteral("/.flare/.env");
}

void SettingsPanel::load()
{
    QFile f(envFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        m_status->setText(QStringLiteral("未找到 ~/.flare/.env（首次使用，直接填写保存即可）"));
        m_model->setText(QStringLiteral("deepseek-chat"));
        return;
    }
    const QString content = QString::fromUtf8(f.readAll());
    f.close();

    QRegularExpression keyRe(QStringLiteral("^DEEPSEEK_API_KEY\\s*=\\s*['\"]?([^'\"\\r\\n]+)"),
                             QRegularExpression::MultilineOption);
    const auto keyMatch = keyRe.match(content);
    m_deepseekKey->setText(keyMatch.hasMatch() ? keyMatch.captured(1) : QString());

    QRegularExpression modelRe(QStringLiteral("^DEFAULT_MODEL\\s*=\\s*['\"]?([^'\"\\r\\n]+)"),
                               QRegularExpression::MultilineOption);
    const auto modelMatch = modelRe.match(content);
    m_model->setText(modelMatch.hasMatch() ? modelMatch.captured(1) : QStringLiteral("deepseek-chat"));

    m_status->setText(QStringLiteral("已从 %1 加载配置").arg(envFilePath()));
}

void SettingsPanel::save()
{
    const QString key = m_deepseekKey->text().trimmed();
    const QString model = m_model->text().trimmed();
    if (key.isEmpty()) {
        m_status->setText(QStringLiteral("⚠️ API key 不能为空"));
        return;
    }

    const QString path = envFilePath();
    QDir dir;
    dir.mkpath(QFileInfo(path).absolutePath());

    // 保留文件中其他变量（VISION_MODEL 等），只更新本面板管理的两个
    QStringList lines;
    QFile f(path);
    bool exists = false;
    if (f.open(QIODevice::ReadOnly)) {
        const QString content = QString::fromUtf8(f.readAll());
        f.close();
        lines = content.split(QLatin1Char('\n'));
        exists = true;
    }

    bool foundKey = false, foundModel = false;
    for (QString &line : lines) {
        if (line.startsWith(QLatin1String("DEEPSEEK_API_KEY"))) {
            line = QStringLiteral("DEEPSEEK_API_KEY=%1").arg(key);
            foundKey = true;
        } else if (line.startsWith(QLatin1String("DEFAULT_MODEL"))) {
            line = QStringLiteral("DEFAULT_MODEL=%1").arg(model);
            foundModel = true;
        }
    }
    if (!foundKey)
        lines.append(QStringLiteral("DEEPSEEK_API_KEY=%1").arg(key));
    if (!foundModel)
        lines.append(QStringLiteral("DEFAULT_MODEL=%1").arg(model));
    if (exists && !lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast(); // 去掉可能残留的尾部空行

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_status->setText(QStringLiteral("❌ 无法写入 %1").arg(path));
        return;
    }
    QTextStream out(&f);
    out << lines.join(QLatin1Char('\n')) << QLatin1Char('\n');
    f.close();
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner); // 600

    m_status->setText(QStringLiteral("✅ 已保存到 %1（文件权限 600）。重启客户端生效。").arg(path));
}
