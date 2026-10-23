#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
QSpinBox *makeSpin(QWidget *parent, int minimum, int maximum, const QString &suffix)
{
    auto *spin = new QSpinBox(parent);
    spin->setLocale(QLocale::c());
    spin->setRange(minimum, maximum);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}
} // namespace

SettingsDialog::SettingsDialog(const AppConfig &config, QWidget *parent)
    : QDialog(parent)
    , m_original(config)
    , m_result(config)
{
    setWindowTitle(QStringLiteral("设置"));
    setModal(true);
    resize(720, 650);
    buildUi();
    applyToUi(config);
}

void SettingsDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *hint = new QLabel(
        QStringLiteral("设置变更只影响下一次检索，不会打断当前任务。所有数值越界时会回退默认值。"),
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto *tabs = new QTabWidget(this);

    // [网络]
    auto *networkTab = new QWidget(tabs);
    auto *networkForm = new QFormLayout(networkTab);
    networkForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto *uaRow = new QWidget(networkTab);
    auto *uaLayout = new QHBoxLayout(uaRow);
    uaLayout->setContentsMargins(0, 0, 0, 0);
    m_userAgent = new QLineEdit(uaRow);
    m_userAgent->setPlaceholderText(QStringLiteral("留空时使用默认 User-Agent"));
    m_restoreUserAgent = new QPushButton(QStringLiteral("恢复默认 UA"), uaRow);
    uaLayout->addWidget(m_userAgent, 1);
    uaLayout->addWidget(m_restoreUserAgent);
    networkForm->addRow(QStringLiteral("User-Agent："), uaRow);

    m_userAgentHint =
        new QLabel(QStringLiteral("默认 UA 仅用于标识本工具；调试连通性时可勾选下方浏览器 UA。"),
                   networkTab);
    m_userAgentHint->setWordWrap(true);
    m_userAgentHint->setStyleSheet(QStringLiteral("color: #666;"));
    networkForm->addRow(QString(), m_userAgentHint);

    m_browserUserAgent =
        new QCheckBox(QStringLiteral("[出现问题时请勾选此项] 使用浏览器 UA"), networkTab);
    networkForm->addRow(QString(), m_browserUserAgent);

    m_proxyMode = new QComboBox(networkTab);
    m_proxyMode->addItem(QStringLiteral("使用系统代理"), true);
    m_proxyMode->addItem(QStringLiteral("手动 HTTP 代理"), false);
    networkForm->addRow(QStringLiteral("代理设置："), m_proxyMode);

    auto *proxyRow = new QWidget(networkTab);
    auto *proxyLayout = new QHBoxLayout(proxyRow);
    proxyLayout->setContentsMargins(0, 0, 0, 0);
    m_proxyHost = new QLineEdit(proxyRow);
    m_proxyHost->setPlaceholderText(QStringLiteral("代理地址，例如 127.0.0.1"));
    m_proxyPort = makeSpin(proxyRow, AppConfig::kProxyPortMin, AppConfig::kProxyPortMax,
                           QString());
    m_proxyPort->setMinimumWidth(95);
    proxyLayout->addWidget(m_proxyHost, 1);
    proxyLayout->addWidget(new QLabel(QStringLiteral("端口"), proxyRow));
    proxyLayout->addWidget(m_proxyPort);
    networkForm->addRow(QStringLiteral("手动代理："), proxyRow);

    tabs->addTab(networkTab, QStringLiteral("网络"));

    // [Modrinth]
    auto *modrinthTab = new QWidget(tabs);
    auto *modrinthForm = new QFormLayout(modrinthTab);
    modrinthForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_mrAttempts = makeSpin(modrinthTab, AppConfig::kAttemptMin, AppConfig::kAttemptMax,
                            QStringLiteral(" 次"));
    m_mrTimeout = makeSpin(modrinthTab, AppConfig::kTimeoutMin, AppConfig::kTimeoutMax,
                           QStringLiteral(" 秒"));
    m_mrConcurrency = makeSpin(modrinthTab, AppConfig::kConcurrencyMin,
                               AppConfig::kConcurrencyMax, QStringLiteral(" 个"));
    m_mrInterval = makeSpin(modrinthTab, AppConfig::kIntervalMin, AppConfig::kIntervalMax,
                            QStringLiteral(" 毫秒"));
    modrinthForm->addRow(QStringLiteral("最大尝试次数："), m_mrAttempts);
    modrinthForm->addRow(QStringLiteral("单次请求超时："), m_mrTimeout);
    modrinthForm->addRow(QStringLiteral("并发请求数："), m_mrConcurrency);
    modrinthForm->addRow(QStringLiteral("请求最小间隔："), m_mrInterval);
    auto *mrHint = new QLabel(
        QStringLiteral("最大尝试次数是总次数（包含第一次），不是额外重试次数。\n"
                       "并发数是同时在途的异步请求数；最小间隔用于限制请求启动频率。"),
        modrinthTab);
    mrHint->setWordWrap(true);
    mrHint->setStyleSheet(QStringLiteral("color: #666;"));
    modrinthForm->addRow(QString(), mrHint);
    tabs->addTab(modrinthTab, QStringLiteral("Modrinth"));

    // [CurseForge]
    auto *cfTab = new QWidget(tabs);
    auto *cfForm = new QFormLayout(cfTab);
    cfForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_cfAttempts = makeSpin(cfTab, AppConfig::kAttemptMin, AppConfig::kAttemptMax,
                            QStringLiteral(" 次"));
    m_cfTimeout = makeSpin(cfTab, AppConfig::kTimeoutMin, AppConfig::kTimeoutMax,
                           QStringLiteral(" 秒"));
    m_cfConcurrency = makeSpin(cfTab, AppConfig::kConcurrencyMin,
                               AppConfig::kConcurrencyMax, QStringLiteral(" 个"));
    m_cfInterval = makeSpin(cfTab, AppConfig::kIntervalMin, AppConfig::kIntervalMax,
                            QStringLiteral(" 毫秒"));
    cfForm->addRow(QStringLiteral("最大尝试次数："), m_cfAttempts);
    cfForm->addRow(QStringLiteral("单次请求超时："), m_cfTimeout);
    cfForm->addRow(QStringLiteral("并发请求数："), m_cfConcurrency);
    cfForm->addRow(QStringLiteral("请求最小间隔："), m_cfInterval);
    auto *cfHint = new QLabel(
        QStringLiteral("CurseForge 需要 API Key，请在主界面填写。建议保持较低并发以避免限流。"),
        cfTab);
    cfHint->setWordWrap(true);
    cfHint->setStyleSheet(QStringLiteral("color: #666;"));
    cfForm->addRow(QString(), cfHint);
    tabs->addTab(cfTab, QStringLiteral("CurseForge"));

    // [其他]
    auto *otherTab = new QWidget(tabs);
    auto *otherForm = new QFormLayout(otherTab);
    otherForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_recursive = new QCheckBox(QStringLiteral("递归扫描子目录"), otherTab);
    m_mcmod = new QCheckBox(QStringLiteral("启用 MC百科进阶检索"), otherTab);
    m_mcmod->setToolTip(
        QStringLiteral("通过非官方 mcmod-api 查询 MC百科链接；失败不会影响主检索和导出。"));
    m_exportFormat = new QComboBox(otherTab);
    for (const QString &format : AppConfig::exportFormats())
        m_exportFormat->addItem(format);
    m_rememberKey = new QCheckBox(QStringLiteral("记住 CurseForge API Key"), otherTab);

    otherForm->addRow(QString(), m_recursive);
    otherForm->addRow(QString(), m_mcmod);
    otherForm->addRow(QStringLiteral("默认导出格式："), m_exportFormat);
    otherForm->addRow(QString(), m_rememberKey);

    m_rememberWarning = new QLabel(
        QStringLiteral("警告：勾选后 API Key 会以明文写入本地 QSettings 配置文件，"
                       "任何能读取该文件的程序都可能获取它。建议仅在可信电脑上使用。"),
        otherTab);
    m_rememberWarning->setWordWrap(true);
    m_rememberWarning->setStyleSheet(QStringLiteral("color: #b71c1c;"));
    otherForm->addRow(QString(), m_rememberWarning);

    auto *mcmodHint = new QLabel(
        QStringLiteral("MC百科检索不计入主进度，采用非官方 API，并限制为单并发、约 1.2 秒间隔。"),
        otherTab);
    mcmodHint->setWordWrap(true);
    mcmodHint->setStyleSheet(QStringLiteral("color: #666;"));
    otherForm->addRow(QString(), mcmodHint);
    tabs->addTab(otherTab, QStringLiteral("其他"));

    root->addWidget(tabs, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         this);
    auto *restoreButton =
        buttons->addButton(QStringLiteral("恢复默认"), QDialogButtonBox::ResetRole);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    root->addWidget(buttons);

    connect(m_restoreUserAgent, &QPushButton::clicked, this, [this]() {
        m_userAgent->setText(AppConfig::defaultUserAgent());
    });
    connect(m_browserUserAgent, &QCheckBox::toggled, this,
            &SettingsDialog::updateUserAgentState);
    connect(m_proxyMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &SettingsDialog::updateProxyState);
    connect(m_rememberKey, &QCheckBox::toggled, this,
            &SettingsDialog::updateRememberWarning);
    connect(restoreButton, &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

void SettingsDialog::applyToUi(const AppConfig &config)
{
    m_userAgent->setText(config.userAgent);
    m_browserUserAgent->setChecked(config.useBrowserUserAgent);
    m_proxyMode->setCurrentIndex(config.useSystemProxy ? 0 : 1);
    m_proxyHost->setText(config.proxyHost);
    m_proxyPort->setValue(config.proxyPort);

    m_mrAttempts->setValue(config.modrinth.maxAttempts);
    m_mrTimeout->setValue(config.modrinth.timeoutSeconds);
    m_mrConcurrency->setValue(config.modrinth.concurrency);
    m_mrInterval->setValue(config.modrinth.minIntervalMs);

    m_cfAttempts->setValue(config.curseForge.maxAttempts);
    m_cfTimeout->setValue(config.curseForge.timeoutSeconds);
    m_cfConcurrency->setValue(config.curseForge.concurrency);
    m_cfInterval->setValue(config.curseForge.minIntervalMs);

    m_recursive->setChecked(config.recursiveScan);
    m_mcmod->setChecked(config.enableMcModWiki);
    const int formatIndex = m_exportFormat->findText(config.defaultExportFormat);
    m_exportFormat->setCurrentIndex(formatIndex >= 0 ? formatIndex : 0);
    m_rememberKey->setChecked(config.rememberCurseForgeKey);

    updateUserAgentState();
    updateProxyState();
    updateRememberWarning();
}

void SettingsDialog::updateUserAgentState()
{
    const bool browser = m_browserUserAgent->isChecked();
    m_userAgent->setEnabled(!browser);
    if (browser) {
        m_userAgent->setToolTip(
            QStringLiteral("已勾选“使用浏览器 UA”。此处自定义 UA 将被忽略，取消勾选后恢复。"));
    } else {
        m_userAgent->setToolTip(QStringLiteral("留空时自动回退到默认 UA。"));
    }
}

void SettingsDialog::updateProxyState()
{
    const bool manual = m_proxyMode->currentIndex() == 1;
    m_proxyHost->setEnabled(manual);
    m_proxyPort->setEnabled(manual);
}

void SettingsDialog::updateRememberWarning()
{
    m_rememberWarning->setVisible(m_rememberKey->isChecked());
}

AppConfig SettingsDialog::collect() const
{
    AppConfig config = m_original;
    config.userAgent = m_userAgent->text();
    config.useBrowserUserAgent = m_browserUserAgent->isChecked();
    config.useSystemProxy = m_proxyMode->currentIndex() == 0;
    config.proxyHost = m_proxyHost->text();
    config.proxyPort = m_proxyPort->value();

    config.modrinth.maxAttempts = m_mrAttempts->value();
    config.modrinth.timeoutSeconds = m_mrTimeout->value();
    config.modrinth.concurrency = m_mrConcurrency->value();
    config.modrinth.minIntervalMs = m_mrInterval->value();

    config.curseForge.maxAttempts = m_cfAttempts->value();
    config.curseForge.timeoutSeconds = m_cfTimeout->value();
    config.curseForge.concurrency = m_cfConcurrency->value();
    config.curseForge.minIntervalMs = m_cfInterval->value();

    config.recursiveScan = m_recursive->isChecked();
    config.enableMcModWiki = m_mcmod->isChecked();
    config.defaultExportFormat = m_exportFormat->currentText();
    config.rememberCurseForgeKey = m_rememberKey->isChecked();
    config.sanitize();
    return config;
}

void SettingsDialog::restoreDefaults()
{
    AppConfig config = AppConfig::defaults();
    // “恢复默认”不销毁用户当前输入的 Key，只恢复其他设置。
    config.curseForgeApiKey = m_original.curseForgeApiKey;
    config.useBrowserUserAgent = false;
    applyToUi(config);
}

void SettingsDialog::accept()
{
    const bool manualProxyEmpty =
        m_proxyMode->currentIndex() == 1 && m_proxyHost->text().trimmed().isEmpty();
    AppConfig config = collect();

    if (manualProxyEmpty) {
        QMessageBox::warning(this, QStringLiteral("代理设置"),
                             QStringLiteral("手动代理地址为空，已回退为使用系统代理。"));
    }

    if (config.rememberCurseForgeKey) {
        const QMessageBox::StandardButton answer = QMessageBox::warning(
            this, QStringLiteral("明文存储风险"),
            QStringLiteral("你选择了记住 CurseForge API Key。\n\n"
                           "Key 将以明文保存在本机 QSettings 配置文件中，"
                           "能够读取该文件的程序都可能获取它。\n\n是否继续保存？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    m_result = config;
    QDialog::accept();
}
