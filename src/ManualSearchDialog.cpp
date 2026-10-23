#include "ManualSearchDialog.h"

#include "NetworkTaskManager.h"
#include "ResultModel.h"
#include "UrlLauncher.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ManualSearchDialog::ManualSearchDialog(const AppConfig &config, ResultModel *model,
                                       const QString &prefill, quint64 updateRowId,
                                       int fixedPlatform, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
    , m_model(model)
    , m_updateRowId(updateRowId)
    , m_fixedPlatform(fixedPlatform)
{
    setWindowTitle(updateRowId == 0 ? QStringLiteral("单独检索")
                                     : QStringLiteral("手动检索并更新原行"));
    setModal(true);
    resize(980, 620);

    m_manager = new NetworkTaskManager(this);
    m_manager->configure(m_config);

    buildUi(prefill);

    connect(m_manager, &NetworkTaskManager::searchStarted, this,
            &ManualSearchDialog::onSearchStarted);
    connect(m_manager, &NetworkTaskManager::searchRetry, this,
            &ManualSearchDialog::onSearchRetry);
    connect(m_manager, &NetworkTaskManager::searchFinished, this,
            &ManualSearchDialog::onSearchFinished);
}

ManualSearchDialog::~ManualSearchDialog()
{
    if (m_manager)
        m_manager->cancelAll();
}

void ManualSearchDialog::buildUi(const QString &prefill)
{
    auto *root = new QVBoxLayout(this);

    auto *formRow = new QHBoxLayout;
    formRow->addWidget(new QLabel(QStringLiteral("检索名称："), this));
    m_nameEdit = new QLineEdit(prefill, this);
    m_nameEdit->setPlaceholderText(QStringLiteral("原样使用，不做任何清洗"));
    formRow->addWidget(m_nameEdit, 1);
    root->addLayout(formRow);

    auto *platformRow = new QHBoxLayout;
    platformRow->addWidget(new QLabel(QStringLiteral("平台："), this));
    m_modrinthCheck = new QCheckBox(QStringLiteral("Modrinth"), this);
    m_curseForgeCheck = new QCheckBox(QStringLiteral("CurseForge"), this);
    m_modrinthCheck->setChecked(m_fixedPlatform < 0 || m_fixedPlatform == 0);
    m_curseForgeCheck->setChecked(m_fixedPlatform < 0 || m_fixedPlatform == 1);
    if (m_fixedPlatform == 0) {
        m_curseForgeCheck->setChecked(false);
        m_curseForgeCheck->setEnabled(false);
    } else if (m_fixedPlatform == 1) {
        m_modrinthCheck->setChecked(false);
        m_modrinthCheck->setEnabled(false);
    }
    if (m_config.curseForgeApiKey.trimmed().isEmpty()) {
        m_curseForgeCheck->setChecked(false);
        m_curseForgeCheck->setEnabled(false);
        m_curseForgeCheck->setToolTip(
            QStringLiteral("未填写 CurseForge API Key，无法检索 CurseForge。"));
    }
    platformRow->addWidget(m_modrinthCheck);
    platformRow->addWidget(m_curseForgeCheck);
    platformRow->addStretch(1);

    m_searchButton = new QPushButton(QStringLiteral("检索"), this);
    m_searchButton->setDefault(true);
    platformRow->addWidget(m_searchButton);
    root->addLayout(platformRow);

    m_progress = new QProgressBar(this);
    m_progress->setLocale(QLocale::c());
    QFont progressFont = QApplication::font();
    progressFont.setWeight(QFont::Normal);
    m_progress->setFont(progressFont);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setFormat(QStringLiteral("就绪"));
    root->addWidget(m_progress);

    m_statusLabel = new QLabel(QStringLiteral("输入名称后点击“检索”。结果会列出候选条目。"), this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_statusLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("平台"), QStringLiteral("名称"), QStringLiteral("ID"),
         QStringLiteral("作者"), QStringLiteral("许可证"), QStringLiteral("直链")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setColumnWidth(1, 300);
    root->addWidget(m_table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("关闭"));
    m_addButton = buttons->addButton(
        m_updateRowId == 0 ? QStringLiteral("添加到表格") : QStringLiteral("更新原行"),
        QDialogButtonBox::AcceptRole);
    m_addButton->setEnabled(false);
    root->addWidget(buttons);

    connect(m_searchButton, &QPushButton::clicked, this, &ManualSearchDialog::startSearch);
    connect(m_addButton, &QPushButton::clicked, this, &ManualSearchDialog::acceptSelection);
    connect(buttons, &QDialogButtonBox::rejected, this, &ManualSearchDialog::reject);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_addButton->setEnabled(!m_busy && !m_table->selectionModel()->selectedRows().isEmpty());
    });
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &ManualSearchDialog::startSearch);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (column != 5)
            return;
        const QTableWidgetItem *item = m_table->item(row, column);
        if (!item || item->text().isEmpty())
            return;
        UrlLauncher::openInteractive(this, QUrl(item->text()), QStringLiteral("模组直链"));
    });
}

void ManualSearchDialog::appendStatus(const QString &text)
{
    m_statusLabel->setText(text);
}

void ManualSearchDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_searchButton->setEnabled(!busy);
    m_nameEdit->setEnabled(!busy);
    m_modrinthCheck->setEnabled(!busy && (m_fixedPlatform < 0 || m_fixedPlatform == 0));
    const bool cfAllowed = !m_config.curseForgeApiKey.trimmed().isEmpty();
    m_curseForgeCheck->setEnabled(!busy && cfAllowed
                                  && (m_fixedPlatform < 0 || m_fixedPlatform == 1));
    m_addButton->setEnabled(!busy && !m_table->selectionModel()->selectedRows().isEmpty());
    if (busy) {
        m_progress->setRange(0, 0);
        m_progress->setFormat(QStringLiteral("检索中…"));
    } else {
        m_progress->setRange(0, 1);
        m_progress->setValue(1);
        m_progress->setFormat(QStringLiteral("完成"));
    }
}

void ManualSearchDialog::startSearch()
{
    if (m_busy)
        return;
    const QString name = m_nameEdit->text();
    if (name.trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("缺少名称"),
                                 QStringLiteral("请输入要检索的名称。"));
        m_nameEdit->setFocus();
        return;
    }

    QVector<Platform> platforms;
    if (m_modrinthCheck->isChecked())
        platforms.append(Platform::Modrinth);
    if (m_curseForgeCheck->isChecked())
        platforms.append(Platform::CurseForge);
    if (platforms.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("选择平台"),
                                 QStringLiteral("请至少选择一个检索平台。"));
        return;
    }

    m_manager->cancelAll();
    m_manager->configure(m_config);
    m_pendingPlatforms.clear();
    m_resultRows.clear();
    m_table->setRowCount(0);
    m_addButton->setEnabled(false);
    setBusy(true);
    appendStatus(QStringLiteral("正在检索“%1”…").arg(name));

    for (Platform platform : platforms) {
        m_pendingPlatforms.insert(platformToInt(platform));
        const QString tag = QStringLiteral("manual-%1-%2")
                                .arg(platformToInt(platform))
                                .arg(reinterpret_cast<quintptr>(this));
        m_manager->submitSearch(platform, name, tag);
    }
}

void ManualSearchDialog::onSearchStarted(const QString &tag, int platform,
                                          const QString &query, int attempt,
                                          int maxAttempts)
{
    Q_UNUSED(tag);
    Q_UNUSED(query);
    appendStatus(QStringLiteral("%1：检索中（尝试 %2/%3）")
                     .arg(platformName(platformFromInt(platform)))
                     .arg(attempt)
                     .arg(maxAttempts));
}

void ManualSearchDialog::onSearchRetry(const QString &tag, int platform, const QString &query,
                                       int nextAttempt, int maxAttempts, int delayMs,
                                       const QString &reason)
{
    Q_UNUSED(tag);
    Q_UNUSED(query);
    appendStatus(QStringLiteral("%1：%2；重试 %3/%4（等待 %5 ms）")
                     .arg(platformName(platformFromInt(platform)), reason)
                     .arg(nextAttempt)
                     .arg(maxAttempts)
                     .arg(delayMs));
}

void ManualSearchDialog::onSearchFinished(const QString &tag, int platform,
                                          const PlatformResult &result)
{
    Q_UNUSED(tag);
    m_pendingPlatforms.remove(platform);

    if (result.isSuccess()) {
        for (const SearchCandidate &candidate : result.candidates) {
            ResultRow row;
            row.platform = platformFromInt(platform);
            row.candidate = candidate;
            m_resultRows.append(row);

            const int tableRow = m_table->rowCount();
            m_table->insertRow(tableRow);
            auto *platformItem =
                new QTableWidgetItem(platformName(row.platform));
            platformItem->setData(Qt::UserRole, m_resultRows.size() - 1);
            m_table->setItem(tableRow, 0, platformItem);
            m_table->setItem(tableRow, 1, new QTableWidgetItem(candidate.name));
            m_table->setItem(tableRow, 2, new QTableWidgetItem(candidate.id));
            m_table->setItem(tableRow, 3, new QTableWidgetItem(candidate.author));
            m_table->setItem(tableRow, 4, new QTableWidgetItem(
                                               candidate.license.isEmpty()
                                                   ? QStringLiteral("未知")
                                                   : candidate.license));
            auto *urlItem = new QTableWidgetItem(candidate.url);
            urlItem->setToolTip(candidate.url + QStringLiteral("\n（双击打开浏览器）"));
            m_table->setItem(tableRow, 5, urlItem);
        }
        appendStatus(QStringLiteral("%1：成功，找到 %2 个候选")
                         .arg(platformName(platformFromInt(platform)))
                         .arg(result.candidates.size()));
    } else {
        appendStatus(QStringLiteral("%1：%2%3")
                         .arg(platformName(platformFromInt(platform)),
                              stateText(result.state),
                              result.error.isEmpty()
                                  ? QString()
                                  : QStringLiteral(" — %1").arg(result.error)));
    }

    m_table->clearSelection();
    finishSearchIfDone();
}

void ManualSearchDialog::finishSearchIfDone()
{
    if (!m_pendingPlatforms.isEmpty())
        return;
    setBusy(false);
    m_addButton->setEnabled(!m_table->selectionModel()->selectedRows().isEmpty());
    if (m_resultRows.isEmpty()) {
        m_progress->setFormat(QStringLiteral("未找到结果"));
        appendStatus(m_statusLabel->text() + QStringLiteral("\n未获得可添加的结果，可修改名称后重试。"));
    } else {
        m_progress->setFormat(QStringLiteral("共 %1 个候选").arg(m_resultRows.size()));
        appendStatus(m_statusLabel->text()
                     + QStringLiteral("\n可多选结果，然后点击“%1”。")
                           .arg(m_updateRowId == 0 ? QStringLiteral("添加到表格")
                                                   : QStringLiteral("更新原行")));
    }
}

QVector<ManualSearchDialog::ResultRow> ManualSearchDialog::selectedResults() const
{
    QVector<ResultRow> selected;
    QSet<int> seenRows;
    const auto indexes = m_table->selectionModel()->selectedRows();
    for (const QModelIndex &index : indexes) {
        if (seenRows.contains(index.row()))
            continue;
        seenRows.insert(index.row());
        const QTableWidgetItem *item = m_table->item(index.row(), 0);
        if (!item)
            continue;
        const int resultIndex = item->data(Qt::UserRole).toInt();
        if (resultIndex >= 0 && resultIndex < m_resultRows.size())
            selected.append(m_resultRows.at(resultIndex));
    }
    return selected;
}

void ManualSearchDialog::acceptSelection()
{
    if (m_busy)
        return;
    const QVector<ResultRow> selected = selectedResults();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("未选择结果"),
                                 QStringLiteral("请先在结果表格中选择要使用的结果。"));
        return;
    }

    const QString name = m_nameEdit->text();

    if (m_updateRowId == 0) {
        bool duplicate = false;
        for (const ResultRow &row : selected) {
            if (m_model->containsPlatformId(row.platform, row.candidate.id)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("可能重复"),
                QStringLiteral("表格中已存在相同平台+相同 ID 的结果。仍要添加新行吗？"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
        }

        for (const ResultRow &row : selected) {
            ManualRowData data;
            data.searchName = name;
            PlatformResult result;
            result.state = SearchState::Success;
            result.name = row.candidate.name;
            result.id = row.candidate.id;
            result.author = row.candidate.author;
            result.license = row.candidate.license;
            result.url = row.candidate.url;
            result.attempts = 1;
            result.timestamp = QDateTime::currentDateTime();
            if (row.platform == Platform::Modrinth)
                data.modrinth = result;
            else
                data.curseforge = result;
            m_model->addManualRow(data);
        }
        accept();
        return;
    }

    const ModEntry *entry = m_model->entryById(m_updateRowId);
    if (!entry) {
        QMessageBox::warning(this, QStringLiteral("行已不存在"),
                             QStringLiteral("原表格行已被删除，无法更新。"));
        reject();
        return;
    }

    QSet<int> updatedPlatforms;
    for (const ResultRow &row : selected) {
        const int platformIndex = platformToInt(row.platform);
        if (updatedPlatforms.contains(platformIndex))
            continue;
        updatedPlatforms.insert(platformIndex);

        PlatformResult result;
        result.state = SearchState::Success;
        result.name = row.candidate.name;
        result.id = row.candidate.id;
        result.author = row.candidate.author;
        result.license = row.candidate.license;
        result.url = row.candidate.url;
        result.attempts = 1;
        result.timestamp = QDateTime::currentDateTime();
        m_model->setPlatformResult(m_updateRowId, row.platform, result);
    }

    QString note = m_model->entryById(m_updateRowId)->rowNote;
    if (!note.isEmpty())
        note.append(QStringLiteral("；"));
    note.append(QStringLiteral("手动检索更新（%1）").arg(name));
    m_model->setRowNote(m_updateRowId, note);
    accept();
}

void ManualSearchDialog::reject()
{
    if (m_manager)
        m_manager->cancelAll();
    QDialog::reject();
}
