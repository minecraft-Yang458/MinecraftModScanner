#include "MainWindow.h"

#include "ActionButtonDelegate.h"
#include "AppLog.h"
#include "CurseForgeClient.h"
#include "DetailsDialog.h"
#include "ExportService.h"
#include "ManualSearchDialog.h"
#include "McModWikiClient.h"
#include "NetworkTaskManager.h"
#include "ResultModel.h"
#include "ScanCleaner.h"
#include "SettingsDialog.h"
#include "SummaryDialog.h"
#include "UrlLauncher.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString shortError(const QString &error, int length = 120)
{
    const QString text = error.simplified();
    return text.size() > length ? text.left(length - 1) + QChar(0x2026) : text;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_model(new ResultModel(this))
    , m_searchManager(new NetworkTaskManager(this))
{
    setWindowTitle(QStringLiteral("Minecraft 模组自动检索"));
    setWindowIcon(QIcon(QStringLiteral(":/app.svg")));
    resize(1600, 900);

    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDirectory);
    m_settings = new QSettings(QDir(dataDirectory).filePath(QStringLiteral("settings.ini")),
                               QSettings::IniFormat, this);
    qInfo().noquote() << QStringLiteral("Settings file: %1").arg(m_settings->fileName());

    m_actionDelegate = new ActionButtonDelegate(this);
    buildUi();
    buildMenus();
    connectSignals();
    loadInitialSettings();

    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(QStringLiteral("尚未开始"));
    m_statusLabel->setText(QStringLiteral(
        "请选择 mods 文件夹。检索过程中界面保持响应，可随时取消；单击直链可打开浏览器。"));
    updateUiState();
    updateProgressDisplay(0, 0);
    m_cancelButton->setEnabled(false);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *configBox = new QGroupBox(QStringLiteral("配置"), central);
    auto *configGrid = new QGridLayout(configBox);
    configGrid->setHorizontalSpacing(8);
    configGrid->setVerticalSpacing(6);

    m_chooseButton = new QPushButton(QStringLiteral("选择 mods 文件夹"), configBox);
    m_pathEdit = new QLineEdit(configBox);
    m_pathEdit->setReadOnly(true);
    m_pathEdit->setFrame(false);
    m_pathEdit->setPlaceholderText(QStringLiteral("尚未选择文件夹"));
    m_pathEdit->setToolTip(QStringLiteral("当前 mods 文件夹"));

    m_startButton = new QPushButton(QStringLiteral("开始检索"), configBox);
    m_startButton->setObjectName(QStringLiteral("primaryButton"));

    m_exportButton = new QToolButton(configBox);
    m_exportButton->setText(QStringLiteral("导出"));
    m_exportButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_exportButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_exportButton->setToolTip(QStringLiteral("点击按默认格式导出；箭头选择格式"));

    m_keyEdit = new QLineEdit(configBox);
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setPlaceholderText(
        QStringLiteral("CurseForge API Key；不填写则仅检索 Modrinth"));
    m_keyEdit->setClearButtonEnabled(false);

    m_validateButton = new QPushButton(QStringLiteral("验证 Key / 检查连接"), configBox);
    auto *settingsButton = new QPushButton(QStringLiteral("设置…"), configBox);

    configGrid->addWidget(m_chooseButton, 0, 0);
    configGrid->addWidget(m_pathEdit, 0, 1, 1, 5);
    configGrid->addWidget(m_startButton, 0, 6);
    configGrid->addWidget(m_exportButton, 0, 7);

    configGrid->addWidget(new QLabel(QStringLiteral("CurseForge API Key："), configBox), 1, 0);
    configGrid->addWidget(m_keyEdit, 1, 1, 1, 3);
    configGrid->addWidget(m_validateButton, 1, 4);
    configGrid->addWidget(settingsButton, 1, 6);
    configGrid->setColumnStretch(1, 1);
    root->addWidget(configBox);

    auto *progressBox = new QGroupBox(QStringLiteral("进度"), central);
    auto *progressLayout = new QVBoxLayout(progressBox);
    auto *progressRow = new QHBoxLayout;
    m_progressBar = new QProgressBar(progressBox);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMinimumHeight(24);
    m_progressBar->setLocale(QLocale::c());
    QFont progressFont = QApplication::font();
    progressFont.setWeight(QFont::Normal);
    m_progressBar->setFont(progressFont);
    m_cancelButton = new QPushButton(QStringLiteral("取消"), progressBox);
    m_cancelButton->setObjectName(QStringLiteral("dangerButton"));
    progressRow->addWidget(m_progressBar, 1);
    progressRow->addWidget(m_cancelButton);
    progressLayout->addLayout(progressRow);

    m_statusLabel = new QLabel(progressBox);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLabel->setMinimumHeight(32);
    progressLayout->addWidget(m_statusLabel);
    root->addWidget(progressBox);

    m_table = new QTableView(central);
    m_table->setModel(m_model);
    m_table->setItemDelegateForColumn(ResultModel::Operation, m_actionDelegate);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->setSortingEnabled(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    const QList<int> widths = {215, 165, 165, 190, 120, 115, 125, 260,
                               190, 105, 115, 125, 260, 230, 310, 84};
    for (int i = 0; i < widths.size() && i < ResultModel::ColumnCount; ++i)
        m_table->setColumnWidth(i, widths.at(i));
    for (int column : {static_cast<int>(ResultModel::ModrinthId),
                       static_cast<int>(ResultModel::ModrinthAuthor),
                       static_cast<int>(ResultModel::ModrinthLicense),
                       static_cast<int>(ResultModel::CurseForgeId),
                       static_cast<int>(ResultModel::CurseForgeAuthor),
                       static_cast<int>(ResultModel::CurseForgeLicense),
                       static_cast<int>(ResultModel::Operation)}) {
        m_table->horizontalHeader()->setSectionResizeMode(column,
                                                          QHeaderView::ResizeToContents);
    }
    root->addWidget(m_table, 1);

    setCentralWidget(central);

    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(m_chooseButton, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startSearch);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::cancelSearch);
    connect(m_exportButton, &QToolButton::clicked, this, &MainWindow::exportDefaultFormat);
    connect(m_validateButton, &QPushButton::clicked, this, &MainWindow::validateCurseForgeKey);
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    auto *chooseAction = fileMenu->addAction(QStringLiteral("选择 mods 文件夹…"));
    chooseAction->setShortcut(QKeySequence::Open);
    connect(chooseAction, &QAction::triggered, this, &MainWindow::chooseFolder);

    m_startAction = fileMenu->addAction(QStringLiteral("开始检索"));
    m_startAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(m_startAction, &QAction::triggered, this, &MainWindow::startSearch);

    m_exportCsvAction = new QAction(QStringLiteral("导出 CSV…"), this);
    m_exportMarkdownAction = new QAction(QStringLiteral("导出 Markdown…"), this);
    m_exportExcelAction = new QAction(QStringLiteral("导出 Excel…"), this);
    m_exportQuickAction = new QAction(QStringLiteral("快速导出到默认目录"), this);
    connect(m_exportCsvAction, &QAction::triggered, this,
            [this]() { exportResults(ExportFormat::Csv); });
    connect(m_exportMarkdownAction, &QAction::triggered, this,
            [this]() { exportResults(ExportFormat::Markdown); });
    connect(m_exportExcelAction, &QAction::triggered, this,
            [this]() { exportResults(ExportFormat::Excel); });
    connect(m_exportQuickAction, &QAction::triggered, this, [this]() {
        exportQuick(ExportService::formatFromString(m_config.defaultExportFormat));
    });

    auto *fileExportMenu = fileMenu->addMenu(QStringLiteral("导出"));
    fileExportMenu->addAction(m_exportCsvAction);
    fileExportMenu->addAction(m_exportMarkdownAction);
    fileExportMenu->addAction(m_exportExcelAction);
    fileExportMenu->addSeparator();
    fileExportMenu->addAction(m_exportQuickAction);
    auto *openExportFolderAction =
        new QAction(QStringLiteral("打开导出文件夹"), this);
    connect(openExportFolderAction, &QAction::triggered, this, [this]() {
        UrlLauncher::openInteractive(this,
                                     QUrl::fromLocalFile(defaultExportDirectory()),
                                     QStringLiteral("导出文件夹"));
    });
    fileExportMenu->addAction(openExportFolderAction);
    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(QStringLiteral("退出"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    m_settingsAction = menuBar()->addMenu(QStringLiteral("设置(&S)"))->addAction(
        QStringLiteral("首选项…"));
    m_settingsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    auto *toolsMenu = menuBar()->addMenu(QStringLiteral("工具(&T)"));
    m_manualAction = toolsMenu->addAction(QStringLiteral("单独检索…"));
    m_manualAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    connect(m_manualAction, &QAction::triggered, this, &MainWindow::openStandaloneSearch);
    auto *summaryAction = toolsMenu->addAction(QStringLiteral("总结视图…"));
    summaryAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    connect(summaryAction, &QAction::triggered, this, &MainWindow::openSummary);

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    auto *keyHelpAction = helpMenu->addAction(QStringLiteral("如何获取 CurseForge API Key"));
    connect(keyHelpAction, &QAction::triggered, this, &MainWindow::showApiKeyHelp);
    auto *logAction = helpMenu->addAction(QStringLiteral("打开日志文件夹"));
    connect(logAction, &QAction::triggered, this, [this]() {
        const QString path = QFileInfo(AppLog::logFilePath()).absolutePath();
        UrlLauncher::openInteractive(this, QUrl::fromLocalFile(path),
                                     QStringLiteral("日志文件夹"));
    });

    auto *aboutMenu = menuBar()->addMenu(QStringLiteral("关于(&A)"));
    auto *aboutAction = aboutMenu->addAction(QStringLiteral("关于 Minecraft Mod Scanner"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    auto *licenseAction = aboutMenu->addAction(QStringLiteral("许可证：CC0 1.0"));
    connect(licenseAction, &QAction::triggered, this, &MainWindow::showLicense);

    auto *buttonExportMenu = new QMenu(m_exportButton);
    buttonExportMenu->addAction(m_exportCsvAction);
    buttonExportMenu->addAction(m_exportMarkdownAction);
    buttonExportMenu->addAction(m_exportExcelAction);
    buttonExportMenu->addSeparator();
    buttonExportMenu->addAction(m_exportQuickAction);
    buttonExportMenu->addAction(openExportFolderAction);
    m_exportButton->setMenu(buttonExportMenu);

    statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::connectSignals()
{
    connect(m_keyEdit, &QLineEdit::editingFinished, this, [this]() {
        m_config.curseForgeApiKey = m_keyEdit->text().trimmed();
        if (m_config.rememberCurseForgeKey)
            persistSettings();
    });

    connect(m_model, &ResultModel::rowCountChanged, this, [this]() {
        updateUiState();
        updateStartActionState();
    });

    connect(m_actionDelegate, &ActionButtonDelegate::clicked, this,
            [this](const QModelIndex &index, const QPoint &globalPosition) {
                showRowMenu(index, globalPosition);
            });

    connect(m_table, &QTableView::clicked, this, [this](const QModelIndex &index) {
        if (!index.isValid())
            return;
        if (index.column() != ResultModel::ModrinthUrl
            && index.column() != ResultModel::CurseForgeUrl
            && index.column() != ResultModel::McModUrl) {
            return;
        }
        const QString urlText = m_model->data(index, Qt::DisplayRole).toString();
        if (urlText.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
            || urlText.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
            QString description = QStringLiteral("链接");
            if (index.column() == ResultModel::ModrinthUrl)
                description = QStringLiteral("Modrinth 直链");
            else if (index.column() == ResultModel::CurseForgeUrl)
                description = QStringLiteral("CurseForge 直链");
            else if (index.column() == ResultModel::McModUrl)
                description = QStringLiteral("MC百科链接");
            UrlLauncher::openInteractive(this, QUrl(urlText), description);
        }
    });

    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (m_running || m_singleRetryActive)
            return;
        const QModelIndex index = m_table->indexAt(pos);
        if (!index.isValid())
            return;
        showRowMenu(m_model->index(index.row(), ResultModel::Operation),
                    m_table->viewport()->mapToGlobal(pos));
    });

    connect(m_searchManager, &NetworkTaskManager::searchStarted, this,
            &MainWindow::onSearchStarted);
    connect(m_searchManager, &NetworkTaskManager::searchRetry, this,
            &MainWindow::onSearchRetry);
    connect(m_searchManager, &NetworkTaskManager::searchFinished, this,
            &MainWindow::onSearchFinished);
    connect(m_searchManager, &NetworkTaskManager::connectionChecked, this,
            &MainWindow::onConnectionChecked);
    connect(m_searchManager, &NetworkTaskManager::connectionCheckFinished, this,
            &MainWindow::onConnectionCheckFinished);

    m_wikiClient = new McModWikiClient(this);
    connect(m_wikiClient, &McModWikiClient::lookupFinished, this,
            &MainWindow::onWikiLookupFinished);
}

void MainWindow::loadInitialSettings()
{
    m_config = AppConfig::load(*m_settings);
    if (m_config.rememberCurseForgeKey)
        m_keyEdit->setText(m_config.curseForgeApiKey);
    else
        m_keyEdit->clear();

    const QByteArray geometry = m_settings->value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray state = m_settings->value(QStringLiteral("window/state")).toByteArray();
    if (!state.isEmpty())
        restoreState(state);

    m_currentDirectory = m_settings->value(QStringLiteral("paths/modsDir")).toString();
    if (!m_currentDirectory.isEmpty()) {
        m_pathEdit->setText(QDir::toNativeSeparators(m_currentDirectory));
        m_pathEdit->setToolTip(m_currentDirectory);
    }
}

void MainWindow::persistSettings()
{
    if (!m_settings)
        return;
    m_config.curseForgeApiKey = m_keyEdit->text().trimmed();
    m_config.save(*m_settings);
    m_settings->setValue(QStringLiteral("paths/modsDir"), m_currentDirectory);
    m_settings->sync();
    qInfo().noquote() << QStringLiteral("Settings saved: %1 (status %2)")
                             .arg(m_settings->fileName())
                             .arg(static_cast<int>(m_settings->status()));
}

void MainWindow::chooseFolder()
{
    if (m_running || m_singleRetryActive)
        return;
    const QString startDir = m_currentDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        : m_currentDirectory;
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择 mods 文件夹"), startDir);
    if (directory.isEmpty())
        return;
    m_currentDirectory = QDir(directory).absolutePath();
    m_pathEdit->setText(QDir::toNativeSeparators(m_currentDirectory));
    m_pathEdit->setToolTip(m_currentDirectory);
    m_settings->setValue(QStringLiteral("paths/modsDir"), m_currentDirectory);
    rescanFolder();
}

void MainWindow::rescanFolder()
{
    if (m_currentDirectory.isEmpty())
        return;

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const ScanOutcome outcome =
        ScanCleaner::scanDirectory(m_currentDirectory, m_config.recursiveScan);
    QGuiApplication::restoreOverrideCursor();

    QVector<ModEntry> entries;
    entries.reserve(outcome.files.size());
    int anomalies = 0;
    for (const CleanedModFile &file : outcome.files) {
        ModEntry entry;
        entry.originalFileName = file.originalFileName;
        entry.baseCleanName = file.baseCleanName;
        entry.searchName = file.searchName;
        entry.cleanerNote = file.cleanerNote;
        entry.cleanAnomaly = file.cleanAnomaly;
        if (entry.cleanAnomaly)
            ++anomalies;
        entries.append(entry);
    }
    m_model->setEntries(entries);
    m_phase = Phase::Idle;
    m_running = false;
    m_pendingTasks.clear();
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(QStringLiteral("待检索"));
    updateUiState();

    if (entries.isEmpty()) {
        setStatusLabel(QStringLiteral("未找到 .jar 文件。"));
        QMessageBox::information(this, QStringLiteral("扫描完成"),
                                 QStringLiteral("在所选文件夹中未找到 .jar 文件。"));
        return;
    }

    setStatusLabel(QStringLiteral("已扫描 %1 个 .jar 文件；其中清洗异常 %2 个。")
                       .arg(entries.size())
                       .arg(anomalies));
    QString message =
        QStringLiteral("已扫描 %1 个 .jar 文件，并完成基础清洗与加载器清洗。\n\n"
                       "请点击“开始检索”。")
            .arg(entries.size());
    if (anomalies > 0)
        message += QStringLiteral("\n\n有 %1 个文件清洗结果为空，已回退原始文件名并标记异常。")
                       .arg(anomalies);
    if (!outcome.warnings.isEmpty())
        message += QStringLiteral("\n\n") + outcome.warnings.join(QLatin1Char('\n'));
    QMessageBox::information(this, QStringLiteral("扫描完成"), message);
}

void MainWindow::startSearch()
{
    if (m_running || m_singleRetryActive)
        return;

    int scannedRows = 0;
    for (const ModEntry &entry : m_model->entries()) {
        if (!entry.manualRow)
            ++scannedRows;
    }
    if (scannedRows == 0) {
        QMessageBox::information(
            this, QStringLiteral("没有可检索的行"),
            QStringLiteral("请先选择 mods 文件夹并扫描 .jar 文件。手动添加的行不会参与主检索。"));
        return;
    }

    m_config.curseForgeApiKey = m_keyEdit->text().trimmed();
    if (m_config.rememberCurseForgeKey)
        persistSettings();

    bool curseForgeEnabled = !m_config.curseForgeApiKey.isEmpty();
    if (!curseForgeEnabled) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(QStringLiteral("CurseForge API Key"));
        box.setText(QStringLiteral(
            "未填写 CurseForge API Key。\n\n"
            "不填写将禁用 CurseForge 检索，只使用 Modrinth。\n"
            "也可以现在填写 Key 后再开始。"));
        QPushButton *skip = box.addButton(
            QStringLiteral("仅检索 Modrinth（不填写）"), QMessageBox::AcceptRole);
        QPushButton *fill = box.addButton(QStringLiteral("去填写"), QMessageBox::ActionRole);
        QPushButton *cancel = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == cancel)
            return;
        if (box.clickedButton() == fill) {
            m_keyEdit->setFocus();
            m_keyEdit->selectAll();
            return;
        }
        if (box.clickedButton() != skip)
            return;
        curseForgeEnabled = false;
    }

    m_jobConfig = m_config;
    m_jobConfig.sanitize();
    m_jobConfig.curseForgeApiKey = m_config.curseForgeApiKey;
    m_modrinthEnabled = true;
    m_curseForgeEnabled = curseForgeEnabled;
    m_running = true;
    m_cancelRequested = false;
    m_phase = Phase::Connecting;
    m_connectionResults.clear();
    m_pendingTasks.clear();
    m_completed = 0;
    m_total = 0;
    m_successCount = 0;
    m_notFoundCount = 0;
    m_failedCount = 0;
    ++m_jobId;

    m_searchManager->configure(m_jobConfig);
    updateUiState();

    m_progressBar->setRange(0, 0);
    m_progressBar->setFormat(QStringLiteral("正在检查连接…"));
    setStatusLabel(QStringLiteral("开始检索前正在检查 Modrinth%1 的连通性…")
                       .arg(m_curseForgeEnabled ? QStringLiteral(" / CurseForge")
                                                : QString()));
    m_searchManager->checkConnections(true, m_curseForgeEnabled);
}

void MainWindow::onConnectionChecked(int platform, bool ok, const QString &message,
                                     int httpStatus)
{
    Q_UNUSED(httpStatus);
    if (m_phase != Phase::Connecting)
        return;
    m_connectionResults.insert(platform, qMakePair(ok, message));
    setStatusLabel(QStringLiteral("%1 连通性：%2 — %3")
                       .arg(platformName(platformFromInt(platform)),
                            ok ? QStringLiteral("正常") : QStringLiteral("失败"), message));
}

void MainWindow::onConnectionCheckFinished()
{
    if (m_phase != Phase::Connecting || m_cancelRequested)
        return;

    const auto mr = m_connectionResults.value(platformToInt(Platform::Modrinth));
    const auto cf = m_connectionResults.value(platformToInt(Platform::CurseForge));
    const bool mrOk = mr.first;
    const bool cfOk = m_curseForgeEnabled ? cf.first : false;

    QStringList failures;
    if (!mrOk) {
        failures.append(QStringLiteral("Modrinth：%1")
                            .arg(mr.second.isEmpty() ? QStringLiteral("连接检查失败") : mr.second));
    }
    if (m_curseForgeEnabled && !cfOk) {
        failures.append(QStringLiteral("CurseForge：%1")
                            .arg(cf.second.isEmpty() ? QStringLiteral("连接检查失败") : cf.second));
    }

    if (!mrOk && !cfOk) {
        QMessageBox::critical(
            this, QStringLiteral("连通性检查失败"),
            QStringLiteral("无法连接任何检索平台：\n\n%1\n\n请检查网络、代理和 API Key。")
                .arg(failures.join(QLatin1Char('\n'))));
        abortBeforeSearch(QStringLiteral("已取消：连通性检查失败。"));
        return;
    }

    if (!failures.isEmpty()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(QStringLiteral("部分平台连接失败"));
        box.setText(QStringLiteral("以下平台无法使用：\n\n%1\n\n是否继续使用其余平台检索？")
                        .arg(failures.join(QLatin1Char('\n'))));
        QPushButton *continueButton = box.addButton(
            QStringLiteral("继续可用平台"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() != continueButton) {
            abortBeforeSearch(QStringLiteral("已取消。"));
            return;
        }
    }

    m_modrinthEnabled = mrOk;
    m_curseForgeEnabled = m_curseForgeEnabled && cfOk;
    if (!m_modrinthEnabled && !m_curseForgeEnabled) {
        abortBeforeSearch(QStringLiteral("已取消：没有可用平台。"));
        return;
    }

    beginSearchPhase();
}

void MainWindow::beginSearchPhase()
{
    if (m_cancelRequested)
        return;

    m_phase = Phase::Searching;
    m_completed = 0;
    m_successCount = 0;
    m_notFoundCount = 0;
    m_failedCount = 0;
    m_pendingTasks.clear();

    struct Job
    {
        quint64 uid = 0;
        Platform platform = Platform::Modrinth;
        QString searchName;
    };
    QVector<Job> jobs;

    for (const ModEntry &entry : m_model->entries()) {
        if (entry.manualRow)
            continue;

        m_model->setMcModLookup(entry.uid, QString(), QString(), QString());
        m_model->setRowNote(entry.uid, QString());

        if (m_modrinthEnabled) {
            m_model->preparePlatformForSearch(entry.uid, Platform::Modrinth);
        } else {
            m_model->markPlatformSkipped(entry.uid, Platform::Modrinth,
                                         platformSkipReason(Platform::Modrinth));
        }

        if (m_curseForgeEnabled) {
            m_model->preparePlatformForSearch(entry.uid, Platform::CurseForge);
        } else {
            m_model->markPlatformSkipped(entry.uid, Platform::CurseForge,
                                         platformSkipReason(Platform::CurseForge));
        }

        if (entry.searchName.trimmed().isEmpty()) {
            m_model->markPlatformSkipped(entry.uid, Platform::Modrinth,
                                         QStringLiteral("检索用名称为空，已跳过"));
            m_model->markPlatformSkipped(entry.uid, Platform::CurseForge,
                                         QStringLiteral("检索用名称为空，已跳过"));
            continue;
        }

        if (m_modrinthEnabled) {
            Job job;
            job.uid = entry.uid;
            job.platform = Platform::Modrinth;
            job.searchName = entry.searchName;
            jobs.append(job);
        }
        if (m_curseForgeEnabled) {
            Job job;
            job.uid = entry.uid;
            job.platform = Platform::CurseForge;
            job.searchName = entry.searchName;
            jobs.append(job);
        }
    }

    m_total = jobs.size();
    if (m_total == 0) {
        abortBeforeSearch(QStringLiteral("已取消：没有可提交的检索任务。"));
        return;
    }

    m_progressBar->setRange(0, m_total);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(QStringLiteral("0 / %1（已完成）").arg(m_total));
    updateUiState();

    for (const Job &job : jobs) {
        const QString tag = QStringLiteral("job%1-r%2-p%3")
                                .arg(m_jobId)
                                .arg(job.uid)
                                .arg(platformToInt(job.platform));
        PendingTask pending;
        pending.uid = job.uid;
        pending.platform = job.platform;
        m_pendingTasks.insert(tag, pending);
        m_searchManager->submitSearch(job.platform, job.searchName, tag);
    }

    setStatusLabel(QStringLiteral("已提交 %1 个检索任务（Modrinth 并发 %2，CurseForge 并发 %3）。")
                       .arg(m_total)
                       .arg(m_jobConfig.modrinth.concurrency)
                       .arg(m_jobConfig.curseForge.concurrency));
}

void MainWindow::onSearchStarted(const QString &tag, int platform, const QString &query,
                                 int attempt, int maxAttempts)
{
    Q_UNUSED(query);
    if (m_phase != Phase::Searching)
        return;
    const auto it = m_pendingTasks.constFind(tag);
    if (it == m_pendingTasks.constEnd())
        return;
    const PendingTask pending = it.value();
    const ModEntry *entry = m_model->entryById(pending.uid);
    if (!entry)
        return;

    m_model->setPlatformState(pending.uid, pending.platform, SearchState::Searching,
                              qMax(0, attempt - 1), QString());
    setStatusLabel(QStringLiteral("正在检索：%1 · %2（尝试 %3/%4）")
                       .arg(entry->originalFileName,
                            platformName(platformFromInt(platform)))
                       .arg(attempt)
                       .arg(maxAttempts));
}

void MainWindow::onSearchRetry(const QString &tag, int platform, const QString &query,
                               int nextAttempt, int maxAttempts, int delayMs,
                               const QString &reason)
{
    Q_UNUSED(query);
    if (m_phase != Phase::Searching)
        return;
    const auto it = m_pendingTasks.constFind(tag);
    if (it == m_pendingTasks.constEnd())
        return;
    const PendingTask pending = it.value();
    const ModEntry *entry = m_model->entryById(pending.uid);
    if (!entry)
        return;

    m_model->setPlatformState(pending.uid, pending.platform, SearchState::RetryWaiting,
                              qMax(0, nextAttempt - 1), reason);
    setStatusLabel(QStringLiteral("重试 %1/%2：%3 · %4 — %5（等待 %6 ms）")
                       .arg(nextAttempt)
                       .arg(maxAttempts)
                       .arg(entry->originalFileName,
                            platformName(platformFromInt(platform)), reason)
                       .arg(delayMs),
                   reason);
}

void MainWindow::onSearchFinished(const QString &tag, int platform,
                                  const PlatformResult &result)
{
    if (m_phase != Phase::Searching)
        return;
    const auto it = m_pendingTasks.find(tag);
    if (it == m_pendingTasks.end())
        return;
    const PendingTask pending = it.value();
    m_pendingTasks.erase(it);

    m_model->setPlatformResult(pending.uid, pending.platform, result);
    ++m_completed;
    switch (result.state) {
    case SearchState::Success:
        ++m_successCount;
        break;
    case SearchState::NotFound:
        ++m_notFoundCount;
        break;
    case SearchState::Failed:
        ++m_failedCount;
        break;
    default:
        break;
    }

    updateProgressDisplay(m_completed, m_total);

    const ModEntry *entry = m_model->entryById(pending.uid);
    const QString fileName = entry ? entry->originalFileName : QStringLiteral("（已删除行）");
    QString status = QStringLiteral("%1 · %2：%3")
                         .arg(fileName, platformName(platformFromInt(platform)),
                              stateText(result.state));
    if (!result.error.trimmed().isEmpty()
        && (result.state == SearchState::Failed || result.state == SearchState::NotFound)) {
        status += QStringLiteral(" — %1").arg(shortError(result.error));
    }
    setStatusLabel(status, result.error);

    if (m_completed >= m_total && m_phase == Phase::Searching) {
        QTimer::singleShot(0, this, [this]() {
            if (m_phase == Phase::Searching && m_completed >= m_total)
                finishPlatformPhase();
        });
    }
}

void MainWindow::finishPlatformPhase()
{
    if (m_phase != Phase::Searching)
        return;

    if (!m_jobConfig.enableMcModWiki) {
        finishJob(false);
        return;
    }

    QVector<QPair<quint64, QString>> targets;
    for (const ModEntry &entry : m_model->entries()) {
        QString name;
        if (entry.results[0].isSuccess() && !entry.results[0].name.trimmed().isEmpty())
            name = entry.results[0].name;
        else if (entry.results[1].isSuccess() && !entry.results[1].name.trimmed().isEmpty())
            name = entry.results[1].name;
        if (name.trimmed().isEmpty())
            continue;
        targets.append(qMakePair(entry.uid, name));
    }

    if (targets.isEmpty()) {
        finishJob(false);
        return;
    }

    m_phase = Phase::Wiki;
    m_wikiTotal = targets.size();
    m_wikiDone = 0;
    m_wikiMatched = 0;
    m_wikiClient->cancelAll();
    m_wikiClient->configure(m_jobConfig);
    m_progressBar->setValue(m_total);
    m_progressBar->setFormat(QStringLiteral("%1 / %1（主检索完成）").arg(m_total));

    for (const auto &target : targets) {
        m_model->setMcModLookup(target.first, QString(), QString(), QStringLiteral("检索中"));
        m_wikiClient->lookup(QString::number(target.first), target.second);
    }

    setStatusLabel(QStringLiteral("主检索完成：成功 %1，未找到 %2，失败 %3；"
                                  "MC百科检索 0/%4（不计入主进度）")
                       .arg(m_successCount)
                       .arg(m_notFoundCount)
                       .arg(m_failedCount)
                       .arg(m_wikiTotal));
    updateUiState();
}

void MainWindow::onWikiLookupFinished(const McModWikiResult &result)
{
    if (m_phase != Phase::Wiki)
        return;

    bool ok = false;
    const quint64 uid = result.tag.toULongLong(&ok);
    if (ok) {
        QString status;
        if (result.matched)
            status = QStringLiteral("已匹配");
        else if (result.fallbackSearch)
            status = QStringLiteral("已回退搜索页");
        else
            status = QStringLiteral("失败");
        if (!result.error.isEmpty() && !result.matched)
            status += QStringLiteral("：%1").arg(result.error);
        m_model->setMcModLookup(uid, result.url, result.title, status);
        if (result.matched)
            ++m_wikiMatched;
    }

    ++m_wikiDone;
    const QString wikiState = result.matched
        ? QStringLiteral("已匹配")
        : (result.fallbackSearch ? QStringLiteral("已回退搜索页")
                                 : QStringLiteral("失败"));
    setStatusLabel(QStringLiteral("MC百科检索 %1/%2：%3 — %4；成功匹配 %5（不计入主进度）")
                       .arg(m_wikiDone)
                       .arg(m_wikiTotal)
                       .arg(result.name, wikiState)
                       .arg(m_wikiMatched),
                   result.error);
    if (m_wikiDone >= m_wikiTotal)
        finishJob(false);
}

void MainWindow::finishJob(bool canceled)
{
    if (m_phase == Phase::Idle && !m_running)
        return;

    if (canceled) {
        if (m_searchManager)
            m_searchManager->cancelAll();
        if (m_wikiClient)
            m_wikiClient->cancelAll();
    }

    m_running = false;
    m_cancelRequested = false;
    m_phase = Phase::Idle;
    m_pendingTasks.clear();
    updateUiState();

    if (canceled) {
        setStatusLabel(QStringLiteral("已取消：成功 %1，未找到 %2，失败 %3；已保留已有结果。")
                           .arg(m_successCount)
                           .arg(m_notFoundCount)
                           .arg(m_failedCount));
        statusBar()->showMessage(QStringLiteral("检索已取消"), 5000);
        return;
    }

    const QString summary =
        QStringLiteral("检索完成：成功 %1，未找到 %2，失败 %3。%4")
            .arg(m_successCount)
            .arg(m_notFoundCount)
            .arg(m_failedCount)
            .arg(m_jobConfig.enableMcModWiki
                     ? QStringLiteral("MC百科匹配 %1/%2（失败不影响主检索和导出）。")
                           .arg(m_wikiMatched)
                           .arg(m_wikiTotal)
                     : QString());
    setStatusLabel(summary);
    statusBar()->showMessage(QStringLiteral("检索完成"), 5000);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("检索完成"));
    box.setText(summary);
    QPushButton *viewButton = box.addButton(QStringLiteral("查看总结"),
                                            QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == viewButton)
        openSummary();
}

void MainWindow::cancelSearch()
{
    if (!m_running)
        return;

    m_cancelRequested = true;
    if (m_phase == Phase::Connecting) {
        m_searchManager->cancelChecks();
        m_running = false;
        m_phase = Phase::Idle;
        m_progressBar->setRange(0, 1);
        m_progressBar->setValue(0);
        m_progressBar->setFormat(QStringLiteral("已取消"));
        setStatusLabel(QStringLiteral("已取消连通性检查。"));
        updateUiState();
        return;
    }

    if (m_phase == Phase::Searching) {
        m_searchManager->cancelAll();
        for (auto it = m_pendingTasks.constBegin(); it != m_pendingTasks.constEnd(); ++it) {
            m_model->markPlatformCancelled(it.value().uid, it.value().platform,
                                           QStringLiteral("用户取消"));
        }
        m_pendingTasks.clear();
        finishJob(true);
        return;
    }

    if (m_phase == Phase::Wiki) {
        m_wikiClient->cancelAll();
        for (const ModEntry &entry : m_model->entries()) {
            if (entry.mcmodStatus == QStringLiteral("检索中"))
                m_model->setMcModLookup(entry.uid, QString(), QString(),
                                        QStringLiteral("已取消"));
        }
        finishJob(true);
    }
}

void MainWindow::abortBeforeSearch(const QString &message)
{
    if (m_phase == Phase::Connecting)
        m_searchManager->cancelChecks();
    m_running = false;
    m_phase = Phase::Idle;
    m_cancelRequested = false;
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(QStringLiteral("未开始"));
    setStatusLabel(message);
    updateUiState();
}

void MainWindow::updateUiState()
{
    const bool idle = !m_running && !m_singleRetryActive;
    const bool hasRows = m_model && m_model->rowCount() > 0;

    if (m_chooseButton)
        m_chooseButton->setEnabled(idle);
    if (m_keyEdit)
        m_keyEdit->setEnabled(!m_running);
    if (m_validateButton)
        m_validateButton->setEnabled(!m_running);
    if (m_cancelButton)
        m_cancelButton->setEnabled(m_running);
    if (m_exportButton)
        m_exportButton->setEnabled(idle && hasRows);
    if (m_manualAction)
        m_manualAction->setEnabled(!m_running);
    if (m_exportCsvAction)
        m_exportCsvAction->setEnabled(idle && hasRows);
    if (m_exportMarkdownAction)
        m_exportMarkdownAction->setEnabled(idle && hasRows);
    if (m_exportExcelAction)
        m_exportExcelAction->setEnabled(idle && hasRows);
    if (m_exportQuickAction)
        m_exportQuickAction->setEnabled(idle && hasRows);

    updateStartActionState();

    if (m_actionDelegate)
        m_actionDelegate->setEnabled(idle);
    if (m_model)
        m_model->setActionsEnabled(idle);
}

void MainWindow::updateStartActionState()
{
    bool hasSearchable = false;
    if (m_model) {
        for (const ModEntry &entry : m_model->entries()) {
            if (!entry.manualRow) {
                hasSearchable = true;
                break;
            }
        }
    }
    const bool canStart = hasSearchable && !m_running && !m_singleRetryActive;
    if (m_startButton)
        m_startButton->setEnabled(canStart);
    if (m_startAction)
        m_startAction->setEnabled(canStart);
}

void MainWindow::updateProgressDisplay(int completed, int total)
{
    if (!m_progressBar)
        return;
    if (total <= 0) {
        m_progressBar->setRange(0, 1);
        m_progressBar->setValue(0);
        m_progressBar->setFormat(QStringLiteral("尚未开始"));
        return;
    }
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(qBound(0, completed, total));
    m_progressBar->setFormat(
        QStringLiteral("%1 / %2（已完成）")
            .arg(QString::number(qBound(0, completed, total)), QString::number(total)));
}

void MainWindow::setStatusLabel(const QString &text, const QString &tooltip)
{
    if (!m_statusLabel)
        return;
    m_statusLabel->setText(text);
    m_statusLabel->setToolTip(tooltip.isEmpty() ? text : tooltip);
}

const ModEntry *MainWindow::entryById(quint64 uid) const
{
    return m_model ? m_model->entryById(uid) : nullptr;
}

QString MainWindow::platformSkipReason(Platform platform) const
{
    if (platform == Platform::CurseForge) {
        if (m_jobConfig.curseForgeApiKey.trimmed().isEmpty())
            return QStringLiteral("未提供 CurseForge API Key，已跳过");
        const auto result =
            m_connectionResults.value(platformToInt(Platform::CurseForge));
        if (!result.second.isEmpty())
            return QStringLiteral("CurseForge 不可用：%1").arg(result.second);
        return QStringLiteral("CurseForge 未启用，已跳过");
    }
    const auto result = m_connectionResults.value(platformToInt(Platform::Modrinth));
    if (!result.second.isEmpty())
        return QStringLiteral("Modrinth 不可用：%1").arg(result.second);
    return QStringLiteral("Modrinth 未启用，已跳过");
}

void MainWindow::showRowMenu(const QModelIndex &index, const QPoint &globalPosition)
{
    if (m_running || m_singleRetryActive || !index.isValid())
        return;
    const ModEntry *entry = m_model->entryAt(index.row());
    if (!entry)
        return;
    m_table->selectRow(index.row());

    QMenu menu(this);
    for (int p = 0; p < kPlatformCount; ++p) {
        const Platform platform = platformFromInt(p);
        const PlatformResult &result = entry->results[p];
        QMenu *platformMenu = menu.addMenu(platformName(platform));

        if (result.isSuccess()) {
            QAction *copy = platformMenu->addAction(QStringLiteral("复制"));
            connect(copy, &QAction::triggered, this,
                    [this, uid = entry->uid, platform]() { copyPlatformResult(uid, platform); });

            QAction *open = platformMenu->addAction(QStringLiteral("打开直链"));
            connect(open, &QAction::triggered, this,
                    [this, uid = entry->uid, platform]() { openPlatformUrl(uid, platform); });

            QAction *copyLink = platformMenu->addAction(QStringLiteral("复制直链"));
            connect(copyLink, &QAction::triggered, this,
                    [this, uid = entry->uid, platform]() {
                        const ModEntry *current = entryById(uid);
                        if (!current)
                            return;
                        const QString url = current->result(platform).url;
                        if (!url.isEmpty()) {
                            QGuiApplication::clipboard()->setText(url);
                            statusBar()->showMessage(QStringLiteral("已复制直链"), 3000);
                        }
                    });
        }

        if (result.state != SearchState::NotSearched) {
            QAction *remove = platformMenu->addAction(QStringLiteral("移除结果"));
            connect(remove, &QAction::triggered, this,
                    [this, uid = entry->uid, platform]() { clearPlatformResult(uid, platform); });
        }

        if (result.isSuccess()) {
            QAction *details = platformMenu->addAction(QStringLiteral("详细检索原始信息"));
            connect(details, &QAction::triggered, this,
                    [this, uid = entry->uid, platform]() { showPlatformDetails(uid, platform); });
        } else if (result.state != SearchState::NotSearched) {
            QAction *error = platformMenu->addAction(QStringLiteral("错误详情"));
            connect(error, &QAction::triggered, this,
                    [this, uid = entry->uid, platform]() { showPlatformDetails(uid, platform); });
        }

        QAction *retry = platformMenu->addAction(QStringLiteral("重新检索"));
        connect(retry, &QAction::triggered, this,
                [this, uid = entry->uid, platform]() { retryRow(uid, platform); });

        QAction *manual = platformMenu->addAction(QStringLiteral("手动检索…"));
        connect(manual, &QAction::triggered, this,
                [this, uid = entry->uid, platform]() { manualSearchForRow(uid, platform); });
    }

    menu.addSeparator();
    QAction *deleteAction = menu.addAction(QStringLiteral("删除整行"));
    connect(deleteAction, &QAction::triggered, this,
            [this, uid = entry->uid]() { deleteRow(uid); });
    menu.exec(globalPosition);
}

void MainWindow::copyPlatformResult(quint64 uid, Platform platform)
{
    const ModEntry *entry = entryById(uid);
    if (!entry)
        return;
    const PlatformResult &result = entry->result(platform);
    if (!result.isSuccess())
        return;
    const QString text = QStringLiteral("名称\tID\t作者\t许可证\t直链\n%1\t%2\t%3\t%4\t%5")
                             .arg(result.name, result.id, result.author,
                                  result.displayLicense(), result.url);
    QGuiApplication::clipboard()->setText(text);
    statusBar()->showMessage(QStringLiteral("已复制 %1 结果摘要").arg(platformName(platform)),
                             3000);
}

void MainWindow::openPlatformUrl(quint64 uid, Platform platform)
{
    const ModEntry *entry = entryById(uid);
    if (!entry)
        return;
    const QString url = entry->result(platform).url;
    if (!url.isEmpty()) {
        UrlLauncher::openInteractive(this, QUrl(url),
                                     QStringLiteral("%1 直链").arg(platformName(platform)));
    }
}

void MainWindow::showPlatformDetails(quint64 uid, Platform platform)
{
    const ModEntry *entry = entryById(uid);
    if (!entry)
        return;
    DetailsDialog::showPlatformDetails(this, *entry, platform);
}

void MainWindow::clearPlatformResult(quint64 uid, Platform platform)
{
    if (m_running || m_singleRetryActive)
        return;
    m_model->clearPlatformResult(uid, platform);
    setStatusLabel(QStringLiteral("已移除 %1 结果（未删除整行）。")
                       .arg(platformName(platform)));
}

void MainWindow::retryRow(quint64 uid, Platform platform)
{
    if (m_running) {
        QMessageBox::information(this, QStringLiteral("检索进行中"),
                                 QStringLiteral("请先等待当前检索完成或点击“取消”。"));
        return;
    }
    if (m_singleRetryActive) {
        QMessageBox::information(this, QStringLiteral("请稍候"),
                                 QStringLiteral("已有单行重新检索正在进行，请稍候。"));
        return;
    }
    const ModEntry *entry = entryById(uid);
    if (!entry)
        return;
    if (platform == Platform::CurseForge && m_config.curseForgeApiKey.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("缺少 API Key"),
                             QStringLiteral("未填写 CurseForge API Key，无法重新检索 CurseForge。"));
        return;
    }
    const QString fileName = entry->originalFileName;
    const QString query = entry->searchName;
    if (query.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("名称为空"),
                             QStringLiteral("该行的检索用名称为空。"));
        return;
    }

    AppConfig config = m_config;
    config.curseForgeApiKey = m_keyEdit->text().trimmed();
    config.sanitize();

    auto *manager = new NetworkTaskManager(this);
    manager->configure(config);
    m_retryManager = manager;
    m_singleRetryActive = true;
    m_model->setPlatformState(uid, platform, SearchState::Searching, 0, QString());
    updateUiState();
    setStatusLabel(QStringLiteral("重新检索：%1 · %2").arg(entry->originalFileName,
                                                           platformName(platform)));

    const QString tag = QStringLiteral("retry-%1-%2-%3")
                            .arg(uid)
                            .arg(platformToInt(platform))
                            .arg(QDateTime::currentMSecsSinceEpoch());
    connect(manager, &NetworkTaskManager::searchStarted, this,
            [this, platform, fileName](const QString &, int, const QString &, int attempt,
                                       int maxAttempts) {
                setStatusLabel(QStringLiteral("重新检索：%1 · %2（尝试 %3/%4）")
                                   .arg(fileName, platformName(platform))
                                   .arg(attempt)
                                   .arg(maxAttempts));
            });
    connect(manager, &NetworkTaskManager::searchRetry, this,
            [this, platform, fileName](const QString &, int, const QString &, int nextAttempt,
                                       int maxAttempts, int delayMs, const QString &reason) {
                setStatusLabel(QStringLiteral("重新检索重试 %1/%2：%3 · %4 — %5（等待 %6 ms）")
                                   .arg(nextAttempt)
                                   .arg(maxAttempts)
                                   .arg(fileName, platformName(platform), reason)
                                   .arg(delayMs),
                               reason);
            });
    connect(manager, &NetworkTaskManager::searchFinished, this,
            [this, manager, uid, platform](const QString &, int,
                                           const PlatformResult &result) {
                m_model->setPlatformResult(uid, platform, result);
                if (m_retryManager == manager)
                    m_retryManager = nullptr;
                m_singleRetryActive = false;
                manager->deleteLater();
                updateUiState();
                QString status = QStringLiteral("重新检索完成：%1 · %2")
                                     .arg(platformName(platform), stateText(result.state));
                if (!result.error.isEmpty())
                    status += QStringLiteral(" — %1").arg(shortError(result.error));
                setStatusLabel(status, result.error);
            });

    manager->submitSearch(platform, query, tag);
}

void MainWindow::manualSearchForRow(quint64 uid, Platform platform)
{
    if (m_running || m_singleRetryActive) {
        QMessageBox::information(this, QStringLiteral("检索进行中"),
                                 QStringLiteral("请先等待当前检索完成或点击“取消”。"));
        return;
    }
    const ModEntry *entry = entryById(uid);
    if (!entry)
        return;
    if (platform == Platform::CurseForge && m_config.curseForgeApiKey.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("缺少 API Key"),
                             QStringLiteral("未填写 CurseForge API Key，无法手动检索 CurseForge。"));
        return;
    }

    AppConfig config = m_config;
    config.curseForgeApiKey = m_keyEdit->text().trimmed();
    ManualSearchDialog dialog(config, m_model, entry->searchName, uid,
                              platformToInt(platform), this);
    if (dialog.exec() == QDialog::Accepted) {
        setStatusLabel(QStringLiteral("已通过手动检索更新原行。"));
        updateUiState();
    }
}

void MainWindow::deleteRow(quint64 uid)
{
    if (m_running || m_singleRetryActive)
        return;
    const ModEntry *entry = entryById(uid);
    if (!entry)
        return;
    const auto answer = QMessageBox::question(
        this, QStringLiteral("删除整行"),
        QStringLiteral("确定要删除“%1”这一整行吗？\n该操作不可撤销。")
            .arg(entry->originalFileName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    m_model->removeRowById(uid);
    setStatusLabel(QStringLiteral("已删除整行。"));
}

void MainWindow::selectRowById(quint64 uid)
{
    const int row = m_model->rowIndexById(uid);
    if (row < 0)
        return;
    m_table->selectRow(row);
    m_table->scrollTo(m_model->index(row, 0), QAbstractItemView::PositionAtCenter);
    m_table->setFocus();
    activateWindow();
}

void MainWindow::openStandaloneSearch()
{
    if (m_running) {
        QMessageBox::information(this, QStringLiteral("检索进行中"),
                                 QStringLiteral("主检索进行中，请先取消或等待完成。"));
        return;
    }
    if (m_singleRetryActive) {
        QMessageBox::information(this, QStringLiteral("请稍候"),
                                 QStringLiteral("单行重新检索正在进行，请稍候。"));
        return;
    }
    AppConfig config = m_config;
    config.curseForgeApiKey = m_keyEdit->text().trimmed();
    ManualSearchDialog dialog(config, m_model, QString(), 0, -1, this);
    if (dialog.exec() == QDialog::Accepted) {
        setStatusLabel(QStringLiteral("单独检索结果已添加到表格。"));
        updateUiState();
    }
}

void MainWindow::openSummary()
{
    if (m_summaryDialog) {
        m_summaryDialog->show();
        m_summaryDialog->raise();
        m_summaryDialog->activateWindow();
        m_summaryDialog->refresh();
        return;
    }

    auto *dialog = new SummaryDialog(m_model, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_summaryDialog = dialog;
    connect(dialog, &SummaryDialog::retryRequested, this,
            [this](quint64 uid, int platform) {
                retryRow(uid, platformFromInt(platform));
            });
    connect(dialog, &SummaryDialog::manualSearchRequested, this,
            [this](quint64 uid, int platform) {
                manualSearchForRow(uid, platformFromInt(platform));
            });
    connect(dialog, &SummaryDialog::rowActivated, this, &MainWindow::selectRowById);
    dialog->show();
}

void MainWindow::openSettings()
{
    m_config.curseForgeApiKey = m_keyEdit->text().trimmed();
    SettingsDialog dialog(m_config, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    AppConfig updated = dialog.config();
    updated.curseForgeApiKey = m_keyEdit->text().trimmed();
    m_config = updated;
    persistSettings();
    setStatusLabel(QStringLiteral("设置已保存。变更只影响下一次检索。"));
    updateUiState();
}

void MainWindow::validateCurseForgeKey()
{
    if (m_running)
        return;
    const QString key = m_keyEdit->text().trimmed();
    if (key.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("API Key 为空"),
                                 QStringLiteral("请先填写 CurseForge API Key。"));
        m_keyEdit->setFocus();
        return;
    }

    m_config.curseForgeApiKey = key;
    AppConfig config = m_config;
    config.sanitize();
    auto *checker = new CurseForgeClient(this);
    checker->configure(config);
    m_validateButton->setEnabled(false);
    setStatusLabel(QStringLiteral("正在验证 CurseForge API Key 并检查连接…"));

    connect(checker, &BasePlatformClient::connectionChecked, this,
            [this, checker](int, bool ok, const QString &message, int) {
                checker->deleteLater();
                m_validateButton->setEnabled(!m_running);
                setStatusLabel(QStringLiteral("CurseForge 验证结果：%1").arg(message));
                if (ok) {
                    QMessageBox::information(this, QStringLiteral("验证成功"), message);
                } else {
                    QMessageBox::warning(this, QStringLiteral("验证失败"), message);
                }
            });
    checker->checkConnection();
}

void MainWindow::exportResults(ExportFormat format)
{
    if (m_running || m_singleRetryActive) {
        QMessageBox::information(this, QStringLiteral("请稍候"),
                                 QStringLiteral("检索进行中，完成后才能导出。"));
        return;
    }
    if (m_model->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("没有数据"),
                                 QStringLiteral("表格中没有可导出的结果。"));
        return;
    }

    QString directory = m_settings->value(QStringLiteral("paths/lastExportDir")).toString();
    const QString documentsDirectory =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (directory.isEmpty() || !QDir(directory).exists()
        || !QFileInfo(directory).isWritable()
        || (!documentsDirectory.isEmpty()
            && QFileInfo(directory).absoluteFilePath()
                == QFileInfo(documentsDirectory).absoluteFilePath())) {
        directory = defaultExportDirectory();
    }

    const QString defaultName = defaultExportFileName(format);
    const QString suffix = ExportService::defaultSuffix(format);
    QFileDialog dialog(this, QStringLiteral("导出检索结果"),
                       QDir(directory).filePath(defaultName),
                       ExportService::fileFilter(format));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDefaultSuffix(suffix);
    // 使用 Qt 自带保存对话框，避免 Windows 原生外壳对 Documents 等目录的误判。
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".") + suffix;

    writeExportFile(path, format, true);
}

void MainWindow::exportQuick(ExportFormat format)
{
    if (m_running || m_singleRetryActive) {
        QMessageBox::information(this, QStringLiteral("请稍候"),
                                 QStringLiteral("检索进行中，完成后才能导出。"));
        return;
    }
    if (m_model->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("没有数据"),
                                 QStringLiteral("表格中没有可导出的结果。"));
        return;
    }

    const QString directory = defaultExportDirectory();
    const QString path = QDir(directory).filePath(defaultExportFileName(format));
    writeExportFile(path, format, false);
}

QString MainWindow::defaultExportDirectory() const
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QStringList candidates;
    // 应用数据目录与 settings.ini / 日志同属一个已确认可写的位置，优先使用。
    if (!appData.isEmpty())
        candidates.append(QDir(appData).filePath(QStringLiteral("exports")));
    if (!home.isEmpty())
        candidates.append(QDir(home).filePath(QStringLiteral("MinecraftModScannerExports")));
    candidates.append(QDir(QCoreApplication::applicationDirPath())
                          .filePath(QStringLiteral("exports")));
    candidates.append(QDir(QDir::tempPath()).filePath(
        QStringLiteral("MinecraftModScannerExports")));

    for (const QString &candidate : candidates) {
        QDir().mkpath(candidate);
        const QFileInfo info(candidate);
        if (info.isDir() && info.isWritable())
            return info.absoluteFilePath();
    }
    return QDir::homePath();
}

QString MainWindow::defaultExportFileName(ExportFormat format) const
{
    return QStringLiteral("mod-search-%1.%2")
        .arg(QLocale::c().toString(QDateTime::currentDateTime(),
                                   QStringLiteral("yyyyMMdd-HHmmss")),
             ExportService::defaultSuffix(format));
}

bool MainWindow::writeExportFile(const QString &path, ExportFormat format,
                                 bool offerFallback)
{
    QString error;
    if (!ExportService::exportToFile(path, format, m_model->exportHeaders(),
                                     m_model->exportRows(), &error)) {
        if (offerFallback) {
            QMessageBox failure(this);
            failure.setIcon(QMessageBox::Warning);
            failure.setWindowTitle(QStringLiteral("导出失败"));
            failure.setText(QStringLiteral("无法写入所选位置。"));
            failure.setInformativeText(
                QStringLiteral("目标：\n%1\n\n错误：%2\n\n是否改存到默认导出目录？\n%3")
                    .arg(QDir::toNativeSeparators(path), error,
                         QDir::toNativeSeparators(defaultExportDirectory())));
            failure.setDetailedText(error);
            QPushButton *fallbackButton =
                failure.addButton(QStringLiteral("改存到默认目录"),
                                  QMessageBox::AcceptRole);
            failure.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
            failure.exec();
            if (failure.clickedButton() == fallbackButton) {
                const QString fallbackDir = defaultExportDirectory();
                QString fallbackName = QFileInfo(path).fileName();
                if (fallbackName.isEmpty())
                    fallbackName = defaultExportFileName(format);
                QString fallbackPath = QDir(fallbackDir).filePath(fallbackName);
                if (QFileInfo(fallbackPath).absoluteFilePath()
                    == QFileInfo(path).absoluteFilePath()) {
                    fallbackPath =
                        QDir(fallbackDir).filePath(defaultExportFileName(format));
                }
                return writeExportFile(fallbackPath, format, false);
            }
            return false;
        }

        QMessageBox::critical(
            this, QStringLiteral("导出失败"),
            QStringLiteral("无法写入文件：\n%1\n\n%2")
                .arg(QDir::toNativeSeparators(path), error));
        return false;
    }

    m_settings->setValue(QStringLiteral("paths/lastExportDir"),
                         QFileInfo(path).absolutePath());
    m_settings->sync();
    setStatusLabel(QStringLiteral("已导出 %1：%2")
                       .arg(ExportService::displayName(format),
                            QDir::toNativeSeparators(path)));

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("导出成功"));
    box.setText(QStringLiteral("已导出到：\n%1")
                    .arg(QDir::toNativeSeparators(path)));
    QPushButton *openFolder =
        box.addButton(QStringLiteral("打开导出文件夹"), QMessageBox::ActionRole);
    box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == openFolder) {
        UrlLauncher::openInteractive(this,
                                     QUrl::fromLocalFile(QFileInfo(path).absolutePath()),
                                     QStringLiteral("导出文件夹"));
    }
    return true;
}

void MainWindow::exportDefaultFormat()
{
    exportResults(ExportService::formatFromString(m_config.defaultExportFormat));
}

void MainWindow::showApiKeyHelp()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("获取 CurseForge API Key"));
    box.setTextFormat(Qt::PlainText);
    box.setText(QStringLiteral(
        "1. 打开 CurseForge 开发者控制台：https://console.curseforge.com/\n"
        "2. 创建 API Key。\n"
        "3. 将 Key 填入主界面的密码输入框。\n"
        "4. 点击“验证 Key / 检查连接”确认有效。\n\n"
        "不填写 Key 时只检索 Modrinth；单独检索与 MC百科功能不受影响。"));
    QPushButton *openButton =
        box.addButton(QStringLiteral("打开开发者控制台"), QMessageBox::ActionRole);
    box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == openButton) {
        UrlLauncher::openInteractive(this, QUrl(QStringLiteral("https://console.curseforge.com/")),
                                     QStringLiteral("CurseForge 开发者控制台"));
    }
}

void MainWindow::showAbout()
{
    const QString text =
        QStringLiteral("<b>Minecraft 模组自动检索</b> v%1<br><br>"
                       "made by DeepSeek-V4.1-Flash(1017)<br>"
                       "许可证：CC0 1.0 Universal（公共领域贡献）<br><br>"
                       "C++17 / Qt %2 / CMake<br>"
                       "数据来源：Modrinth API、CurseForge API、mcmod-api（非官方）。<br><br>"
                       "日志：%3<br><br>"
                       "API Key 与 User-Agent 不会写入日志。")
            .arg(QStringLiteral(MMS_VERSION), QString::fromLatin1(qVersion()),
                 AppLog::logFilePath().toHtmlEscaped());
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("关于"));
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    box.exec();
}

void MainWindow::showLicense()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("CC0 1.0 许可证"));
    box.setTextFormat(Qt::PlainText);
    box.setText(QStringLiteral(
        "Minecraft Mod Scanner 源代码采用 CC0 1.0 Universal 许可。\n\n"
        "在法律允许的最大范围内，作者 DeepSeek-V4.1-Flash(1017) 放弃该作品的"
        "全部版权及相关权利。你可以复制、修改、分发和商业使用，无需申请授权。\n\n"
        "完整法律文本：https://creativecommons.org/publicdomain/zero/1.0/legalcode\n\n"
        "该许可仅适用于本项目的原创源代码。Qt、Modrinth、CurseForge、MC百科、"
        "mcmod-api 及其他第三方组件、数据和商标仍遵循各自许可。"));
    QPushButton *openButton =
        box.addButton(QStringLiteral("打开 CC0 许可页面"), QMessageBox::ActionRole);
    box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == openButton) {
        UrlLauncher::openInteractive(
            this, QUrl(QStringLiteral("https://creativecommons.org/publicdomain/zero/1.0/")),
            QStringLiteral("CC0 1.0 许可"));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qInfo() << "MainWindow closeEvent";
    if (m_running) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("检索进行中"),
            QStringLiteral("检索仍在进行，退出将取消任务。确定退出吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_searchManager->cancelAll();
        if (m_wikiClient)
            m_wikiClient->cancelAll();
    }
    if (m_retryManager)
        m_retryManager->cancelAll();

    persistSettings();
    if (m_settings) {
        m_settings->setValue(QStringLiteral("window/geometry"), saveGeometry());
        m_settings->setValue(QStringLiteral("window/state"), saveState());
        m_settings->sync();
    }
    event->accept();
}
