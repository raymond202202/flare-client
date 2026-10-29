#include "versionpanel.h"

#include "updatechecker.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
// 两个仓库常量：GUI 归 flare-client，内核归 flare（Go 引擎主线，原名 flare-go）；改这里即可换比对源
const char *kGuiRepo = "raymond202202/flare-client";
const char *kKernelRepo = "raymond202202/flare"; // 2026-10 起该仓库即 Go 引擎主线（Node 版已改名 flare-node）
const char *kKernelName = "flare"; // 内核产品名固定（对齐 `flare version` → "flare v0.6.161"）

// 结论后面附上 Release 页地址（纯文本，可选中复制）
QString withUrl(const QString &msg, const QString &url)
{
    if (url.startsWith(QLatin1String("http")))
        return msg + QStringLiteral("　") + url;
    return msg;
}

QLabel *makeValueLabel(const QString &objectName, QWidget *parent)
{
    auto *lbl = new QLabel(parent);
    lbl->setObjectName(objectName);
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lbl->setStyleSheet(QStringLiteral("color:#4a2e0d; font-size:13px;"));
    return lbl;
}
} // namespace

VersionPanel::VersionPanel(const QString &guiVersion,
                           const QString &kernelVersion,
                           const QString &protocolVersion,
                           QWidget *parent)
    : QDialog(parent)
    , m_guiVersion(guiVersion)
    , m_kernelVersion(kernelVersion)
    , m_protocolVersion(protocolVersion)
{
    setWindowTitle(QStringLiteral("版本与更新"));
    setModal(false);
    resize(500, 340);
    setStyleSheet(QStringLiteral(
        "QDialog { background:#ffffff; }"
        "QGroupBox { border:1px solid #fde3c6; border-radius:8px;"
        "            margin-top:14px; padding:14px 14px 10px 14px; background:#fffbf0; }"
        "QGroupBox::title { subcontrol-origin: margin; left:12px; padding:0 4px;"
        "                    color:#f97316; font-weight:bold; }"
        "QPushButton { background:#f97316; color:#ffffff; border:none; border-radius:6px;"
        "              padding:6px 16px; font-size:13px; }"
        "QPushButton:hover { background:#ea580c; }"
        "QPushButton:pressed { background:#c2410c; }"
        "QPushButton:disabled { background:#e5e0d8; color:#9a8f80; }"
        "QPushButton#guiCheckButton, QPushButton#kernelCheckButton { min-width:88px; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 8, 16, 16);
    root->setSpacing(6);

    // ---------- 第一块：GUI 客户端（自身版本） ----------
    auto *guiBox = new QGroupBox(QStringLiteral("GUI 客户端"), this);
    auto *guiLay = new QVBoxLayout(guiBox);
    guiLay->setSpacing(6);

    m_guiVersionLabel = makeValueLabel(QStringLiteral("guiVersionLabel"), guiBox);
    m_guiVersionLabel->setText(QStringLiteral("Flare Client v%1").arg(m_guiVersion));
    m_guiVersionLabel->setStyleSheet(QStringLiteral("color:#4a2e0d; font-size:15px; font-weight:bold;"));
    guiLay->addWidget(m_guiVersionLabel);

    auto *guiRepo = new QLabel(QStringLiteral("更新来源：GitHub · %1").arg(QLatin1String(kGuiRepo)), guiBox);
    guiRepo->setStyleSheet(QStringLiteral("color:#9a8f80; font-size:12px;"));
    guiLay->addWidget(guiRepo);

    auto *guiRow = new QHBoxLayout();
    m_guiCheckButton = new QPushButton(QStringLiteral("检查客户端更新"), guiBox);
    m_guiCheckButton->setObjectName(QStringLiteral("guiCheckButton"));
    m_guiCheckButton->setCursor(Qt::PointingHandCursor);
    m_guiStatusLabel = new QLabel(QStringLiteral("未检查"), guiBox);
    m_guiStatusLabel->setObjectName(QStringLiteral("guiStatusLabel"));
    m_guiStatusLabel->setStyleSheet(QStringLiteral("color:#9a8f80; font-size:12px;"));
    m_guiStatusLabel->setWordWrap(true);
    m_guiStatusLabel->setOpenExternalLinks(true);
    guiRow->addWidget(m_guiCheckButton);
    guiRow->addWidget(m_guiStatusLabel, 1);
    guiLay->addLayout(guiRow);
    root->addWidget(guiBox);

    // ---------- 第二块：flare 内核（名字 + 运行时版本） ----------
    auto *kBox = new QGroupBox(QStringLiteral("Flare 内核"), this);
    auto *kLay = new QVBoxLayout(kBox);
    kLay->setSpacing(6);

    m_kernelVersionLabel = makeValueLabel(QStringLiteral("kernelVersionLabel"), kBox);
    m_kernelVersionLabel->setStyleSheet(QStringLiteral("color:#4a2e0d; font-size:15px; font-weight:bold;"));
    if (m_kernelVersion.isEmpty()) {
        m_kernelVersionLabel->setText(QStringLiteral("%1 · 未连接（无法获取版本）")
                                          .arg(QLatin1String(kKernelName)));
    } else if (m_protocolVersion.isEmpty()) {
        m_kernelVersionLabel->setText(QStringLiteral("%1 v%2")
                                          .arg(QLatin1String(kKernelName), m_kernelVersion));
    } else {
        m_kernelVersionLabel->setText(QStringLiteral("%1 v%2（协议 %3）")
                                          .arg(QLatin1String(kKernelName), m_kernelVersion,
                                               m_protocolVersion));
    }
    kLay->addWidget(m_kernelVersionLabel);

    auto *kRepo = new QLabel(QStringLiteral("版本来自 flare server 的 version 帧 · 更新来源：GitHub · %1")
                                 .arg(QLatin1String(kKernelRepo)), kBox);
    kRepo->setStyleSheet(QStringLiteral("color:#9a8f80; font-size:12px;"));
    kLay->addWidget(kRepo);

    auto *kRow = new QHBoxLayout();
    m_kernelCheckButton = new QPushButton(QStringLiteral("检查内核更新"), kBox);
    m_kernelCheckButton->setObjectName(QStringLiteral("kernelCheckButton"));
    m_kernelCheckButton->setCursor(Qt::PointingHandCursor);
    m_kernelStatusLabel = new QLabel(m_kernelVersion.isEmpty()
                                         ? QStringLiteral("内核未连接，暂不能检查")
                                         : QStringLiteral("未检查"),
                                     kBox);
    m_kernelStatusLabel->setObjectName(QStringLiteral("kernelStatusLabel"));
    m_kernelStatusLabel->setStyleSheet(QStringLiteral("color:#9a8f80; font-size:12px;"));
    m_kernelStatusLabel->setWordWrap(true);
    m_kernelStatusLabel->setOpenExternalLinks(true);
    m_kernelCheckButton->setEnabled(!m_kernelVersion.isEmpty());
    kRow->addWidget(m_kernelCheckButton);
    kRow->addWidget(m_kernelStatusLabel, 1);
    kLay->addLayout(kRow);
    root->addWidget(kBox);

    auto *hint = new QLabel(QStringLiteral("两条更新检查互相独立：客户端升级不必重装内核，内核升级也不必重装客户端。"), this);
    hint->setStyleSheet(QStringLiteral("color:#b9ab99; font-size:11px;"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    // ---------- 两个独立检查器（id 分开，互不影响） ----------
    m_guiChecker = new UpdateChecker(this);
    m_kernelChecker = new UpdateChecker(this);

    connect(m_guiCheckButton, &QPushButton::clicked, this, &VersionPanel::checkGuiUpdates);
    connect(m_kernelCheckButton, &QPushButton::clicked, this, &VersionPanel::checkKernelUpdates);

    connect(m_guiChecker, &UpdateChecker::finished, this,
            [this](bool ok, const QString &latest, const QString &url, const QString &msg) {
                m_guiCheckButton->setEnabled(true);
                setStatus(m_guiStatusLabel, withUrl(msg, url),
                          ok ? QStringLiteral("#16a34a") : QStringLiteral("#ef4444"));
                Q_UNUSED(latest)
            });
    connect(m_kernelChecker, &UpdateChecker::finished, this,
            [this](bool ok, const QString &latest, const QString &url, const QString &msg) {
                m_kernelCheckButton->setEnabled(true);
                setStatus(m_kernelStatusLabel, withUrl(msg, url),
                          ok ? QStringLiteral("#16a34a") : QStringLiteral("#ef4444"));
                Q_UNUSED(latest)
            });
}

QString VersionPanel::guiVersionText() const
{
    return m_guiVersionLabel ? m_guiVersionLabel->text() : QString();
}

QString VersionPanel::kernelVersionText() const
{
    return m_kernelVersionLabel ? m_kernelVersionLabel->text() : QString();
}

void VersionPanel::setStatus(QLabel *label, const QString &text, const QString &color)
{
    if (!label)
        return;
    label->setTextFormat(Qt::PlainText);
    label->setText(text);
    label->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(color));
}

void VersionPanel::checkGuiUpdates()
{
    if (m_guiVersion.isEmpty()) {
        setStatus(m_guiStatusLabel, QStringLiteral("客户端版本未知，无法检查"), QStringLiteral("#ef4444"));
        return;
    }
    m_guiCheckButton->setEnabled(false);
    setStatus(m_guiStatusLabel, QStringLiteral("检查中…"), QStringLiteral("#9a8f80"));
    m_guiChecker->checkLatest(QLatin1String(kGuiRepo), m_guiVersion);
}

void VersionPanel::checkKernelUpdates()
{
    if (m_kernelVersion.isEmpty()) {
        setStatus(m_kernelStatusLabel, QStringLiteral("内核未连接，暂不能检查"), QStringLiteral("#ef4444"));
        return;
    }
    m_kernelCheckButton->setEnabled(false);
    setStatus(m_kernelStatusLabel, QStringLiteral("检查中…"), QStringLiteral("#9a8f80"));
    m_kernelChecker->checkLatest(QLatin1String(kKernelRepo), m_kernelVersion);
}
